/* DSP runtime behavior on the PC: every message type, error path, and cache hook. */

#include <stdint.h>
#include <string.h>

#include "sdos/protocol.h"
#include "sdos/runtime.h"
#include "sdos_test.h"

#define REGION 4096u

static uint8_t region[REGION];
static sdos_runtime rt;
static size_t invalidated_offset, invalidated_length, written_offset, written_length;
static uint64_t clock_now;

static void invalidate(void *address, size_t length)
{
    invalidated_offset = (size_t)((uint8_t *)address - region);
    invalidated_length = length;
}

static void writeback(void *address, size_t length)
{
    written_offset = (size_t)((uint8_t *)address - region);
    written_length = length;
}

static uint64_t cycles(void)
{
    clock_now += 1000u;
    return clock_now;
}

static void reset(void)
{
    memset(&rt, 0, sizeof rt);
    memset(region, 0, sizeof region);
    rt.identity.core = SDOS_CORE_C66X_0;
    rt.identity.runtime = SDOS_RUNTIME_FREERTOS;
    rt.identity.capabilities = SDOS_CAP_ECHO | SDOS_CAP_STATUS | SDOS_CAP_RUN | SDOS_CAP_CHECKSUM;
    rt.identity.max_payload = SDOS_PAYLOAD_MAX;
    rt.identity.buffer_alignment = 128u;
    rt.identity.region_size = REGION;
    (void)strcpy(rt.identity.firmware, "test-fw 1.0");
    rt.region = region;
    rt.region_size = REGION;
    rt.algorithms = sdos_builtin_algorithms;
    rt.algorithm_count = sdos_builtin_algorithm_count;
    rt.cache_invalidate = invalidate;
    rt.cache_writeback = writeback;
    rt.cycles = cycles;
}

/* Send one request; return the response status and copy its payload out. */
static sdos_status call(uint16_t type, const uint8_t *payload, uint16_t length, uint8_t *out, uint16_t *out_length)
{
    static uint32_t id = 100u;
    uint8_t msg[SDOS_MSG_MAX];
    uint8_t reply[SDOS_MSG_MAX];
    sdos_header h = {SDOS_PROTO_MAJOR, SDOS_PROTO_MINOR, type, 0u, ++id, length, 0u};
    size_t n = sdos_encode(&h, payload, msg, sizeof msg);
    size_t r = sdos_runtime_handle(&rt, msg, n, reply, sizeof reply);
    sdos_header got;
    if (r == 0u || sdos_decode_header(reply, r, &got) != SDOS_OK || got.request_id != id ||
        got.type != type || (got.flags & SDOS_FLAG_RESPONSE) == 0u) {
        return SDOS_E_TRANSPORT;
    }
    if (out != NULL) {
        memcpy(out, reply + SDOS_HEADER_SIZE, got.payload_length);
    }
    if (out_length != NULL) {
        *out_length = got.payload_length;
    }
    return (sdos_status)got.status;
}

static sdos_status run(uint32_t algorithm, sdos_descriptor in, sdos_descriptor out, const uint8_t *params,
                       uint16_t params_length, sdos_run_result *result)
{
    sdos_run_request rq;
    uint8_t payload[SDOS_PAYLOAD_MAX];
    uint8_t reply[SDOS_PAYLOAD_MAX];
    uint16_t reply_length = 0u;
    sdos_status st;
    memset(&rq, 0, sizeof rq);
    rq.algorithm = algorithm;
    rq.input = in;
    rq.output = out;
    rq.params_length = params_length;
    if (params_length > 0u) {
        memcpy(rq.params, params, params_length);
    }
    st = call(SDOS_MSG_RUN, payload, (uint16_t)sdos_run_request_encode(&rq, payload, sizeof payload), reply,
              &reply_length);
    if (st == SDOS_OK && result != NULL && sdos_run_result_decode(reply, reply_length, result) != SDOS_OK) {
        return SDOS_E_TRANSPORT;
    }
    return st;
}

static sdos_descriptor range(uint32_t offset, uint32_t length)
{
    sdos_descriptor d = {0u, offset, length, 0u, 0u, 0u};
    return d;
}

static void test_hello_echo_status(void)
{
    uint8_t out[SDOS_PAYLOAD_MAX];
    uint16_t n = 0u;
    sdos_identity id;
    sdos_stats stats;
    const uint8_t text[] = "hello dsp";

    reset();
    CHECK_EQ("hello succeeds", call(SDOS_MSG_HELLO, NULL, 0u, out, &n), SDOS_OK);
    CHECK_EQ("hello returns an identity", sdos_identity_decode(out, n, &id), SDOS_OK);
    CHECK("identity names the firmware", strcmp(id.firmware, "test-fw 1.0") == 0 && id.region_size == REGION);
    CHECK_EQ("hello takes no payload", call(SDOS_MSG_HELLO, text, 3u, NULL, NULL), SDOS_E_LENGTH);
    CHECK_EQ("echo succeeds", call(SDOS_MSG_ECHO, text, sizeof text, out, &n), SDOS_OK);
    CHECK("echo returns the payload unchanged", n == sizeof text && memcmp(out, text, sizeof text) == 0);
    CHECK_EQ("unknown type", call(99u, NULL, 0u, NULL, NULL), SDOS_E_TYPE);
    CHECK_EQ("status succeeds", call(SDOS_MSG_STATUS, NULL, 0u, out, &n), SDOS_OK);
    CHECK_EQ("status decodes", sdos_stats_decode(out, n, &stats), SDOS_OK);
    CHECK("status counts requests and errors", stats.requests == 5u && stats.errors == 2u &&
                                                    stats.last_error == SDOS_E_TYPE);
}

