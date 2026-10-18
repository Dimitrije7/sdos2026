/* dspctl: list, load, start, stop, and talk to the DSPs. See docs/dspctl.md.
 *
 * Exit status: 0 success, 1 the operation failed, 2 usage error. */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdos/dsp.h"
#include "sdos/runtime.h"

#define EXIT_FAIL 1
#define EXIT_USAGE 2

static const char usage[] =
    "usage: dspctl [--backend linux|mock] [--root DIR] [--timeout MS] [--json] <command> [args]\n"
    "\n"
    "  list                          every DSP, its state, and its firmware\n"
    "  status <dsp>                  state, and identity and counters when running\n"
    "  load <dsp> <firmware.elf>     install firmware on a stopped DSP after checking it fits\n"
    "  start <dsp>                   start and wait for the firmware to answer\n"
    "  stop <dsp>                    stop the DSP\n"
    "  recover <dsp>                 stop if running or crashed, then start\n"
    "  trace <dsp>                   print the firmware trace buffer\n"
    "  echo <dsp> [text]             send text and print the reply\n"
    "  run <dsp> <algorithm> <in> <out> [gain | coefficients]\n"
    "                                run copy, gain_q15 (gain), or fir_q15 (comma-separated\n"
    "                                Q15 coefficients) on file <in>, write <out>\n"
    "\n"
    "<dsp> is c66x_0, c66x_1, or c71x_0. The mock backend runs the DSP runtime on\n"
    "this computer; it needs no board. --json makes list and status print one JSON\n"
    "object for scripts.\n";

typedef struct cli {
    sdos_dsp_options options;
    int json;
} cli;

static int fail(const sdos_dsp *dsp, const char *what, sdos_status status)
{
    fprintf(stderr, "dspctl: %s: %s (%s)\n", what, sdos_dsp_error(dsp), sdos_status_name((uint32_t)status));
    return EXIT_FAIL;
}

static sdos_dsp *open_dsp(cli *c, const char *name)
{
    sdos_dsp *dsp = NULL;
    sdos_status status = sdos_dsp_open(name, &c->options, &dsp);
    if (status != SDOS_OK) {
        size_t i;
        fprintf(stderr, "dspctl: unknown DSP or backend '%s'; DSPs:", name);
        for (i = 0u; i < sdos_dsp_count(); i++) {
            fprintf(stderr, " %s", sdos_dsp_name(i));
        }
        fprintf(stderr, "\n");
    }
    return dsp;
}

static const char *runtime_name(uint8_t runtime)
{
    switch (runtime) {
    case SDOS_RUNTIME_FREERTOS:
        return "freertos";
    case SDOS_RUNTIME_BAREMETAL:
        return "baremetal";
    case SDOS_RUNTIME_MOCK:
        return "mock";
    default:
        return "unknown";
    }
}

static void print_identity(const sdos_identity *id)
{
    printf("  firmware   %s\n", id->firmware);
    printf("  runtime    %s %u.%u.%u\n", runtime_name(id->runtime), (unsigned)(id->runtime_version >> 16),
           (unsigned)((id->runtime_version >> 8) & 0xFFu), (unsigned)(id->runtime_version & 0xFFu));
    printf("  region     %u bytes, %u-byte alignment\n", (unsigned)id->region_size, (unsigned)id->buffer_alignment);
}

/* Print a JSON string literal; DSP and firmware names are short ASCII. */
static void json_string(const char *text)
{
    putchar('"');
    for (; *text != '\0'; text++) {
        unsigned char ch = (unsigned char)*text;
        if (ch == '"' || ch == '\\') {
            printf("\\%c", ch);
        } else if (ch < 0x20u || ch > 0x7Eu) {
            printf("\\u%04x", ch);
        } else {
            putchar(ch);
        }
    }
    putchar('"');
}

static void json_info(const sdos_dsp_info *info)
{
    printf("{\"name\": ");
    json_string(info->name);
    printf(", \"device\": ");
    json_string(info->device);
    printf(", \"remoteproc\": ");
    json_string(info->remoteproc);
    printf(", \"state\": ");
    json_string(sdos_dsp_state_name(info->state));
    printf(", \"firmware\": ");
    json_string(info->firmware);
}

