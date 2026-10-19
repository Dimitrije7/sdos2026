/* Protocol 2.0: golden encodings, invalid input, ownership table, health line, and fuzzing. */

#include <stdint.h>
#include <string.h>

#include "sdos/protocol.h"
#include "sdos/runtime.h"
#include "sdos_test.h"

static void test_golden_hello_request(void)
{
    /* The CRC was computed independently (bitwise CRC-32C in Python). */
    const uint8_t want[] = {0x53, 0x44, 2,    0,    0x01, 0x00, 0x00, 0x00, 0x04, 0x03,
                            0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x60, 0x5A, 0xEC, 0xF0};
    sdos_header h = {SDOS_PROTO_MAJOR, SDOS_PROTO_MINOR, SDOS_MSG_HELLO, 0u, 0x01020304u, 0u, 0u, 0u};
    uint8_t out[SDOS_MSG_MAX];
    CHECK_EQ("hello request is 20 bytes", sdos_encode(&h, NULL, out, sizeof out), 20);
    CHECK("hello request bytes are little-endian, fixed, and CRC-protected", memcmp(out, want, sizeof want) == 0);
}

static void test_header_rejections(void)
{
    sdos_header h = {SDOS_PROTO_MAJOR, SDOS_PROTO_MINOR, SDOS_MSG_ECHO, 0u, 7u, 4u, 0u, 0u};
    const uint8_t payload[4] = {1, 2, 3, 4};
    uint8_t msg[SDOS_MSG_MAX + 1u];
    sdos_header got;
    size_t n = sdos_encode(&h, payload, msg, sizeof msg);
    size_t bit;
    int undetected = 0;

    CHECK_EQ("echo encodes header plus payload", n, 24);
    CHECK_EQ("valid header decodes", sdos_decode_header(msg, n, &got), SDOS_OK);
    CHECK("decoded fields match", got.type == SDOS_MSG_ECHO && got.request_id == 7u && got.payload_length == 4u);
    CHECK_EQ("shorter than a header", sdos_decode_header(msg, 19u, &got), SDOS_E_LENGTH);
    CHECK_EQ("truncated payload fails the CRC", sdos_decode_header(msg, n - 1u, &got), SDOS_E_INTEGRITY);
    CHECK_EQ("longer than an RPMsg buffer", sdos_decode_header(msg, SDOS_MSG_MAX + 1u, &got), SDOS_E_LENGTH);
    /* Every single-bit error anywhere in the message is detected. */
    for (bit = 0u; bit < n * 8u; bit++) {
        sdos_status st;
        msg[bit / 8u] ^= (uint8_t)(1u << (bit % 8u));
        st = sdos_decode_header(msg, n, &got);
        if (st == SDOS_OK) {
            undetected++;
        }
        msg[bit / 8u] ^= (uint8_t)(1u << (bit % 8u));
    }
    CHECK_EQ("all single-bit errors are detected", undetected, 0);
    msg[0] = 'X';
    CHECK_EQ("bad magic", sdos_decode_header(msg, n, &got), SDOS_E_MAGIC);
    msg[0] = 0x53;
    h.version_major = 1u;
    n = sdos_encode(&h, payload, msg, sizeof msg);
    CHECK_EQ("other major version", sdos_decode_header(msg, n, &got), SDOS_E_VERSION);
    h.version_major = SDOS_PROTO_MAJOR;
    h.version_minor = 9u;
    n = sdos_encode(&h, payload, msg, sizeof msg);
    CHECK_EQ("newer minor version is accepted", sdos_decode_header(msg, n, &got), SDOS_OK);
    msg[6] = 0x02;
    CHECK_EQ("a changed flag without a new CRC is corruption", sdos_decode_header(msg, n, &got), SDOS_E_INTEGRITY);
    msg[6] = 0x00;
    msg[12] = 3u; /* payload length 3, CRC recomputed over what the length now claims */
    {
        uint32_t crc = sdos_crc32c(sdos_crc32c(0u, msg, 16u), msg + SDOS_HEADER_SIZE, n - SDOS_HEADER_SIZE);
        msg[16] = (uint8_t)crc;
        msg[17] = (uint8_t)(crc >> 8);
        msg[18] = (uint8_t)(crc >> 16);
        msg[19] = (uint8_t)(crc >> 24);
    }
    CHECK_EQ("length field disagrees with the message", sdos_decode_header(msg, n, &got), SDOS_E_LENGTH);

    h.payload_length = (uint16_t)(SDOS_PAYLOAD_MAX + 1u);
    CHECK_EQ("oversize payload is not encoded", sdos_encode(&h, msg, msg, sizeof msg), 0);
    h.payload_length = 4u;
    h.flags = 0x0002u;
    CHECK_EQ("unknown flag is not encoded", sdos_encode(&h, payload, msg, sizeof msg), 0);
    h.flags = 0u;
    CHECK_EQ("too small an output buffer", sdos_encode(&h, payload, msg, 23u), 0);
}

