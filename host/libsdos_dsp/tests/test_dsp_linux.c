/* The Linux backend against a fake sysfs tree and a simulated kernel.
 *
 * A thread plays the kernel: it turns "start" written to the state file into
 * "running" and adds the rpmsg_ctrl device, and "stop" into "offline". The
 * RPMsg endpoint is a SOCK_SEQPACKET pair whose far end runs the DSP runtime,
 * so framing, timeouts, and lifecycle run through real file descriptors. */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "internal.h"
#include "sdos/dsp.h"
#include "sdos/runtime.h"
#include "sdos_elf_fixture.h"
#include "sdos_test.h"

static char root[256];
static char rproc[512];   /* sysfs directory of the fake c66x_0 remoteproc */
static char virtio[512];  /* where its rpmsg devices live */
static volatile int kernel_running = 1;
static volatile int crash_on_start = 0;
static volatile int ignore_commands = 0;

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (f != NULL) {
        (void)fputs(text, f);
        (void)fclose(f);
    }
}

static void read_file(const char *path, char *out, size_t capacity)
{
    FILE *f = fopen(path, "r");
    size_t n = 0u;
    if (f != NULL) {
        n = fread(out, 1u, capacity - 1u, f);
        (void)fclose(f);
    }
    out[n] = '\0';
}

static void link_to(const char *target, const char *path)
{
    if (symlink(target, path) != 0 && errno != EEXIST) {
        perror(path);
        exit(2);
    }
}

static void mkdirs(const char *path)
{
    char buf[512];
    char *p;
    (void)sdos__format(buf, sizeof buf, "%s", path);
    for (p = buf + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            (void)mkdir(buf, 0755);
            *p = '/';
        }
    }
    (void)mkdir(buf, 0755);
}

static void build_tree(void)
{
    char path[600];
    char target[600];
    (void)sdos__format(root, sizeof root, "/tmp/sdos-sysfs-XXXXXX");
    if (mkdtemp(root) == NULL) {
        perror("mkdtemp");
        exit(2);
    }
    /* c66x_0 enumerates as remoteproc2 here, to prove numbers are not used. */
    (void)sdos__format(rproc, sizeof rproc, "%s/sys/devices/platform/bus@100000/4d80800000.dsp/remoteproc/remoteproc2",
                   root);
    mkdirs(rproc);
    (void)sdos__format(virtio, sizeof virtio, "%s/virtio0", rproc);
    (void)sdos__format(path, sizeof path, "%s/state", rproc);
    write_file(path, "offline\n");
    (void)sdos__format(path, sizeof path, "%s/firmware", rproc);
    write_file(path, "j7-c66_0-fw\n");
    (void)sdos__format(path, sizeof path, "%s/device", rproc);
    link_to("../../../4d80800000.dsp", path);
    /* c66x_1 exists as remoteproc0 but belongs to another device address */
    (void)sdos__format(target, sizeof target, "%s/sys/devices/platform/bus@100000/4d81800000.dsp/remoteproc/remoteproc0",
                   root);
    mkdirs(target);
    (void)sdos__format(path, sizeof path, "%s/device", target);
    link_to("../../../4d81800000.dsp", path);
    (void)sdos__format(path, sizeof path, "%s/state", target);
    write_file(path, "offline\n");
    (void)sdos__format(path, sizeof path, "%s/sys/class/remoteproc", root);
    mkdirs(path);
    (void)sdos__format(path, sizeof path, "%s/sys/class/remoteproc/remoteproc2", root);
    link_to(rproc, path);
    (void)sdos__format(path, sizeof path, "%s/sys/class/remoteproc/remoteproc0", root);
    link_to(target, path);
    (void)sdos__format(path, sizeof path, "%s/sys/class/rpmsg", root);
    mkdirs(path);
    (void)sdos__format(path, sizeof path, "%s/lib/firmware", root);
    mkdirs(path);
}

static void *kernel(void *unused)
{
    char path[600];
    char ctrl[600];
    char link[600];
    char state[64];
    struct timespec tick = {0, 2000000L};
    (void)unused;
    (void)sdos__format(path, sizeof path, "%s/state", rproc);
    (void)sdos__format(ctrl, sizeof ctrl, "%s/rpmsg/rpmsg_ctrl0", virtio);
    (void)sdos__format(link, sizeof link, "%s/sys/class/rpmsg/rpmsg_ctrl0", root);
    while (kernel_running) {
        read_file(path, state, sizeof state);
        if (!ignore_commands && strncmp(state, "start", 5) == 0) {
            if (crash_on_start) {
                write_file(path, "crashed\n");
            } else {
                mkdirs(ctrl);
                link_to(ctrl, link);
                write_file(path, "running\n");
            }
        } else if (!ignore_commands && strncmp(state, "stop", 4) == 0) {
            (void)unlink(link);
            write_file(path, "offline\n");
        }
        (void)nanosleep(&tick, NULL);
    }
    return NULL;
}

/* One simulated firmware per connection, serving its own end of the pair. */
static void *firmware(void *argument) /* cppcheck-suppress constParameterCallback */
{
    static uint8_t region[65536];
    sdos_runtime *rt = calloc(1u, sizeof *rt);
    int dsp_side = (int)(intptr_t)argument;
    uint8_t msg[SDOS_MSG_MAX];
    uint8_t reply[SDOS_MSG_MAX];
    ssize_t n;
    if (rt == NULL) {
        return NULL;
    }
    rt->identity.buffer_alignment = 64u;
    (void)strcpy(rt->identity.firmware, "fake-board-fw");
    rt->region = region;
    rt->region_size = sizeof region;
    rt->algorithms = sdos_builtin_algorithms;
    rt->algorithm_count = sdos_builtin_algorithm_count;
    while ((n = read(dsp_side, msg, sizeof msg)) > 0) {
        size_t r = sdos_runtime_handle(rt, msg, (size_t)n, reply, sizeof reply);
        if (r > 0u && write(dsp_side, reply, r) != (ssize_t)r) {
            break;
        }
    }
    (void)close(dsp_side);
    free(rt);
    return NULL;
}