static int cmd_list_json(cli *c)
{
    size_t i;
    printf("{\"dsps\": [");
    for (i = 0u; i < sdos_dsp_count(); i++) {
        sdos_dsp *dsp = open_dsp(c, sdos_dsp_name(i));
        sdos_dsp_info info;
        if (dsp == NULL || sdos_dsp_info_get(dsp, &info) != SDOS_OK) {
            printf("]}\n");
            sdos_dsp_close(dsp);
            return EXIT_FAIL;
        }
        printf("%s", i > 0u ? ", " : "");
        json_info(&info);
        printf("}");
        sdos_dsp_close(dsp);
    }
    printf("]}\n");
    return 0;
}

static int cmd_list(cli *c)
{
    size_t i;
    int result = 0;
    if (c->json) {
        return cmd_list_json(c);
    }
    printf("%-8s %-16s %-12s %-10s %s\n", "DSP", "DEVICE", "REMOTEPROC", "STATE", "FIRMWARE");
    for (i = 0u; i < sdos_dsp_count(); i++) {
        sdos_dsp *dsp = open_dsp(c, sdos_dsp_name(i));
        sdos_dsp_info info;
        if (dsp == NULL) {
            return EXIT_FAIL;
        }
        if (sdos_dsp_info_get(dsp, &info) == SDOS_OK) {
            printf("%-8s %-16s %-12s %-10s %s\n", info.name, info.device,
                   info.remoteproc[0] != '\0' ? info.remoteproc : "-", sdos_dsp_state_name(info.state),
                   info.firmware[0] != '\0' ? info.firmware : "-");
        } else {
            result = fail(dsp, info.name, SDOS_E_TRANSPORT);
        }
        sdos_dsp_close(dsp);
    }
    return result;
}

static int cmd_status_json(sdos_dsp *dsp)
{
    sdos_dsp_info info;
    sdos_identity id;
    sdos_stats stats;
    sdos_status status = sdos_dsp_info_get(dsp, &info);
    if (status != SDOS_OK) {
        return fail(dsp, "status", status);
    }
    json_info(&info);
    if (info.state == SDOS_DSP_RUNNING) {
        status = sdos_dsp_hello(dsp, &id);
        if (status == SDOS_OK) {
            status = sdos_dsp_status(dsp, &stats);
        }
        if (status != SDOS_OK) {
            printf(", \"error\": ");
            json_string(sdos_dsp_error(dsp));
            printf("}\n");
            return EXIT_FAIL;
        }
        printf(", \"identity\": {\"firmware\": ");
        json_string(id.firmware);
        printf(", \"core\": %u, \"runtime\": ", (unsigned)id.core);
        json_string(runtime_name(id.runtime));
        printf(", \"runtime_version\": \"%u.%u.%u\", \"capabilities\": %u, \"region_size\": %u, "
               "\"buffer_alignment\": %u}",
               (unsigned)(id.runtime_version >> 16), (unsigned)((id.runtime_version >> 8) & 0xFFu),
               (unsigned)(id.runtime_version & 0xFFu), (unsigned)id.capabilities, (unsigned)id.region_size,
               (unsigned)id.buffer_alignment);
        printf(", \"stats\": {\"uptime_ms\": %u, \"requests\": %u, \"errors\": %u, \"runs\": %u, "
               "\"last_error\": ",
               (unsigned)stats.uptime_ms, (unsigned)stats.requests, (unsigned)stats.errors, (unsigned)stats.runs);
        json_string(sdos_status_name(stats.last_error));
        printf("}");
    }
    printf("}\n");
    return 0;
}

static int cmd_status(sdos_dsp *dsp)
{
    sdos_dsp_info info;
    sdos_identity id;
    sdos_stats stats;
    sdos_status status = sdos_dsp_info_get(dsp, &info);
    if (status != SDOS_OK) {
        return fail(dsp, "status", status);
    }
    printf("%s (%s) %s\n", info.label, info.device, sdos_dsp_state_name(info.state));
    if (info.state != SDOS_DSP_RUNNING) {
        return 0;
    }
    status = sdos_dsp_hello(dsp, &id);
    if (status == SDOS_OK) {
        print_identity(&id);
        status = sdos_dsp_status(dsp, &stats);
    }
    if (status != SDOS_OK) {
        return fail(dsp, "status", status);
    }
    printf("  requests   %u, errors %u, runs %u, last error %s\n", (unsigned)stats.requests,
           (unsigned)stats.errors, (unsigned)stats.runs, sdos_status_name(stats.last_error));
    return 0;
}