static void test_payload_round_trips(void)
{
    sdos_identity id = {SDOS_CORE_C71X_0, SDOS_RUNTIME_BAREMETAL, SDOS_CAP_ECHO | SDOS_CAP_RUN, 480u, 128u,
                        0x00010203u, 0x00100000u, "sdos-template"};
    sdos_identity id2;
    sdos_run_request rq;
    sdos_run_request rq2;
    sdos_run_result rr = {64u, 0xDEADBEEFu, 0x0000000500000006ull, 9u};
    sdos_run_result rr2;
    uint8_t buf[SDOS_PAYLOAD_MAX];

    CHECK_EQ("identity encodes to 48 bytes", sdos_identity_encode(&id, buf, sizeof buf), SDOS_IDENTITY_SIZE);
    CHECK_EQ("identity decodes", sdos_identity_decode(buf, SDOS_IDENTITY_SIZE, &id2), SDOS_OK);
    CHECK("identity round trip", id2.core == id.core && id2.runtime == id.runtime &&
                                     id2.capabilities == id.capabilities && id2.region_size == id.region_size &&
                                     strcmp(id2.firmware, "sdos-template") == 0);
    buf[2] = 1;
    CHECK_EQ("identity reserved bytes must be zero", sdos_identity_decode(buf, SDOS_IDENTITY_SIZE, &id2), SDOS_E_FIELD);
    buf[2] = 0;
    buf[47] = 'x';
    CHECK_EQ("unterminated firmware name", sdos_identity_decode(buf, SDOS_IDENTITY_SIZE, &id2), SDOS_E_FIELD);
    buf[47] = 0;
    buf[10] = 3;
    CHECK_EQ("alignment must be a power of two", sdos_identity_decode(buf, SDOS_IDENTITY_SIZE, &id2), SDOS_E_FIELD);

    memset(&rq, 0, sizeof rq);
    rq.algorithm = SDOS_ALG_GAIN_Q15;
    rq.input.offset = 0x100u;
    rq.input.length = 64u;
    rq.input.flags = SDOS_DESC_CHECKSUM;
    rq.input.checksum = 0x12345678u;
    rq.output.offset = 0x200u;
    rq.output.length = 64u;
    rq.output.sequence = 42u;
    rq.params_length = 2u;
    rq.params[0] = 0x00;
    rq.params[1] = 0x40;
    rq.budget_us = 1500u;
    CHECK_EQ("run request encodes fixed part plus params", sdos_run_request_encode(&rq, buf, sizeof buf), 62);
    CHECK_EQ("run request decodes", sdos_run_request_decode(buf, 62u, &rq2), SDOS_OK);
    CHECK("run request round trip", rq2.input.checksum == 0x12345678u && rq2.output.sequence == 42u &&
                                        rq2.params_length == 2u && rq2.params[1] == 0x40 && rq2.budget_us == 1500u);
    buf[6] = 0xFF; /* budget beyond the watchdog limit */
    CHECK_EQ("budget above the watchdog limit", sdos_run_request_decode(buf, 62u, &rq2), SDOS_E_FIELD);
    buf[6] = 0x00;
    rq.budget_us = SDOS_WATCHDOG_US + 1u;
    CHECK_EQ("such a budget is not encoded", sdos_run_request_encode(&rq, buf + 100, sizeof buf - 100u), 0);
    CHECK_EQ("params length must match", sdos_run_request_decode(buf, 61u, &rq2), SDOS_E_LENGTH);
    buf[8 + 22] = 1;
    CHECK_EQ("descriptor reserved bytes must be zero", sdos_run_request_decode(buf, 62u, &rq2), SDOS_E_FIELD);

    CHECK_EQ("run result encodes", sdos_run_result_encode(&rr, buf, sizeof buf), SDOS_RUN_RESULT_SIZE);
    CHECK_EQ("run result decodes", sdos_run_result_decode(buf, SDOS_RUN_RESULT_SIZE, &rr2), SDOS_OK);
    CHECK("64-bit cycles survive", rr2.cycles == 0x0000000500000006ull && rr2.output_checksum == 0xDEADBEEFu);
}

