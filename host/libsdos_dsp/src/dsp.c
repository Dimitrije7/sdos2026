/* Session logic shared by every backend: requests, negotiation, buffers. */
#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "internal.h"

#define DEFAULT_TIMEOUT_MS 2000u

sdos_status sdos__fail(sdos_dsp *dsp, sdos_status status, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    (void)vsnprintf(dsp->error, sizeof dsp->error, format, args);
    va_end(args);
    return status;
}

int sdos__format(char *out, size_t capacity, const char *format, ...)
{
    va_list args;
    int n;
    va_start(args, format);
    n = vsnprintf(out, capacity, format, args);
    va_end(args);
    return n >= 0 && (size_t)n < capacity ? 0 : -1;
}

uint64_t sdos__now_ms(void)
{
    struct timespec now;
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u;
}

size_t sdos_dsp_count(void)
{
    return SDOS_CORE_COUNT;
}

const char *sdos_dsp_name(size_t index)
{
    return index < SDOS_CORE_COUNT ? sdos_core_table[index].name : NULL;
}

const char *sdos_dsp_state_name(sdos_dsp_state state)
{
    switch (state) {
    case SDOS_DSP_ABSENT:
        return "absent";
    case SDOS_DSP_OFFLINE:
        return "offline";
    case SDOS_DSP_RUNNING:
        return "running";
    case SDOS_DSP_CRASHED:
        return "crashed";
    default:
        return "other";
    }
}

sdos_status sdos_dsp_open(const char *name, const sdos_dsp_options *options, sdos_dsp **out)
{
    const char *backend = options != NULL && options->backend != NULL ? options->backend : "linux";
    const char *root = options != NULL && options->root != NULL ? options->root : "/";
    sdos_dsp *dsp;
    sdos_status status;
    size_t i;

    *out = NULL;
    dsp = calloc(1u, sizeof *dsp);
    if (dsp == NULL) {
        return SDOS_E_INTERNAL;
    }
    for (i = 0u; i < SDOS_CORE_COUNT; i++) {
        if (name != NULL && strcmp(name, sdos_core_table[i].name) == 0) {
            dsp->core = &sdos_core_table[i];
            dsp->core_index = i;
        }
    }
    dsp->timeout_ms = options != NULL && options->timeout_ms != 0u ? options->timeout_ms : DEFAULT_TIMEOUT_MS;
    dsp->next_request = (uint32_t)(sdos__now_ms() & 0xFFFFu) << 16; /* ids differ across runs */
    /* A bounded root keeps every path the backends compose far below
     * SDOS_PATH_MAX, so they never truncate. */
    if (dsp->core == NULL || strlen(root) > 200u) {
        free(dsp);
        return SDOS_E_FIELD;
    }
    (void)strcpy(dsp->root, root);
    if (strcmp(backend, "linux") == 0) {
        status = sdos__linux_init(dsp);
    } else if (strcmp(backend, "mock") == 0) {
        status = sdos__mock_init(dsp);
    } else {
        status = SDOS_E_FIELD;
    }
    if (status != SDOS_OK) {
        free(dsp);
        return status;
    }
    *out = dsp;
    return SDOS_OK;
}

void sdos_dsp_close(sdos_dsp *dsp)
{
    if (dsp == NULL) {
        return;
    }
    if (dsp->connected) {
        dsp->ops->disconnect(dsp);
    }
    dsp->ops->destroy(dsp);
    free(dsp);
}

const char *sdos_dsp_error(const sdos_dsp *dsp)
{
    return dsp != NULL && dsp->error[0] != '\0' ? dsp->error : "no error";
}

sdos_status sdos_dsp_info_get(sdos_dsp *dsp, sdos_dsp_info *info)
{
    memset(info, 0, sizeof *info);
    info->name = dsp->core->name;
    info->label = dsp->core->label;
    info->device = dsp->core->device;
    return dsp->ops->query(dsp, info);
}

static sdos_status require_state(sdos_dsp *dsp, sdos_dsp_state wanted, const char *action)
{
    sdos_dsp_info info;
    sdos_status status = sdos_dsp_info_get(dsp, &info);
    if (status != SDOS_OK) {
        return status;
    }
    if (info.state == SDOS_DSP_ABSENT) {
        return sdos__fail(dsp, SDOS_E_TRANSPORT, "%s (%s) is not present on this system", dsp->core->name,
                          dsp->core->device);
    }
    if (info.state != wanted) {
        return sdos__fail(dsp, SDOS_E_STATE, "%s needs %s %s, but it is %s", action, dsp->core->name,
                          sdos_dsp_state_name(wanted), sdos_dsp_state_name(info.state));
    }
    return SDOS_OK;
}