static int last_endpoint;

static int socket_endpoint(sdos_dsp *dsp, const char *ctrl_path, uint32_t endpoint, char *why, size_t capacity)
{
    int pair[2];
    pthread_t thread;
    (void)dsp;
    last_endpoint = (int)endpoint;
    if (strstr(ctrl_path, "/dev/rpmsg_ctrl0") == NULL || socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair) != 0) {
        (void)snprintf(why, capacity, "unexpected control device %s", ctrl_path);
        return -1;
    }
    (void)pthread_create(&thread, NULL, firmware, (void *)(intptr_t)pair[1]);
    (void)pthread_detach(thread);
    return pair[0];
}

static sdos_dsp *open_linux(const char *name, uint32_t timeout_ms)
{
    sdos_dsp_options options = {"linux", root, timeout_ms};
    sdos_dsp *dsp = NULL;
    return sdos_dsp_open(name, &options, &dsp) == SDOS_OK ? dsp : NULL;
}

int main(void)
{
    pthread_t kernel_thread;
    sdos_dsp *dsp;
    sdos_dsp *absent;
    sdos_dsp_info info;
    sdos_identity id;
    char text[64];
    char path[700];
    char elf[300];
    size_t n = 0u;

    build_tree();
    sdos__linux_set_rpmsg_open(socket_endpoint);
    (void)pthread_create(&kernel_thread, NULL, kernel, NULL);
    dsp = open_linux("c66x_0", 2000u);

    CHECK_EQ("info", sdos_dsp_info_get(dsp, &info), SDOS_OK);
    CHECK("c66x_0 is found by device address, not number",
          strcmp(info.remoteproc, "remoteproc2") == 0 && info.state == SDOS_DSP_OFFLINE);
    CHECK("current firmware name", strcmp(info.firmware, "j7-c66_0-fw") == 0);

    (void)sdos__format(elf, sizeof elf, "%s/fw.xe66", root);
    (void)sdos_write_elf(elf, 140u, 0xA6100000u, 0x2000u);
    CHECK_EQ("load", sdos_dsp_load(dsp, elf), SDOS_OK);
    (void)sdos__format(path, sizeof path, "%s/lib/firmware/sdos/c66x_0.xe66", root);
    CHECK("firmware file installed", access(path, R_OK) == 0);
    (void)sdos__format(path, sizeof path, "%s/firmware", rproc);
    read_file(path, text, sizeof text);
    CHECK("remoteproc firmware selected", strcmp(text, "sdos/c66x_0.xe66") == 0);

    CHECK_EQ("start", sdos_dsp_start(dsp, &id), SDOS_OK);
    CHECK("start negotiated with the firmware", strcmp(id.firmware, "fake-board-fw") == 0);
    CHECK_EQ("course endpoint", last_endpoint, (int)SDOS_RPMSG_ENDPOINT);
    CHECK_EQ("echo over the endpoint", sdos_dsp_echo(dsp, "board", 5u, text, sizeof text, &n), SDOS_OK);
    CHECK("echo reply", n == 5u && memcmp(text, "board", 5u) == 0);
    CHECK_EQ("load while running is refused", sdos_dsp_load(dsp, elf), SDOS_E_STATE);
    CHECK_EQ("stop", sdos_dsp_stop(dsp), SDOS_OK);
    (void)sdos_dsp_info_get(dsp, &info);
    CHECK("stopped", info.state == SDOS_DSP_OFFLINE);
    CHECK_EQ("stop twice is refused", sdos_dsp_stop(dsp), SDOS_E_STATE);
    CHECK_EQ("recover from offline starts", sdos_dsp_recover(dsp, &id), SDOS_OK);
    CHECK_EQ("stop again", sdos_dsp_stop(dsp), SDOS_OK);

    crash_on_start = 1;
    CHECK_EQ("a crash during start is reported", sdos_dsp_start(dsp, &id), SDOS_E_INTERNAL);
    CHECK("the error points at the trace", strstr(sdos_dsp_error(dsp), "dspctl trace c66x_0") != NULL);
    crash_on_start = 0;
    CHECK_EQ("a crashed core can be recovered", sdos_dsp_recover(dsp, &id), SDOS_OK);
    CHECK_EQ("stop after recovery", sdos_dsp_stop(dsp), SDOS_OK);
    sdos_dsp_close(dsp);

    dsp = open_linux("c66x_0", 100u);
    ignore_commands = 1;
    CHECK_EQ("a core that never starts times out", sdos_dsp_start(dsp, &id), SDOS_E_TIMEOUT);
    ignore_commands = 0;
    sdos_dsp_close(dsp);

    absent = open_linux("c71x_0", 100u);
    CHECK_EQ("absent core info", sdos_dsp_info_get(absent, &info), SDOS_OK);
    CHECK("a core missing from sysfs is absent", info.state == SDOS_DSP_ABSENT);
    CHECK_EQ("an absent core cannot load", sdos_dsp_load(absent, elf), SDOS_E_TRANSPORT);
    CHECK("the error names the device", strstr(sdos_dsp_error(absent), "64800000.dsp") != NULL);
    sdos_dsp_close(absent);

    kernel_running = 0;
    (void)pthread_join(kernel_thread, NULL);
    (void)sdos__format(path, sizeof path, "rm -rf '%s'", root);
    if (system(path) != 0) {
        printf("note: could not remove %s\n", root);
    }
    return TEST_EXIT();
}