static void test_stats_round_trip(void)
{
    sdos_stats st;
    sdos_stats st2;
    uint8_t buf[SDOS_STATS_SIZE];
    memset(&st, 0, sizeof st);
    st.uptime_ms = 1u;
    st.runs = 6u;
    st.state = SDOS_STATE_FAULTED;
    st.fault = SDOS_FAULT_ACCESS;
    st.fault_info = 0x30u;
    st.fault_address = 0x00000001A7100000ull;
    st.heartbeat = 77u;
    st.deadline_misses = 2u;
    st.integrity_errors = 3u;
    st.foreign_messages = 4u;
    st.stack_margin = 0x1F00u;
    st.integrity_passes = 9u;
    CHECK_EQ("stats encode to 64 bytes", sdos_stats_encode(&st, buf, sizeof buf), SDOS_STATS_SIZE);
    CHECK_EQ("stats decode", sdos_stats_decode(buf, sizeof buf, &st2), SDOS_OK);
    CHECK("stats round trip", st2.fault_address == st.fault_address && st2.state == st.state &&
                                  st2.fault == st.fault && st2.fault_info == 0x30u && st2.heartbeat == 77u &&
                                  st2.deadline_misses == 2u && st2.integrity_errors == 3u &&
                                  st2.foreign_messages == 4u && st2.integrity_passes == 9u &&
                                  st2.stack_margin == 0x1F00u && st2.runs == 6u);
    buf[25] = (uint8_t)SDOS_FAULT_COUNT;
    CHECK_EQ("unknown fault code", sdos_stats_decode(buf, sizeof buf, &st2), SDOS_E_FIELD);
    buf[25] = 0u;
    buf[26] = 1u;
    CHECK_EQ("reserved stats bytes must be zero", sdos_stats_decode(buf, sizeof buf, &st2), SDOS_E_FIELD);
}

