/* Linux backend: remoteproc sysfs for lifecycle, rpmsg_char for messages.
 *
 * Every path is built under dsp->root, so tests run it against a fake tree.
 * Cores are found by device-tree unit address: the device link of each
 * /sys/class/remoteproc entry resolves to the DSP's platform device, whatever
 * its remoteprocN number. */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "internal.h"

/* <linux/rpmsg.h>, spelled out so the library builds without kernel headers. */
struct sdos_rpmsg_endpoint_info {
    char name[32];
    uint32_t src;
    uint32_t dst;
};
#define SDOS_RPMSG_CREATE_EPT_IOCTL _IOW(0xb5, 0x1, struct sdos_rpmsg_endpoint_info)
#define SDOS_RPMSG_DESTROY_EPT_IOCTL _IO(0xb5, 0x2)
#define SDOS_RPMSG_ADDR_ANY 0xFFFFFFFFu

typedef struct linux_state {
    char rproc[SDOS_PATH_MAX]; /* sysfs directory of this core's remoteproc */
    char rproc_name[32];
    int fd;                    /* open endpoint, or -1 */
} linux_state;

static sdos_rpmsg_open_fn rpmsg_open;

static linux_state *state_of(sdos_dsp *dsp)
{
    return (linux_state *)dsp->backend;
}

static int join(char *out, size_t capacity, const char *root, const char *path)
{
    return sdos__format(out, capacity, "%s%s%s", root, root[strlen(root) - 1u] == '/' ? "" : "/", path);
}

static int read_text(const char *path, char *out, size_t capacity)
{
    FILE *file = fopen(path, "r");
    size_t n;
    if (file == NULL) {
        return -1;
    }
    n = fread(out, 1u, capacity - 1u, file);
    (void)fclose(file);
    out[n] = '\0';
    while (n > 0u && (out[n - 1u] == '\n' || out[n - 1u] == ' ')) {
        out[--n] = '\0';
    }
    return 0;
}

static int write_text(const char *path, const char *text)
{
    int fd = open(path, O_WRONLY | O_TRUNC);
    ssize_t n;
    if (fd < 0) {
        return -1;
    }
    n = write(fd, text, strlen(text));
    (void)close(fd);
    return n == (ssize_t)strlen(text) ? 0 : -1;
}

static const char *basename_of(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

/* Find /sys/class/remoteproc/remoteprocN whose device is this core. */
static int find_rproc(sdos_dsp *dsp)
{
    linux_state *st = state_of(dsp);
    char dir[SDOS_PATH_MAX];
    DIR *listing;
    struct dirent *entry;
    int found = -1;

    if (join(dir, sizeof dir, dsp->root, "sys/class/remoteproc") != 0 || (listing = opendir(dir)) == NULL) {
        return -1;
    }
    while (found != 0 && (entry = readdir(listing)) != NULL) {
        char link[SDOS_PATH_MAX + 64];
        char resolved[PATH_MAX];
        if (strncmp(entry->d_name, "remoteproc", 10) != 0) {
            continue;
        }
        (void)sdos__format(link, sizeof link, "%s/%s/device", dir, entry->d_name);
        if (realpath(link, resolved) != NULL && strcmp(basename_of(resolved), dsp->core->device) == 0) {
            if (sdos__format(st->rproc, sizeof st->rproc, "%s/%s", dir, entry->d_name) == 0 &&
                sdos__format(st->rproc_name, sizeof st->rproc_name, "%s", entry->d_name) == 0) {
                found = 0;
            }
        }
    }
    (void)closedir(listing);
    return found;
}

static sdos_dsp_state parse_state(const char *text)
{
    if (strcmp(text, "offline") == 0) {
        return SDOS_DSP_OFFLINE;
    }
    if (strcmp(text, "running") == 0) {
        return SDOS_DSP_RUNNING;
    }
    if (strcmp(text, "crashed") == 0) {
        return SDOS_DSP_CRASHED;
    }
    return SDOS_DSP_OTHER;
}

static sdos_status linux_query(sdos_dsp *dsp, sdos_dsp_info *info)
{
    linux_state *st = state_of(dsp);
    char path[SDOS_PATH_MAX + 16];
    char text[64];
    if (find_rproc(dsp) != 0) {
        info->state = SDOS_DSP_ABSENT;
        return SDOS_OK;
    }
    (void)sdos__format(info->remoteproc, sizeof info->remoteproc, "%s", st->rproc_name);
    (void)sdos__format(path, sizeof path, "%s/state", st->rproc);
    if (read_text(path, text, sizeof text) != 0) {
        return sdos__fail(dsp, SDOS_E_TRANSPORT, "cannot read %s: %s", path, strerror(errno));
    }
    info->state = parse_state(text);
    (void)sdos__format(path, sizeof path, "%s/firmware", st->rproc);
    if (read_text(path, info->firmware, sizeof info->firmware) != 0) {
        info->firmware[0] = '\0';
    }
    return SDOS_OK;
}

static int copy_file(const char *from, const char *to)
{
    char temporary[SDOS_PATH_MAX + 8];
    char chunk[65536];
    int in = open(from, O_RDONLY);
    int out;
    ssize_t n;
    int result = 0;
    if (in < 0) {
        return -1;
    }
    (void)sdos__format(temporary, sizeof temporary, "%s.part", to);
    out = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) {
        (void)close(in);
        return -1;
    }
    while ((n = read(in, chunk, sizeof chunk)) > 0) {
        if (write(out, chunk, (size_t)n) != n) {
            result = -1;
            break;
        }
    }
    if (n < 0 || fsync(out) != 0) {
        result = -1;
    }
    (void)close(in);
    (void)close(out);
    if (result == 0 && rename(temporary, to) != 0) {
        result = -1;
    }
    if (result != 0) {
        (void)unlink(temporary);
    }
    return result;
}