static void disconnect(sdos_dsp *dsp)
{
    if (dsp->connected) {
        dsp->ops->disconnect(dsp);
        dsp->connected = 0;
    }
    dsp->have_identity = 0;
}

/* Send one request and wait for its response. Responses to older requests,
 * left over after a timeout, are counted and dropped. */
static sdos_status request(sdos_dsp *dsp, uint16_t type, const uint8_t *payload, uint16_t length, uint8_t *reply,
                           size_t capacity, size_t *reply_length)
{
    uint8_t message[SDOS_MSG_MAX];
    uint8_t response[SDOS_MSG_MAX];
    sdos_header header = {SDOS_PROTO_MAJOR, SDOS_PROTO_MINOR, type, 0u, 0u, length, 0u, 0u};
    uint64_t deadline;
    size_t n;
    sdos_status status;

    if (!dsp->connected) {
        status = dsp->ops->connect(dsp);
        if (status != SDOS_OK) {
            return status;
        }
        dsp->connected = 1;
    }
    header.request_id = ++dsp->next_request;
    n = sdos_encode(&header, payload, message, sizeof message);
    if (n == 0u) {
        return sdos__fail(dsp, SDOS_E_LENGTH, "a %u-byte payload does not fit one message", (unsigned)length);
    }
    status = dsp->ops->send(dsp, message, n);
    if (status != SDOS_OK) {
        return status;
    }
    deadline = sdos__now_ms() + dsp->timeout_ms;
    for (;;) {
        uint64_t now = sdos__now_ms();
        sdos_header got;
        size_t received = 0u;
        if (now >= deadline) {
            return sdos__fail(dsp, SDOS_E_TIMEOUT, "%s did not answer request %u within %u ms", dsp->core->name,
                              (unsigned)header.request_id, (unsigned)dsp->timeout_ms);
        }
        status = dsp->ops->receive(dsp, response, sizeof response, &received, (uint32_t)(deadline - now));
        if (status != SDOS_OK) {
            return status;
        }
        status = sdos_decode_header(response, received, &got);
        if (status != SDOS_OK) {
            return sdos__fail(dsp, status, "malformed response from %s: %s", dsp->core->name,
                              sdos_status_name((uint32_t)status));
        }
        if ((got.flags & SDOS_FLAG_RESPONSE) == 0u || got.request_id != header.request_id) {
            dsp->stale_responses++;
            continue;
        }
        if (got.type != type) {
            return sdos__fail(dsp, SDOS_E_FIELD, "response type %u to a type %u request", got.type, type);
        }
        if (got.status != SDOS_OK) {
            return sdos__fail(dsp, (sdos_status)got.status, "%s refused the request: %s", dsp->core->name,
                              sdos_status_name(got.status));
        }
        if (got.payload_length > capacity) {
            return sdos__fail(dsp, SDOS_E_LENGTH, "a %u-byte response does not fit %zu bytes", got.payload_length,
                              capacity);
        }
        if (got.payload_length > 0u) {
            memcpy(reply, response + SDOS_HEADER_SIZE, got.payload_length);
        }
        *reply_length = got.payload_length;
        return SDOS_OK;
    }
}

sdos_status sdos_dsp_hello(sdos_dsp *dsp, sdos_identity *identity)
{
    uint8_t reply[SDOS_PAYLOAD_MAX];
    size_t n = 0u;
    sdos_status status = request(dsp, SDOS_MSG_HELLO, NULL, 0u, reply, sizeof reply, &n);
    if (status == SDOS_OK) {
        status = sdos_identity_decode(reply, n, &dsp->identity);
        if (status != SDOS_OK) {
            return sdos__fail(dsp, status, "%s sent an invalid identity", dsp->core->name);
        }
        dsp->have_identity = 1;
        if (identity != NULL) {
            *identity = dsp->identity;
        }
    }
    return status;
}

sdos_status sdos_dsp_echo(sdos_dsp *dsp, const void *data, size_t length, void *reply, size_t capacity,
                          size_t *reply_length)
{
    if (length > SDOS_PAYLOAD_MAX) {
        return sdos__fail(dsp, SDOS_E_LENGTH, "echo carries at most %u bytes", (unsigned)SDOS_PAYLOAD_MAX);
    }
    return request(dsp, SDOS_MSG_ECHO, data, (uint16_t)length, reply, capacity, reply_length);
}