static void test_copy_with_checksums_and_cache(void)
{
    sdos_descriptor in = range(0u, 256u);
    sdos_descriptor out = range(1024u, 256u);
    sdos_run_result result;
    uint32_t i;

    reset();
    for (i = 0u; i < 256u; i++) {
        region[i] = (uint8_t)(i * 7u);
    }
    in.flags = SDOS_DESC_CHECKSUM;
    in.checksum = sdos_crc32c(0u, region, 256u);
    out.flags = SDOS_DESC_CHECKSUM;
    out.sequence = 77u;
    CHECK_EQ("copy runs", run(SDOS_ALG_COPY, in, out, NULL, 0u, &result), SDOS_OK);
    CHECK("copy output matches input", memcmp(region + 1024, region, 256u) == 0);
    CHECK("output checksum covers the output", result.output_checksum == in.checksum && result.output_length == 256u);
    CHECK_EQ("output sequence is echoed", result.output_sequence, 77);
    CHECK_EQ("cycles are measured", result.cycles, 1000);
    CHECK("input range was invalidated", invalidated_offset == 0u && invalidated_length == 256u);
    CHECK("output range was written back", written_offset == 1024u && written_length == 256u);

    in.checksum ^= 1u;
    CHECK_EQ("corrupted input is refused", run(SDOS_ALG_COPY, in, out, NULL, 0u, NULL), SDOS_E_CHECKSUM);
}

static void test_gain_q15(void)
{
    const int16_t x[] = {16384, -16384, 32767, -32768, 1, -1, 0};
    const int16_t want_half[] = {8192, -8192, 16384, -16384, 1, 0, 0};
    const uint8_t half[2] = {0x00, 0x40};  /* 0.5 in Q15 */
    const uint8_t minus_one[2] = {0x00, 0x80}; /* -1.0 in Q15 */
    sdos_run_result result;
    int ok = 1;
    size_t i;

    reset();
    for (i = 0u; i < 7u; i++) {
        region[2u * i] = (uint8_t)((uint16_t)x[i] & 0xFFu);
        region[2u * i + 1u] = (uint8_t)((uint16_t)x[i] >> 8);
    }
    CHECK_EQ("gain runs", run(SDOS_ALG_GAIN_Q15, range(0u, 14u), range(512u, 14u), half, 2u, &result), SDOS_OK);
    for (i = 0u; i < 7u; i++) {
        int16_t y = (int16_t)(uint16_t)(region[512u + 2u * i] | (uint16_t)(region[513u + 2u * i] << 8));
        ok = ok && y == want_half[i];
    }
    CHECK("gain 0.5 rounds half up", ok);
    CHECK_EQ("gain -1.0 runs", run(SDOS_ALG_GAIN_Q15, range(0u, 14u), range(512u, 14u), minus_one, 2u, NULL), SDOS_OK);
    CHECK("-1.0 * -32768 saturates to 32767", region[512u + 6u] == 0xFFu && region[512u + 7u] == 0x7Fu);
    CHECK_EQ("gain needs exactly one parameter", run(SDOS_ALG_GAIN_Q15, range(0u, 14u), range(512u, 14u), half, 1u, NULL),
             SDOS_E_FIELD);
}

static void test_run_refusals(void)
{
    reset();
    CHECK_EQ("unknown algorithm", run(1000u, range(0u, 16u), range(128u, 16u), NULL, 0u, NULL), SDOS_E_ALGORITHM);
    CHECK_EQ("overlapping input and output", run(SDOS_ALG_COPY, range(0u, 256u), range(128u, 256u), NULL, 0u, NULL),
             SDOS_E_DESCRIPTOR);
    CHECK_EQ("output past the region", run(SDOS_ALG_COPY, range(0u, 16u), range(REGION - 128u, 256u), NULL, 0u, NULL),
             SDOS_E_DESCRIPTOR);
    CHECK_EQ("misaligned input", run(SDOS_ALG_COPY, range(64u, 16u), range(256u, 16u), NULL, 0u, NULL),
             SDOS_E_DESCRIPTOR);
    CHECK_EQ("output smaller than input", run(SDOS_ALG_COPY, range(0u, 64u), range(256u, 32u), NULL, 0u, NULL),
             SDOS_E_DESCRIPTOR);
    rt.busy = 1;
    CHECK_EQ("a second request while busy", run(SDOS_ALG_COPY, range(0u, 16u), range(256u, 16u), NULL, 0u, NULL),
             SDOS_E_BUSY);
    rt.busy = 0;
    rt.region_size = 0u;
    CHECK_EQ("firmware without a region refuses buffers", run(SDOS_ALG_COPY, range(0u, 16u), range(256u, 16u), NULL, 0u,
                                                              NULL),
             SDOS_E_DESCRIPTOR);
}

static void test_responses_are_never_handled_as_requests(void)
{
    uint8_t msg[SDOS_MSG_MAX];
    uint8_t reply[SDOS_MSG_MAX];
    sdos_header h = {1u, 0u, SDOS_MSG_ECHO, SDOS_FLAG_RESPONSE, 5u, 0u, 0u};
    sdos_header got;
    size_t n;
    reset();
    n = sdos_runtime_handle(&rt, msg, sdos_encode(&h, NULL, msg, sizeof msg), reply, sizeof reply);
    CHECK("a response sent to the DSP is refused",
          sdos_decode_header(reply, n, &got) == SDOS_OK && got.status == SDOS_E_FIELD && got.request_id == 5u);
}

int main(void)
{
    test_hello_echo_status();
    test_copy_with_checksums_and_cache();
    test_gain_q15();
    test_run_refusals();
    test_responses_are_never_handled_as_requests();
    return TEST_EXIT();
}