static sdos_status linux_install(sdos_dsp *dsp, const char *elf_path)
{
    linux_state *st = state_of(dsp);
    char dir[SDOS_PATH_MAX];
    char target[SDOS_PATH_MAX + 64];
    char name[96];
    char attribute[SDOS_PATH_MAX + 16];

    if (join(dir, sizeof dir, dsp->root, "lib/firmware/sdos") != 0) {
        return sdos__fail(dsp, SDOS_E_FIELD, "root path is too long");
    }
    (void)mkdir(dir, 0755);
    (void)sdos__format(name, sizeof name, "sdos/%s.%s", dsp->core->name, dsp->core->firmware_ext);
    (void)sdos__format(target, sizeof target, "%s/%s.%s", dir, dsp->core->name, dsp->core->firmware_ext);
    if (copy_file(elf_path, target) != 0) {
        return sdos__fail(dsp, SDOS_E_TRANSPORT, "cannot install %s as %s: %s", elf_path, target, strerror(errno));
    }
    (void)sdos__format(attribute, sizeof attribute, "%s/firmware", st->rproc);
    if (write_text(attribute, name) != 0) {
        return sdos__fail(dsp, SDOS_E_TRANSPORT, "cannot select firmware %s: %s", name, strerror(errno));
    }
    return SDOS_OK;
}

static void sleep_ms(unsigned ms)
{
    struct timespec pause = {0, (long)ms * 1000000L};
    (void)nanosleep(&pause, NULL);
}

static sdos_status linux_set_running(sdos_dsp *dsp, int running)
{
    linux_state *st = state_of(dsp);
    char path[SDOS_PATH_MAX + 16];
    char text[64];
    sdos_dsp_state wanted = running ? SDOS_DSP_RUNNING : SDOS_DSP_OFFLINE;
    uint64_t deadline = sdos__now_ms() + dsp->timeout_ms;

    (void)sdos__format(path, sizeof path, "%s/state", st->rproc);
    if (write_text(path, running ? "start" : "stop") != 0) {
        return sdos__fail(dsp, SDOS_E_TRANSPORT, "writing %s to %s failed: %s", running ? "start" : "stop", path,
                          strerror(errno));
    }
    do {
        if (read_text(path, text, sizeof text) == 0) {
            sdos_dsp_state now = parse_state(text);
            if (now == wanted) {
                return SDOS_OK;
            }
            if (running && now == SDOS_DSP_CRASHED) {
                return sdos__fail(dsp, SDOS_E_INTERNAL, "%s crashed while starting; see `dspctl trace %s`",
                                  dsp->core->name, dsp->core->name);
            }
        }
        sleep_ms(10u);
    } while (sdos__now_ms() < deadline);
    return sdos__fail(dsp, SDOS_E_TIMEOUT, "%s did not become %s within %u ms (state %s)", dsp->core->name,
                      sdos_dsp_state_name(wanted), (unsigned)dsp->timeout_ms, text);
}

