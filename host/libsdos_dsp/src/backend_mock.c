/* Mock backend: the DSP runtime from firmware/common, run in-process.
 *
 * Applications and tests exercise the real request handling and buffer rules
 * on a PC. A mock DSP starts out running the course runtime, so a one-shot
 * command such as `dspctl --backend mock echo c66x_0 hi` works.
 *
 * SDOS_MOCK_FAULT injects one fault for tests and demonstrations:
 *   drop     never answer RUN (the host times out and quarantines buffers)
 *   corrupt  flip an output byte after the DSP checksummed it
 *   stale    answer each request twice, so the next request sees a stale reply */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "sdos/runtime.h"

#define MOCK_REGION (1024u * 1024u)
#define QUEUE 4u

typedef struct mock_state {
    sdos_runtime runtime;
    uint8_t *region;
    sdos_dsp_state state;
    char firmware[128];
    char fault[16];
    uint8_t queue[QUEUE][SDOS_MSG_MAX];
    size_t queue_length[QUEUE];
    size_t head;
    size_t count;
    char trace[2048];
} mock_state;

static mock_state *state_of(sdos_dsp *dsp)
{
    return (mock_state *)dsp->backend;
}

static void trace(mock_state *st, const char *line)
{
    size_t used = strlen(st->trace);
    (void)snprintf(st->trace + used, sizeof st->trace - used, "[mock] %s\n", line);
}

static uint32_t mock_uptime(void)
{
    return (uint32_t)(sdos__now_ms() & 0xFFFFFFFFu);
}

static sdos_status mock_query(sdos_dsp *dsp, sdos_dsp_info *info)
{
    const mock_state *st = state_of(dsp);
    info->state = st->state;
    (void)snprintf(info->remoteproc, sizeof info->remoteproc, "mock");
    (void)snprintf(info->firmware, sizeof info->firmware, "%s", st->firmware);
    return SDOS_OK;
}

static sdos_status mock_install(sdos_dsp *dsp, const char *elf_path)
{
    mock_state *st = state_of(dsp);
    (void)snprintf(st->firmware, sizeof st->firmware, "sdos/%s.%s", dsp->core->name, dsp->core->firmware_ext);
    trace(st, "firmware installed");
    (void)elf_path;
    return SDOS_OK;
}

static sdos_status mock_set_running(sdos_dsp *dsp, int running)
{
    mock_state *st = state_of(dsp);
    st->state = running ? SDOS_DSP_RUNNING : SDOS_DSP_OFFLINE;
    st->count = 0u;
    if (running) {
        memset(&st->runtime.stats, 0, sizeof st->runtime.stats);
    }
    trace(st, running ? "started" : "stopped");
    return SDOS_OK;
}

static sdos_status mock_connect(sdos_dsp *dsp)
{
    if (state_of(dsp)->state != SDOS_DSP_RUNNING) {
        return sdos__fail(dsp, SDOS_E_TRANSPORT, "%s is not running", dsp->core->name);
    }
    return SDOS_OK;
}

static void mock_disconnect(sdos_dsp *dsp)
{
    state_of(dsp)->count = 0u;
}

static void enqueue(mock_state *st, const uint8_t *message, size_t length)
{
    size_t slot;
    if (st->count == QUEUE) {
        return;
    }
    slot = (st->head + st->count) % QUEUE;
    memcpy(st->queue[slot], message, length);
    st->queue_length[slot] = length;
    st->count++;
}

static sdos_status mock_send(sdos_dsp *dsp, const uint8_t *message, size_t length)
{
    mock_state *st = state_of(dsp);
    uint8_t reply[SDOS_MSG_MAX];
    size_t n;
    if (st->state != SDOS_DSP_RUNNING) {
        return sdos__fail(dsp, SDOS_E_TRANSPORT, "%s is not running", dsp->core->name);
    }
    n = sdos_runtime_handle(&st->runtime, message, length, reply, sizeof reply);
    if (n == 0u) {
        return SDOS_OK;
    }
    if (strcmp(st->fault, "drop") == 0 && length > 5u && message[4] == SDOS_MSG_RUN) {
        return SDOS_OK;
    }
    if (strcmp(st->fault, "corrupt") == 0 && length > 5u && message[4] == SDOS_MSG_RUN) {
        uint32_t offset = (uint32_t)message[16 + 32 + 4] | ((uint32_t)message[16 + 32 + 5] << 8) |
                          ((uint32_t)message[16 + 32 + 6] << 16) | ((uint32_t)message[16 + 32 + 7] << 24);
        if (offset < MOCK_REGION) {
            st->region[offset] ^= 0x01u;
        }
    }
    if (strcmp(st->fault, "stale") == 0) {
        enqueue(st, reply, n);
    }
    enqueue(st, reply, n);
    return SDOS_OK;
}