static void test_health_line(void)
{
    sdos_health h;
    sdos_health h2;
    char line[SDOS_HEALTH_SIZE + 1u];
    size_t i;
    int nul = 0;
    memset(&h, 0, sizeof h);
    h.sequence = 0x10u;
    h.heartbeat = 0xABCDEF01u;
    h.uptime_ms = 5000u;
    h.state = SDOS_STATE_HALTED;
    h.fault = SDOS_FAULT_EXCEPTION;
    h.fault_info = 0x80u;
    h.fault_address = 0x0000ABCD12345678ull;
    h.stack_margin = 4096u;
    h.integrity_passes = 3u;
    h.deadline_misses = 1u;
    sdos_health_format(&h, line);
    line[SDOS_HEALTH_SIZE] = '\0';
    for (i = 0u; i < SDOS_HEALTH_SIZE; i++) {
        nul += line[i] == '\0';
    }
    CHECK_EQ("the health line holds no NUL, so remoteproc's trace shows it whole", nul, 0);
    CHECK("the health line ends with a newline", line[SDOS_HEALTH_SIZE - 1u] == '\n');
    CHECK("the health line is readable text",
          strncmp(line, "SDOS-HEALTH 1 seq=00000010 hb=abcdef01 up=00001388 state=3 fault=05", 67) == 0);
    CHECK_EQ("the health line parses", sdos_health_parse(line, SDOS_HEALTH_SIZE, &h2), SDOS_OK);
    CHECK("health round trip", h2.heartbeat == h.heartbeat && h2.fault_address == h.fault_address &&
                                   h2.state == h.state && h2.fault == h.fault && h2.stack_margin == 4096u &&
                                   h2.deadline_misses == 1u);
    line[161] = '1'; /* end= differs from seq=: a torn read */
    CHECK_EQ("a torn health line is detected", sdos_health_parse(line, SDOS_HEALTH_SIZE, &h2), SDOS_E_STATE);
    CHECK_EQ("text without a health line", sdos_health_parse("[0.000] boot\n", 13u, &h2), SDOS_E_MAGIC);
    line[161] = 'x';
    CHECK_EQ("a malformed health line", sdos_health_parse(line, SDOS_HEALTH_SIZE, &h2), SDOS_E_FIELD);
}

static void test_descriptor_bounds(void)
{
    sdos_descriptor d = {0u, 0u, 0x1000u, 0u, 0u, 0u};
    CHECK_EQ("whole region", sdos_descriptor_check(&d, 0x1000u, 64u), SDOS_OK);
    d.offset = 0x40u;
    CHECK_EQ("one byte past the end", sdos_descriptor_check(&d, 0x1000u, 64u), SDOS_E_DESCRIPTOR);
    d.offset = 0xFFFFFFC0u;
    d.length = 0x80u;
    CHECK_EQ("offset plus length wraps", sdos_descriptor_check(&d, 0x1000u, 64u), SDOS_E_DESCRIPTOR);
    d.offset = 0x20u;
    d.length = 0x20u;
    CHECK_EQ("misaligned offset", sdos_descriptor_check(&d, 0x1000u, 64u), SDOS_E_DESCRIPTOR);
    d.offset = 0x40u;
    d.region = 1u;
    CHECK_EQ("only region 0 exists in 1.0", sdos_descriptor_check(&d, 0x1000u, 64u), SDOS_E_FIELD);
    d.region = 0u;
    CHECK_EQ("no region at all", sdos_descriptor_check(&d, 0u, 64u), SDOS_E_DESCRIPTOR);
}

static void test_ownership_table(void)
{
    /* want[state][event]: next state, or -1 where the step is refused. */
    static const int want[6][9] = {
        /* ACQUIRE SUBMIT ACCEPT COMPLETE HOST_COMPLETE RELEASE TIMEOUT RESTARTED REJECTED */
        {SDOS_BUF_HOST, -1, -1, -1, -1, -1, -1, SDOS_BUF_FREE, -1},                              /* FREE */
        {-1, SDOS_BUF_TO_DSP, -1, -1, -1, SDOS_BUF_FREE, -1, SDOS_BUF_HOST, -1},                 /* HOST */
        {-1, -1, SDOS_BUF_DSP, -1, -1, -1, SDOS_BUF_QUARANTINED, SDOS_BUF_FREE, SDOS_BUF_HOST},  /* TO_DSP */
        {-1, -1, -1, SDOS_BUF_TO_HOST, -1, -1, SDOS_BUF_QUARANTINED, SDOS_BUF_FREE, -1},         /* DSP */
        {-1, -1, -1, -1, SDOS_BUF_HOST, -1, SDOS_BUF_QUARANTINED, SDOS_BUF_FREE, -1},            /* TO_HOST */
        {-1, -1, -1, -1, -1, -1, -1, SDOS_BUF_FREE, -1},                                         /* QUARANTINED */
    };
    int mismatches = 0;
    int s;
    int e;
    for (s = 0; s < 6; s++) {
        for (e = 0; e < 9; e++) {
            sdos_buf_state next = SDOS_BUF_FREE;
            sdos_status st = sdos_buf_next((sdos_buf_state)s, (sdos_buf_event)e, &next);
            int got = st == SDOS_OK ? (int)next : -1;
            if (got != want[s][e]) {
                printf("  state %d event %d: got %d, expected %d\n", s, e, got, want[s][e]);
                mismatches++;
            }
        }
    }
    CHECK_EQ("ownership transitions match the specification table", mismatches, 0);
}