sdos_status sdos_dsp_status(sdos_dsp *dsp, sdos_stats *stats)
{
    uint8_t reply[SDOS_PAYLOAD_MAX];
    size_t n = 0u;
    sdos_status status = request(dsp, SDOS_MSG_STATUS, NULL, 0u, reply, sizeof reply, &n);
    return status == SDOS_OK ? sdos_stats_decode(reply, n, stats) : status;
}

sdos_status sdos_dsp_load(sdos_dsp *dsp, const char *elf_path)
{
    char why[160];
    sdos_status status = require_state(dsp, SDOS_DSP_OFFLINE, "load");
    if (status != SDOS_OK) {
        return status;
    }
    status = sdos__elf_check(elf_path, dsp->core_index, why, sizeof why);
    if (status != SDOS_OK) {
        return sdos__fail(dsp, status, "%s: %s", elf_path, why);
    }
    return dsp->ops->install(dsp, elf_path);
}

#define START_ATTEMPT_MS 500u

sdos_status sdos_dsp_start(sdos_dsp *dsp, sdos_identity *identity)
{
    uint64_t deadline;
    uint32_t whole;
    sdos_status status = require_state(dsp, SDOS_DSP_OFFLINE, "start");
    if (status != SDOS_OK) {
        return status;
    }
    disconnect(dsp);
    status = dsp->ops->set_running(dsp, 1);
    if (status != SDOS_OK) {
        return status;
    }
    dsp->generation++;
    /* The firmware announces its endpoint some time after the state changes,
     * and a HELLO sent before that is lost: retry with short attempts. One
     * attempt as long as the whole timeout made start fail on a C71x that
     * answered a moment later (board, Task 021). */
    deadline = sdos__now_ms() + dsp->timeout_ms;
    whole = dsp->timeout_ms;
    dsp->timeout_ms = whole < START_ATTEMPT_MS ? whole : START_ATTEMPT_MS;
    do {
        status = sdos_dsp_hello(dsp, identity);
        if (status == SDOS_OK || status == SDOS_E_VERSION || status == SDOS_E_FIELD) {
            break;
        }
        disconnect(dsp);
    } while (sdos__now_ms() < deadline);
    dsp->timeout_ms = whole;
    return status;
}

sdos_status sdos_dsp_stop(sdos_dsp *dsp)
{
    sdos_status status = require_state(dsp, SDOS_DSP_RUNNING, "stop");
    if (status == SDOS_E_STATE) {
        sdos_dsp_info info;
        if (sdos_dsp_info_get(dsp, &info) == SDOS_OK && info.state == SDOS_DSP_CRASHED) {
            status = SDOS_OK; /* a crashed core can and must be stopped */
        }
    }
    if (status != SDOS_OK) {
        return status;
    }
    disconnect(dsp);
    return dsp->ops->set_running(dsp, 0);
}

sdos_status sdos_dsp_recover(sdos_dsp *dsp, sdos_identity *identity)
{
    sdos_dsp_info info;
    sdos_status status = sdos_dsp_info_get(dsp, &info);
    if (status == SDOS_OK && (info.state == SDOS_DSP_RUNNING || info.state == SDOS_DSP_CRASHED)) {
        status = sdos_dsp_stop(dsp);
    }
    return status == SDOS_OK ? sdos_dsp_start(dsp, identity) : status;
}

static sdos_status refresh(const sdos_dsp *dsp, sdos_buffer *buffer)
{
    sdos_buf_state next;
    if (buffer->generation != dsp->generation) {
        if (sdos_buf_next(buffer->state, SDOS_BUF_DSP_RESTARTED, &next) == SDOS_OK) {
            buffer->state = next;
        }
        buffer->generation = dsp->generation;
    }
    return SDOS_OK;
}

static sdos_status step(sdos_dsp *dsp, sdos_buffer *buffer, sdos_buf_event event)
{
    sdos_buf_state next;
    if (sdos_buf_next(buffer->state, event, &next) != SDOS_OK) {
        return sdos__fail(dsp, SDOS_E_STATE, "buffer at offset %u is in state %d; step %d is not allowed",
                          (unsigned)buffer->offset, (int)buffer->state, (int)event);
    }
    buffer->state = next;
    return SDOS_OK;
}