static int cmd_echo(sdos_dsp *dsp, int argc, char **argv)
{
    char text[SDOS_PAYLOAD_MAX + 1u];
    char reply[SDOS_PAYLOAD_MAX + 1u];
    size_t length = 0u;
    size_t n = 0u;
    int i;
    sdos_status status;
    text[0] = '\0';
    for (i = 0; i < argc; i++) {
        int w = snprintf(text + length, sizeof text - length, "%s%s", i > 0 ? " " : "", argv[i]);
        if (w < 0 || (size_t)w >= sizeof text - length) {
            fprintf(stderr, "dspctl: echo text is longer than %u bytes\n", (unsigned)SDOS_PAYLOAD_MAX);
            return EXIT_USAGE;
        }
        length += (size_t)w;
    }
    if (argc == 0) {
        (void)strcpy(text, "ping");
        length = 4u;
    }
    status = sdos_dsp_echo(dsp, text, length, reply, SDOS_PAYLOAD_MAX, &n);
    if (status != SDOS_OK) {
        return fail(dsp, "echo", status);
    }
    reply[n] = '\0';
    printf("%s\n", reply);
    return n == length && memcmp(reply, text, n) == 0 ? 0 : EXIT_FAIL;
}

static long read_file(const char *path, uint8_t *data, size_t capacity)
{
    FILE *file = fopen(path, "rb");
    size_t n;
    if (file == NULL) {
        fprintf(stderr, "dspctl: %s: %s\n", path, strerror(errno));
        return -1;
    }
    n = fread(data, 1u, capacity, file);
    if (!feof(file)) {
        fprintf(stderr, "dspctl: %s is larger than %zu bytes\n", path, capacity);
        (void)fclose(file);
        return -1;
    }
    (void)fclose(file);
    return (long)n;
}

static int cmd_run(sdos_dsp *dsp, int argc, char **argv)
{
    const uint32_t half = 256u * 1024u; /* input at 0, output at 256 KiB */
    sdos_buffer in;
    sdos_buffer out;
    sdos_run_result result;
    static uint8_t params[SDOS_RUN_PARAMS_MAX];
    size_t params_length = 0u;
    uint32_t algorithm;
    long n;
    sdos_status status;
    FILE *file;

    if (argc < 3) {
        fputs(usage, stderr);
        return EXIT_USAGE;
    }
    if (strcmp(argv[0], "copy") == 0) {
        algorithm = SDOS_ALG_COPY;
    } else if (strcmp(argv[0], "gain_q15") == 0) {
        long gain = argc > 3 ? strtol(argv[3], NULL, 0) : 16384;
        if (gain < -32768 || gain > 32767) {
            fprintf(stderr, "dspctl: gain must be a Q15 value from -32768 to 32767\n");
            return EXIT_USAGE;
        }
        algorithm = SDOS_ALG_GAIN_Q15;
        params[0] = (uint8_t)((unsigned long)gain & 0xFFu);
        params[1] = (uint8_t)(((unsigned long)gain >> 8) & 0xFFu);
        params_length = 2u;
    } else if (strcmp(argv[0], "fir_q15") == 0) {
        char *list = argc > 3 ? argv[3] : NULL;
        const char *token;
        size_t taps = 0u;
        if (list == NULL) {
            fprintf(stderr, "dspctl: fir_q15 needs comma-separated Q15 coefficients, e.g. 16384,16384\n");
            return EXIT_USAGE;
        }
        for (token = strtok(list, ","); token != NULL; token = strtok(NULL, ",")) {
            long value = strtol(token, NULL, 0);
            if (taps >= SDOS_FIR_MAX_TAPS || value < -32768 || value > 32767) {
                fprintf(stderr, "dspctl: at most %u Q15 coefficients from -32768 to 32767\n",
                        (unsigned)SDOS_FIR_MAX_TAPS);
                return EXIT_USAGE;
            }
            params[2u + 2u * taps] = (uint8_t)((unsigned long)value & 0xFFu);
            params[3u + 2u * taps] = (uint8_t)(((unsigned long)value >> 8) & 0xFFu);
            taps++;
        }
        params[0] = (uint8_t)(taps & 0xFFu);
        params[1] = (uint8_t)(taps >> 8);
        algorithm = SDOS_ALG_FIR_Q15;
        params_length = 2u + 2u * taps;
    } else {
        fprintf(stderr, "dspctl: unknown algorithm '%s'; known: copy, gain_q15, fir_q15\n", argv[0]);
        return EXIT_USAGE;
    }
    status = sdos_dsp_buffer_acquire(dsp, 0u, half, &in);
    if (status == SDOS_OK) {
        status = sdos_dsp_buffer_acquire(dsp, half, half, &out);
    }
    if (status != SDOS_OK) {
        return fail(dsp, "buffers", status);
    }
    n = read_file(argv[1], in.data, half);
    if (n < 0) {
        return EXIT_FAIL;
    }
    in.length = (uint32_t)n;
    status = sdos_dsp_run(dsp, algorithm, &in, &out, params, params_length, &result);
    if (status != SDOS_OK) {
        return fail(dsp, "run", status);
    }
    file = fopen(argv[2], "wb");
    if (file == NULL || fwrite(out.data, 1u, result.output_length, file) != result.output_length) {
        fprintf(stderr, "dspctl: cannot write %s\n", argv[2]);
        if (file != NULL) {
            (void)fclose(file);
        }
        return EXIT_FAIL;
    }
    (void)fclose(file);
    printf("%s: %u bytes in, %u bytes out, %llu DSP cycles, checksums verified\n", argv[0], (unsigned)n,
           (unsigned)result.output_length, (unsigned long long)result.cycles);
    (void)sdos_dsp_buffer_release(dsp, &in);
    (void)sdos_dsp_buffer_release(dsp, &out);
    return 0;
}