/* /dev/rpmsg_ctrlN of the virtio device under this core, found through sysfs. */
static int find_ctrl(sdos_dsp *dsp, char *out, size_t capacity)
{
    char dir[SDOS_PATH_MAX];
    char needle[64];
    DIR *listing;
    struct dirent *entry;
    int found = -1;
    if (join(dir, sizeof dir, dsp->root, "sys/class/rpmsg") != 0 || (listing = opendir(dir)) == NULL) {
        return -1;
    }
    (void)sdos__format(needle, sizeof needle, "/%s/", dsp->core->device);
    while (found != 0 && (entry = readdir(listing)) != NULL) {
        char link[SDOS_PATH_MAX + 64];
        char resolved[PATH_MAX];
        if (strncmp(entry->d_name, "rpmsg_ctrl", 10) != 0) {
            continue;
        }
        (void)sdos__format(link, sizeof link, "%s/%s", dir, entry->d_name);
        if (realpath(link, resolved) != NULL && strstr(resolved, needle) != NULL) {
            char dev[SDOS_PATH_MAX];
            (void)sdos__format(dev, sizeof dev, "dev/%s", entry->d_name);
            found = join(out, capacity, dsp->root, dev);
        }
    }
    (void)closedir(listing);
    return found;
}

/* The kernel creates /dev/rpmsgN for the new endpoint; find it by its dst. */
static int default_rpmsg_open(sdos_dsp *dsp, const char *ctrl_path, uint32_t endpoint, char *why, size_t capacity)
{
    struct sdos_rpmsg_endpoint_info info;
    char dir[SDOS_PATH_MAX];
    char needle[64];
    int ctrl = open(ctrl_path, O_RDWR | O_CLOEXEC);
    int attempt;
    if (ctrl < 0) {
        (void)snprintf(why, capacity, "open %s: %s", ctrl_path, strerror(errno));
        return -1;
    }
    memset(&info, 0, sizeof info);
    (void)snprintf(info.name, sizeof info.name, "%s", SDOS_RPMSG_SERVICE);
    info.src = SDOS_RPMSG_ADDR_ANY;
    info.dst = endpoint;
    if (ioctl(ctrl, SDOS_RPMSG_CREATE_EPT_IOCTL, &info) != 0) {
        (void)snprintf(why, capacity, "RPMSG_CREATE_EPT_IOCTL on %s: %s", ctrl_path, strerror(errno));
        (void)close(ctrl);
        return -1;
    }
    (void)close(ctrl);
    (void)join(dir, sizeof dir, dsp->root, "sys/class/rpmsg");
    (void)sdos__format(needle, sizeof needle, "/%s/", dsp->core->device);
    for (attempt = 0; attempt < 100; attempt++) { /* udev creates the node asynchronously */
        DIR *listing = opendir(dir);
        struct dirent *entry;
        while (listing != NULL && (entry = readdir(listing)) != NULL) {
            char path[SDOS_PATH_MAX + 64];
            char resolved[PATH_MAX];
            char text[32];
            if (strncmp(entry->d_name, "rpmsg", 5) != 0 || strncmp(entry->d_name, "rpmsg_ctrl", 10) == 0) {
                continue;
            }
            (void)sdos__format(path, sizeof path, "%s/%s", dir, entry->d_name);
            if (realpath(path, resolved) == NULL || strstr(resolved, needle) == NULL) {
                continue;
            }
            (void)sdos__format(path, sizeof path, "%s/%s/dst", dir, entry->d_name);
            if (read_text(path, text, sizeof text) == 0 && strtoul(text, NULL, 0) == endpoint) {
                char dev[SDOS_PATH_MAX];
                char node[SDOS_PATH_MAX + 64];
                int fd;
                (void)sdos__format(dev, sizeof dev, "dev/%s", entry->d_name);
                (void)join(node, sizeof node, dsp->root, dev);
                fd = open(node, O_RDWR | O_CLOEXEC);
                if (fd >= 0) {
                    (void)closedir(listing);
                    return fd;
                }
            }
        }
        if (listing != NULL) {
            (void)closedir(listing);
        }
        sleep_ms(20u);
    }
    (void)snprintf(why, capacity, "no /dev/rpmsgN appeared for endpoint %u", (unsigned)endpoint);
    return -1;
}

static sdos_status linux_connect(sdos_dsp *dsp)
{
    linux_state *st = state_of(dsp);
    char ctrl[SDOS_PATH_MAX];
    char why[160] = "";
    uint64_t deadline = sdos__now_ms() + dsp->timeout_ms;
    while (find_ctrl(dsp, ctrl, sizeof ctrl) != 0) {
        if (sdos__now_ms() >= deadline) {
            return sdos__fail(dsp, SDOS_E_TRANSPORT,
                              "%s has no rpmsg_ctrl device; is it running firmware with an RPMsg vdev?",
                              dsp->core->name);
        }
        sleep_ms(20u);
    }
    st->fd = rpmsg_open(dsp, ctrl, SDOS_RPMSG_ENDPOINT, why, sizeof why);
    if (st->fd < 0) {
        return sdos__fail(dsp, SDOS_E_TRANSPORT, "%s: %s", dsp->core->name, why);
    }
    return SDOS_OK;
}

