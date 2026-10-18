/* DSP-side request handling. See sdos/runtime.h and docs/protocol.md. */
#include "sdos/runtime.h"

#include <string.h>

static const sdos_algorithm *find_algorithm(const sdos_runtime *runtime, uint32_t id)
{
    size_t i;
    for (i = 0u; i < runtime->algorithm_count; i++) {
        if (runtime->algorithms[i].id == id) {
            return &runtime->algorithms[i];
        }
    }
    return NULL;
}

static int ranges_overlap(const sdos_descriptor *a, const sdos_descriptor *b)
{
    if (a->length == 0u || b->length == 0u) {
        return 0;
    }
    return a->offset < b->offset + b->length && b->offset < a->offset + a->length;
}

static uint64_t now_cycles(const sdos_runtime *runtime)
{
    return runtime->cycles != NULL ? runtime->cycles() : 0u;
}

static sdos_status run(sdos_runtime *runtime, const uint8_t *payload, uint16_t length, uint8_t *out, uint16_t *out_length)
{
    sdos_run_request *const request = &runtime->request;
    const sdos_algorithm *algorithm;
    sdos_run_result result;
    sdos_run_args args;
    uint64_t start;
    uint32_t alignment = runtime->identity.buffer_alignment;
    sdos_status status = sdos_run_request_decode(payload, length, request);

    if (status != SDOS_OK) {
        return status;
    }
    if (runtime->busy != 0) {
        return SDOS_E_BUSY;
    }
    status = sdos_descriptor_check(&request->input, runtime->region_size, alignment);
    if (status == SDOS_OK) {
        status = sdos_descriptor_check(&request->output, runtime->region_size, alignment);
    }
    if (status != SDOS_OK) {
        return status;
    }
    if (ranges_overlap(&request->input, &request->output)) {
        return SDOS_E_DESCRIPTOR;
    }
    algorithm = find_algorithm(runtime, request->algorithm);
    if (algorithm == NULL) {
        return SDOS_E_ALGORITHM;
    }

    runtime->busy = 1;
    /* The host wrote its cache back before sending RUN; drop any stale lines. */
    if (runtime->cache_invalidate != NULL && request->input.length > 0u) {
        runtime->cache_invalidate(runtime->region + request->input.offset, request->input.length);
    }
    if ((request->input.flags & SDOS_DESC_CHECKSUM) != 0u &&
        sdos_crc32c(0u, runtime->region + request->input.offset, request->input.length) != request->input.checksum) {
        runtime->busy = 0;
        return SDOS_E_CHECKSUM;
    }
    args.input = runtime->region + request->input.offset;
    args.input_length = request->input.length;
    args.output = runtime->region + request->output.offset;
    args.output_capacity = request->output.length;
    args.params = request->params;
    args.params_length = request->params_length;
    result.output_length = 0u;
    start = now_cycles(runtime);
    status = algorithm->run(&args, &result.output_length);
    result.cycles = now_cycles(runtime) - start;
    if (status == SDOS_OK && result.output_length > request->output.length) {
        status = SDOS_E_INTERNAL; /* the algorithm claims more than it was given */
    }
    if (status == SDOS_OK) {
        if (runtime->cache_writeback != NULL && result.output_length > 0u) {
            runtime->cache_writeback(args.output, result.output_length);
        }
        result.output_checksum = (request->output.flags & SDOS_DESC_CHECKSUM) != 0u
                                     ? sdos_crc32c(0u, args.output, result.output_length)
                                     : 0u;
        result.output_sequence = request->output.sequence;
        *out_length = (uint16_t)sdos_run_result_encode(&result, out, SDOS_PAYLOAD_MAX);
        runtime->stats.runs++;
    }
    runtime->busy = 0;
    return status;
}