sdos_status sdos_dsp_buffer_acquire(sdos_dsp *dsp, uint32_t offset, uint32_t length, sdos_buffer *buffer)
{
    sdos_descriptor d = {0u, offset, length, 0u, 0u, 0u};
    sdos_status status;
    memset(buffer, 0, sizeof *buffer);
    if (!dsp->have_identity) {
        status = sdos_dsp_hello(dsp, NULL);
        if (status != SDOS_OK) {
            return status;
        }
    }
    if (dsp->region == NULL) {
        status = dsp->ops->map_region(dsp, &dsp->region, &dsp->region_size);
        if (status != SDOS_OK) {
            return status;
        }
    }
    if (dsp->identity.region_size < dsp->region_size) {
        dsp->region_size = dsp->identity.region_size; /* the firmware may use less */
    }
    status = sdos_descriptor_check(&d, dsp->region_size, dsp->identity.buffer_alignment);
    if (status != SDOS_OK) {
        return sdos__fail(dsp, status, "range %u+%u is outside the %u-byte region or not %u-aligned",
                          (unsigned)offset, (unsigned)length, (unsigned)dsp->region_size,
                          (unsigned)dsp->identity.buffer_alignment);
    }
    buffer->data = dsp->region + offset;
    buffer->offset = offset;
    buffer->length = length;
    buffer->generation = dsp->generation;
    buffer->state = SDOS_BUF_FREE;
    return step(dsp, buffer, SDOS_BUF_ACQUIRE);
}

sdos_status sdos_dsp_buffer_release(sdos_dsp *dsp, sdos_buffer *buffer)
{
    (void)refresh(dsp, buffer);
    return step(dsp, buffer, SDOS_BUF_RELEASE);
}

static sdos_descriptor describe(const sdos_buffer *buffer, uint32_t sequence)
{
    sdos_descriptor d = {0u, buffer->offset, buffer->length, sequence, 0u, SDOS_DESC_CHECKSUM};
    return d;
}

sdos_status sdos_dsp_run_control(sdos_dsp *dsp, uint32_t algorithm, const void *params, size_t params_length,
                                 sdos_run_result *result)
{
    sdos_run_request rq;
    uint8_t payload[SDOS_PAYLOAD_MAX];
    uint8_t reply[SDOS_PAYLOAD_MAX];
    size_t n = 0u;
    sdos_status status;

    if (params_length > SDOS_RUN_PARAMS_MAX) {
        return sdos__fail(dsp, SDOS_E_FIELD, "params hold at most %u bytes", (unsigned)SDOS_RUN_PARAMS_MAX);
    }
    memset(&rq, 0, sizeof rq); /* both descriptors empty: offset 0, length 0, no checksum */
    rq.algorithm = algorithm;
    rq.budget_us = dsp->run_budget_us;
    rq.output.sequence = 1u;
    rq.params_length = (uint16_t)params_length;
    if (params_length > 0u) {
        memcpy(rq.params, params, params_length);
    }
    n = sdos_run_request_encode(&rq, payload, sizeof payload);
    status = request(dsp, SDOS_MSG_RUN, payload, (uint16_t)n, reply, sizeof reply, &n);
    if (status != SDOS_OK) {
        return status;
    }
    status = sdos_run_result_decode(reply, n, result);
    if (status != SDOS_OK) {
        return sdos__fail(dsp, status, "%s sent an invalid RUN result", dsp->core->name);
    }
    if (result->output_length != 0u || result->output_sequence != rq.output.sequence) {
        return sdos__fail(dsp, SDOS_E_FIELD, "RUN result does not match the request");
    }
    return SDOS_OK;
}