static void test_crc32c(void)
{
    const uint8_t check[] = "123456789";
    CHECK_EQ("CRC-32C check value", sdos_crc32c(0u, check, 9u), 0xE3069283u);
    CHECK_EQ("CRC-32C continues across calls", sdos_crc32c(sdos_crc32c(0u, check, 4u), check + 4, 5u), 0xE3069283u);
    CHECK_EQ("CRC-32C of nothing", sdos_crc32c(0u, check, 0u), 0u);
}

static uint32_t lcg(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state >> 8;
}

/* Every input gets a well-formed response that names the same request. */
static void test_fuzz_runtime_answers_everything(void)
{
    static uint8_t region[4096];
    static sdos_runtime rt;
    uint8_t msg[SDOS_MSG_MAX];
    uint8_t reply[SDOS_MSG_MAX];
    uint32_t seed = 12345u;
    int bad = 0;
    int i;
    rt.identity.buffer_alignment = 64u;
    rt.identity.region_size = sizeof region;
    (void)strcpy(rt.identity.firmware, "fuzz");
    rt.region = region;
    rt.region_size = sizeof region;
    rt.algorithms = sdos_builtin_algorithms;
    rt.algorithm_count = sdos_builtin_algorithm_count;
    sdos_runtime_ready(&rt);
    for (i = 0; i < 200000; i++) {
        size_t n = SDOS_HEADER_SIZE + (size_t)(lcg(&seed) % (SDOS_MSG_MAX - SDOS_HEADER_SIZE + 1u));
        size_t k;
        sdos_header h;
        for (k = 0; k < n; k++) {
            msg[k] = (uint8_t)lcg(&seed);
        }
        if ((i & 1) != 0) { /* half the inputs carry a valid header over random bodies */
            sdos_header req = {SDOS_PROTO_MAJOR, 0u, (uint16_t)(1u + lcg(&seed) % 5u), 0u, (uint32_t)i,
                               (uint16_t)(n - SDOS_HEADER_SIZE), 0u, 0u};
            (void)sdos_encode(&req, msg + SDOS_HEADER_SIZE, msg, sizeof msg);
        }
        k = sdos_runtime_handle(&rt, msg, n, reply, sizeof reply);
        if (k < SDOS_HEADER_SIZE || sdos_decode_header(reply, k, &h) != SDOS_OK ||
            (h.flags & SDOS_FLAG_RESPONSE) == 0u || h.status >= SDOS_STATUS_COUNT ||
            ((i & 1) != 0 && h.request_id != (uint32_t)i)) {
            bad++;
        }
    }
    CHECK_EQ("200000 random requests all get valid responses", bad, 0);
    CHECK("the fuzzer reached the RUN handler", rt.stats.runs > 0u || rt.stats.errors > 0u);
}

int main(void)
{
    test_golden_hello_request();
    test_header_rejections();
    test_payload_round_trips();
    test_stats_round_trip();
    test_health_line();
    test_descriptor_bounds();
    test_ownership_table();
    test_crc32c();
    test_fuzz_runtime_answers_everything();
    return TEST_EXIT();
}