size_t sdos_runtime_handle(sdos_runtime *runtime, const uint8_t *request, size_t length, uint8_t *response,
                           size_t capacity)
{
    uint8_t *const payload = runtime->payload;
    sdos_header header;
    sdos_header reply;
    uint16_t payload_length = 0u;
    sdos_status status;

    if (length < SDOS_HEADER_SIZE || capacity < SDOS_MSG_MAX) {
        return 0u;
    }
    status = sdos_decode_header(request, length, &header);
    if (status == SDOS_E_MAGIC) {
        /* Echo the raw type and id so the sender can still match the reply. */
        header.type = (uint16_t)(request[4] | (uint16_t)(request[5] << 8));
        header.request_id = (uint32_t)request[8] | ((uint32_t)request[9] << 8) | ((uint32_t)request[10] << 16) |
                            ((uint32_t)request[11] << 24);
    }
    if (status == SDOS_OK && (header.flags & SDOS_FLAG_RESPONSE) != 0u) {
        status = SDOS_E_FIELD; /* a response is never a request */
    }
    runtime->stats.requests++;
    runtime->stats.last_request_id = header.request_id;

    if (status == SDOS_OK) {
        const uint8_t *body = request + SDOS_HEADER_SIZE;
        switch (header.type) {
        case SDOS_MSG_HELLO:
            if (header.payload_length != 0u) {
                status = SDOS_E_LENGTH;
            } else {
                payload_length = (uint16_t)sdos_identity_encode(&runtime->identity, payload, SDOS_PAYLOAD_MAX);
                status = payload_length == 0u ? SDOS_E_INTERNAL : SDOS_OK;
            }
            break;
        case SDOS_MSG_ECHO:
            memcpy(payload, body, header.payload_length);
            payload_length = header.payload_length;
            break;
        case SDOS_MSG_STATUS:
            if (header.payload_length != 0u) {
                status = SDOS_E_LENGTH;
            } else {
                runtime->stats.uptime_ms = runtime->uptime_ms != NULL ? runtime->uptime_ms() : 0u;
                payload_length = (uint16_t)sdos_stats_encode(&runtime->stats, payload, SDOS_PAYLOAD_MAX);
            }
            break;
        case SDOS_MSG_RUN:
            status = run(runtime, body, header.payload_length, payload, &payload_length);
            break;
        default:
            status = SDOS_E_TYPE;
            break;
        }
    }
    if (status != SDOS_OK) {
        payload_length = 0u;
        runtime->stats.errors++;
        runtime->stats.last_error = (uint32_t)status;
    }
    reply.version_major = SDOS_PROTO_MAJOR;
    reply.version_minor = SDOS_PROTO_MINOR;
    reply.type = header.type;
    reply.flags = SDOS_FLAG_RESPONSE;
    reply.request_id = header.request_id;
    reply.payload_length = payload_length;
    reply.status = (uint16_t)status;
    return sdos_encode(&reply, payload, response, capacity);
}

static sdos_status alg_copy(const sdos_run_args *args, uint32_t *output_length)
{
    if (args->params_length != 0u) {
        return SDOS_E_FIELD;
    }
    if (args->input_length > args->output_capacity) {
        return SDOS_E_DESCRIPTOR;
    }
    memcpy(args->output, args->input, args->input_length);
    *output_length = args->input_length;
    return SDOS_OK;
}

static int16_t sample(const uint8_t *p)
{
    uint16_t raw = (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
    return raw >= 0x8000u ? (int16_t)((int32_t)raw - 0x10000) : (int16_t)raw;
}

static void store(uint8_t *p, int16_t value)
{
    uint16_t raw = (uint16_t)value;
    p[0] = (uint8_t)(raw & 0xFFu);
    p[1] = (uint8_t)(raw >> 8);
}

/* y = saturate((x * gain + 2^14) >> 15): Q15 gain with round-half-up. */
static sdos_status alg_gain_q15(const sdos_run_args *args, uint32_t *output_length)
{
    uint32_t i;
    int32_t gain;
    if (args->params_length != 2u || (args->input_length & 1u) != 0u) {
        return SDOS_E_FIELD;
    }
    if (args->input_length > args->output_capacity) {
        return SDOS_E_DESCRIPTOR;
    }
    gain = (int32_t)sample(args->params);
    for (i = 0u; i < args->input_length; i += 2u) {
        int32_t scaled = (int32_t)sample(args->input + i) * gain + 16384;
        /* floor(scaled / 2^15) without relying on signed right shifts */
        int32_t y = scaled >= 0 ? scaled >> 15 : -((-scaled + 32767) >> 15);
        if (y > 32767) {
            y = 32767;
        } else if (y < -32768) {
            y = -32768;
        }
        store(args->output + i, (int16_t)y);
    }
    *output_length = args->input_length;
    return SDOS_OK;
}

const sdos_algorithm sdos_builtin_algorithms[] = {
    {SDOS_ALG_COPY, "copy", alg_copy},
    {SDOS_ALG_GAIN_Q15, "gain_q15", alg_gain_q15},
};
const size_t sdos_builtin_algorithm_count = sizeof sdos_builtin_algorithms / sizeof sdos_builtin_algorithms[0];