sdos_status sdos_dsp_run(sdos_dsp *dsp, uint32_t algorithm, sdos_buffer *input, sdos_buffer *output,
                         const void *params, size_t params_length, sdos_run_result *result)
{
    sdos_run_request rq;
    uint8_t payload[SDOS_PAYLOAD_MAX];
    uint8_t reply[SDOS_PAYLOAD_MAX];
    size_t n = 0u;
    sdos_status status;

    if (input == output || params_length > SDOS_RUN_PARAMS_MAX) {
        return sdos__fail(dsp, SDOS_E_FIELD, "input and output must differ; params hold at most %u bytes",
                          (unsigned)SDOS_RUN_PARAMS_MAX);
    }
    (void)refresh(dsp, input);
    (void)refresh(dsp, output);
    memset(&rq, 0, sizeof rq);
    rq.algorithm = algorithm;
    rq.budget_us = dsp->run_budget_us;
    rq.input = describe(input, ++input->sequence);
    rq.input.checksum = sdos_crc32c(0u, input->data, input->length);
    rq.output = describe(output, ++output->sequence);
    rq.params_length = (uint16_t)params_length;
    if (params_length > 0u) {
        memcpy(rq.params, params, params_length);
    }
    status = step(dsp, input, SDOS_BUF_SUBMIT);
    if (status != SDOS_OK) {
        return status;
    }
    status = step(dsp, output, SDOS_BUF_SUBMIT);
    if (status != SDOS_OK) {
        (void)step(dsp, input, SDOS_BUF_REJECTED);
        return status;
    }
    dsp->ops->sync_for_device(dsp, input->offset, input->length);
    dsp->ops->sync_for_device(dsp, output->offset, output->length);
    n = sdos_run_request_encode(&rq, payload, sizeof payload);
    status = request(dsp, SDOS_MSG_RUN, payload, (uint16_t)n, reply, sizeof reply, &n);
    if (status == SDOS_E_TIMEOUT || status == SDOS_E_TRANSPORT) {
        /* Unknown whether the DSP still uses them: nobody may until a restart. */
        (void)step(dsp, input, SDOS_BUF_TIMEOUT);
        (void)step(dsp, output, SDOS_BUF_TIMEOUT);
        return status;
    }
    if (status != SDOS_OK) {
        (void)step(dsp, input, SDOS_BUF_REJECTED);
        (void)step(dsp, output, SDOS_BUF_REJECTED);
        return status;
    }
    /* The response proves the DSP accepted, finished, and wrote back both. */
    (void)step(dsp, input, SDOS_BUF_DSP_ACCEPT);
    (void)step(dsp, input, SDOS_BUF_DSP_COMPLETE);
    (void)step(dsp, output, SDOS_BUF_DSP_ACCEPT);
    (void)step(dsp, output, SDOS_BUF_DSP_COMPLETE);
    dsp->ops->sync_for_cpu(dsp, output->offset, output->length);
    (void)step(dsp, input, SDOS_BUF_HOST_COMPLETE);
    (void)step(dsp, output, SDOS_BUF_HOST_COMPLETE);
    status = sdos_run_result_decode(reply, n, result);
    if (status != SDOS_OK) {
        return sdos__fail(dsp, status, "%s sent an invalid RUN result", dsp->core->name);
    }
    if (result->output_length > output->length || result->output_sequence != rq.output.sequence) {
        return sdos__fail(dsp, SDOS_E_FIELD, "RUN result does not match the request");
    }
    if (sdos_crc32c(0u, output->data, result->output_length) != result->output_checksum) {
        return sdos__fail(dsp, SDOS_E_CHECKSUM, "output read by the host differs from what %s wrote",
                          dsp->core->name);
    }
    return SDOS_OK;
}

sdos_status sdos_dsp_trace(sdos_dsp *dsp, char *text, size_t capacity, size_t *length)
{
    return dsp->ops->trace(dsp, text, capacity, length);
}

sdos_status sdos_dsp_set_run_budget(sdos_dsp *dsp, uint32_t budget_us)
{
    if (budget_us > SDOS_WATCHDOG_US) {
        return sdos__fail(dsp, SDOS_E_FIELD, "a RUN budget is at most %u us, the watchdog limit",
                          (unsigned)SDOS_WATCHDOG_US);
    }
    dsp->run_budget_us = budget_us;
    return SDOS_OK;
}

sdos_status sdos_dsp_health(sdos_dsp *dsp, sdos_health *health)
{
    char text[SDOS_HEALTH_SIZE + 1u];
    sdos_status status = SDOS_E_STATE;
    int attempt;
    for (attempt = 0; attempt < 5 && status == SDOS_E_STATE; attempt++) {
        size_t n = 0u;
        status = dsp->ops->trace(dsp, text, sizeof text, &n);
        if (status != SDOS_OK) {
            return status;
        }
        status = sdos_health_parse(text, n, health);
    }
    switch (status) {
    case SDOS_OK:
        return SDOS_OK;
    case SDOS_E_MAGIC:
        return sdos__fail(dsp, status, "the %s trace has no health line; the firmware predates protocol 2.0",
                          dsp->core->name);
    case SDOS_E_STATE:
        return sdos__fail(dsp, status, "the %s health line kept changing while it was read", dsp->core->name);
    default:
        return sdos__fail(dsp, status, "the %s health line is malformed", dsp->core->name);
    }
}