static void linux_disconnect(sdos_dsp *dsp)
{
    linux_state *st = state_of(dsp);
    if (st->fd >= 0) {
        (void)ioctl(st->fd, SDOS_RPMSG_DESTROY_EPT_IOCTL); /* fails harmlessly on a socket in tests */
        (void)close(st->fd);
        st->fd = -1;
    }
}

static sdos_status linux_send(sdos_dsp *dsp, const uint8_t *message, size_t length)
{
    ssize_t n = write(state_of(dsp)->fd, message, length);
    if (n != (ssize_t)length) {
        return sdos__fail(dsp, SDOS_E_TRANSPORT, "sending to %s failed: %s", dsp->core->name,
                          n < 0 ? strerror(errno) : "short write");
    }
    return SDOS_OK;
}

static sdos_status linux_receive(sdos_dsp *dsp, uint8_t *message, size_t capacity, size_t *length,
                                 uint32_t timeout_ms)
{
    struct pollfd wait = {state_of(dsp)->fd, POLLIN, 0};
    ssize_t n;
    int ready = poll(&wait, 1, (int)(timeout_ms > (uint32_t)INT_MAX ? INT_MAX : (int)timeout_ms));
    if (ready == 0) {
        return sdos__fail(dsp, SDOS_E_TIMEOUT, "%s did not answer within %u ms", dsp->core->name,
                          (unsigned)dsp->timeout_ms);
    }
    if (ready < 0 || (wait.revents & (POLLERR | POLLHUP)) != 0) {
        return sdos__fail(dsp, SDOS_E_TRANSPORT, "%s endpoint closed; the core may have stopped", dsp->core->name);
    }
    n = read(wait.fd, message, capacity);
    if (n <= 0) {
        return sdos__fail(dsp, SDOS_E_TRANSPORT, "reading from %s failed: %s", dsp->core->name,
                          n < 0 ? strerror(errno) : "end of file");
    }
    *length = (size_t)n;
    return SDOS_OK;
}

static sdos_status linux_map_region(sdos_dsp *dsp, uint8_t **base, uint32_t *size)
{
    (void)base;
    (void)size;
    return sdos__fail(dsp, SDOS_E_TRANSPORT,
                      "shared buffers on the board wait for the Task 027 data-path decision; "
                      "control messages work, and `--backend mock` runs buffers on the PC");
}

static void linux_sync(sdos_dsp *dsp, uint32_t offset, uint32_t length)
{
    (void)dsp;
    (void)offset;
    (void)length;
}

static sdos_status linux_trace(sdos_dsp *dsp, char *text, size_t capacity, size_t *length)
{
    linux_state *st = state_of(dsp);
    char relative[96];
    char path[SDOS_PATH_MAX + 96];
    FILE *file;
    if (find_rproc(dsp) != 0) {
        return sdos__fail(dsp, SDOS_E_TRANSPORT, "%s is not present", dsp->core->name);
    }
    (void)sdos__format(relative, sizeof relative, "sys/kernel/debug/remoteproc/%s/trace0", st->rproc_name);
    (void)join(path, sizeof path, dsp->root, relative);
    file = fopen(path, "r");
    if (file == NULL) {
        return sdos__fail(dsp, SDOS_E_TRANSPORT, "cannot read %s: %s (needs root, debugfs, and running firmware "
                          "with a trace buffer)", path, strerror(errno));
    }
    *length = fread(text, 1u, capacity - 1u, file);
    text[*length] = '\0';
    (void)fclose(file);
    return SDOS_OK;
}

static void linux_destroy(sdos_dsp *dsp)
{
    free(dsp->backend);
}

static const sdos_backend_ops linux_ops = {
    "linux", linux_query, linux_install, linux_set_running, linux_connect, linux_disconnect, linux_send,
    linux_receive, linux_map_region, linux_sync, linux_sync, linux_trace, linux_destroy,
};

void sdos__linux_set_rpmsg_open(sdos_rpmsg_open_fn open_fn)
{
    rpmsg_open = open_fn != NULL ? open_fn : default_rpmsg_open;
}

sdos_status sdos__linux_init(sdos_dsp *dsp)
{
    linux_state *st = calloc(1u, sizeof *st);
    if (st == NULL) {
        return SDOS_E_INTERNAL;
    }
    if (rpmsg_open == NULL) {
        rpmsg_open = default_rpmsg_open;
    }
    st->fd = -1;
    dsp->backend = st;
    dsp->ops = &linux_ops;
    return SDOS_OK;
}