static sdos_status mock_receive(sdos_dsp *dsp, uint8_t *message, size_t capacity, size_t *length,
                                uint32_t timeout_ms)
{
    mock_state *st = state_of(dsp);
    (void)timeout_ms;
    if (st->count == 0u) {
        return sdos__fail(dsp, SDOS_E_TIMEOUT, "%s did not answer within %u ms", dsp->core->name,
                          (unsigned)dsp->timeout_ms);
    }
    if (st->queue_length[st->head] > capacity) {
        return SDOS_E_LENGTH;
    }
    memcpy(message, st->queue[st->head], st->queue_length[st->head]);
    *length = st->queue_length[st->head];
    st->head = (st->head + 1u) % QUEUE;
    st->count--;
    return SDOS_OK;
}

static sdos_status mock_map_region(sdos_dsp *dsp, uint8_t **base, uint32_t *size)
{
    *base = state_of(dsp)->region;
    *size = MOCK_REGION;
    return SDOS_OK;
}

static void mock_sync(sdos_dsp *dsp, uint32_t offset, uint32_t length)
{
    (void)dsp;
    (void)offset;
    (void)length;
}

static sdos_status mock_trace(sdos_dsp *dsp, char *text, size_t capacity, size_t *length)
{
    (void)snprintf(text, capacity, "%s", state_of(dsp)->trace);
    *length = strlen(text);
    return SDOS_OK;
}

static void mock_destroy(sdos_dsp *dsp)
{
    mock_state *st = state_of(dsp);
    free(st->region);
    free(st);
}

static const sdos_backend_ops mock_ops = {
    "mock", mock_query, mock_install, mock_set_running, mock_connect, mock_disconnect, mock_send,
    mock_receive, mock_map_region, mock_sync, mock_sync, mock_trace, mock_destroy,
};

sdos_status sdos__mock_init(sdos_dsp *dsp)
{
    mock_state *st = calloc(1u, sizeof *st);
    const char *fault = getenv("SDOS_MOCK_FAULT");
    if (st == NULL || (st->region = calloc(1u, MOCK_REGION)) == NULL) {
        free(st);
        return SDOS_E_INTERNAL;
    }
    st->state = SDOS_DSP_RUNNING;
    (void)snprintf(st->firmware, sizeof st->firmware, "mock/%s-course-runtime", dsp->core->name);
    (void)snprintf(st->fault, sizeof st->fault, "%s", fault != NULL ? fault : "");
    st->runtime.identity.core = (uint8_t)dsp->core_index;
    st->runtime.identity.runtime = SDOS_RUNTIME_MOCK;
    st->runtime.identity.capabilities = SDOS_CAP_ECHO | SDOS_CAP_STATUS | SDOS_CAP_RUN | SDOS_CAP_CHECKSUM;
    st->runtime.identity.max_payload = SDOS_PAYLOAD_MAX;
    st->runtime.identity.buffer_alignment = 128u;
    st->runtime.identity.runtime_version = 0x000100u;
    st->runtime.identity.region_size = MOCK_REGION;
    (void)snprintf(st->runtime.identity.firmware, sizeof st->runtime.identity.firmware, "mock %s",
                   dsp->core->name);
    st->runtime.region = st->region;
    st->runtime.region_size = MOCK_REGION;
    st->runtime.algorithms = sdos_builtin_algorithms;
    st->runtime.algorithm_count = sdos_builtin_algorithm_count;
    st->runtime.uptime_ms = mock_uptime;
    trace(st, "course runtime running");
    dsp->backend = st;
    dsp->ops = &mock_ops;
    return SDOS_OK;
}