static int dispatch(const cli *c, const char *command, sdos_dsp *dsp, int argc, char **argv)
{
    sdos_identity id;
    sdos_status status;
    if (strcmp(command, "status") == 0) {
        return c->json ? cmd_status_json(dsp) : cmd_status(dsp);
    }
    if (strcmp(command, "load") == 0) {
        if (argc != 1) {
            fputs(usage, stderr);
            return EXIT_USAGE;
        }
        status = sdos_dsp_load(dsp, argv[0]);
        return status == SDOS_OK ? (printf("loaded %s\n", argv[0]), 0) : fail(dsp, "load", status);
    }
    if (strcmp(command, "start") == 0 || strcmp(command, "recover") == 0) {
        status = command[0] == 's' ? sdos_dsp_start(dsp, &id) : sdos_dsp_recover(dsp, &id);
        if (status != SDOS_OK) {
            return fail(dsp, command, status);
        }
        printf("running\n");
        print_identity(&id);
        return 0;
    }
    if (strcmp(command, "stop") == 0) {
        status = sdos_dsp_stop(dsp);
        return status == SDOS_OK ? 0 : fail(dsp, "stop", status);
    }
    if (strcmp(command, "trace") == 0) {
        static char text[1u << 20];
        size_t n = 0u;
        status = sdos_dsp_trace(dsp, text, sizeof text, &n);
        if (status != SDOS_OK) {
            return fail(dsp, "trace", status);
        }
        fwrite(text, 1u, n, stdout);
        return 0;
    }
    if (strcmp(command, "echo") == 0) {
        return cmd_echo(dsp, argc, argv);
    }
    if (strcmp(command, "run") == 0) {
        return cmd_run(dsp, argc, argv);
    }
    fprintf(stderr, "dspctl: unknown command '%s'\n\n%s", command, usage);
    return EXIT_USAGE;
}

int main(int argc, char **argv)
{
    cli c;
    int i = 1;
    int result;
    sdos_dsp *dsp;

    memset(&c, 0, sizeof c);
    while (i < argc && strncmp(argv[i], "--", 2) == 0) {
        if (strcmp(argv[i], "--help") == 0) {
            fputs(usage, stdout);
            return 0;
        }
        if (strcmp(argv[i], "--json") == 0) {
            c.json = 1;
            i += 1;
            continue;
        }
        if (i + 1 >= argc) {
            fputs(usage, stderr);
            return EXIT_USAGE;
        }
        if (strcmp(argv[i], "--backend") == 0) {
            c.options.backend = argv[i + 1];
        } else if (strcmp(argv[i], "--root") == 0) {
            c.options.root = argv[i + 1];
        } else if (strcmp(argv[i], "--timeout") == 0) {
            c.options.timeout_ms = (uint32_t)strtoul(argv[i + 1], NULL, 10);
        } else {
            fprintf(stderr, "dspctl: unknown option %s\n\n%s", argv[i], usage);
            return EXIT_USAGE;
        }
        i += 2;
    }
    if (i >= argc) {
        fputs(usage, stderr);
        return EXIT_USAGE;
    }
    if (strcmp(argv[i], "list") == 0) {
        return cmd_list(&c);
    }
    if (i + 1 >= argc) {
        fputs(usage, stderr);
        return EXIT_USAGE;
    }
    dsp = open_dsp(&c, argv[i + 1]);
    if (dsp == NULL) {
        return EXIT_USAGE;
    }
    result = dispatch(&c, argv[i], dsp, argc - i - 2, argv + i + 2);
    sdos_dsp_close(dsp);
    return result;
}
