/* Safety mechanisms on the PC: stack margins, the integrity record and its
 * incremental check, and the MMU and MPAX address planning. */

#include <stdint.h>
#include <string.h>

#include "sdos/protocol.h"
#include "sdos/safety.h"
#include "sdos_test.h"

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void test_stack_margin(void)
{
    static uint8_t stack[1024];
    sdos_stack s = {"test", stack, sizeof stack};
    sdos_stack_fill(stack, sizeof stack);
    CHECK_EQ("an unused stack has its whole size as margin", sdos_stack_unused(&s), 1024);
    memset(stack + 700, 0x11, 324); /* the top 324 bytes were used */
    CHECK_EQ("margin ends where use began", sdos_stack_unused(&s), 700);
    stack[100] = 0x00;
    CHECK_EQ("a write deep into the stack shrinks the margin", sdos_stack_unused(&s), 100);
    CHECK("that is inside the guard band", sdos_stack_unused(&s) < SDOS_STACK_GUARD);
}

/* A record over two host buffers, as ci/firmware_postlink.py writes it. */
static void make_record(uint8_t *record, const uint8_t *a, uint32_t a_length, const uint8_t *b, uint32_t b_length)
{
    uint32_t crc;
    memset(record, 0, SDOS_INTEGRITY_SIZE);
    put32(record, SDOS_INTEGRITY_MAGIC);
    put32(record + 4, SDOS_INTEGRITY_VERSION);
    put32(record + 8, 2u);
    put32(record + 16, (uint32_t)(uintptr_t)a);
    put32(record + 20, (uint32_t)((uint64_t)(uintptr_t)a >> 32));
    put32(record + 24, a_length);
    put32(record + 28, sdos_crc32c(0u, a, a_length));
    put32(record + 32, (uint32_t)(uintptr_t)b);
    put32(record + 36, (uint32_t)((uint64_t)(uintptr_t)b >> 32));
    put32(record + 40, b_length);
    put32(record + 44, sdos_crc32c(0u, b, b_length));
    crc = sdos_crc32c(0u, record, 12u);
    put32(record + 12, sdos_crc32c(crc, record + 16, 32u));
}

static void test_integrity(void)
{
    static uint8_t code[5000];
    static uint8_t rodata[300];
    static uint8_t record[SDOS_INTEGRITY_SIZE];
    sdos_integrity it;
    uint32_t failed = 99u;
    int result = 0;
    int steps = 0;
    size_t i;

    for (i = 0u; i < sizeof code; i++) {
        code[i] = (uint8_t)(i * 13u);
    }
    memset(rodata, 0x5A, sizeof rodata);
    make_record(record, code, sizeof code, rodata, sizeof rodata);
    CHECK_EQ("a valid record loads", sdos_integrity_load(&it, record, sizeof record), SDOS_OK);
    CHECK_EQ("two ranges", it.count, 2);
    while (result == 0 && steps < 100) {
        result = sdos_integrity_step(&it, 1024u, &failed);
        steps++;
    }
    CHECK_EQ("a clean pass completes", result, 1);
    CHECK_EQ("in 1 KiB steps it takes six steps", steps, 6);
    CHECK_EQ("the pass is counted", it.passes, 1);

    code[4321] ^= 0x04u; /* one flipped bit */
    result = 0;
    while (result == 0) {
        result = sdos_integrity_step(&it, 4096u, &failed);
    }
    CHECK_EQ("a flipped bit is found", result, -1);
    CHECK_EQ("in the range that holds it", failed, 0);
    code[4321] ^= 0x04u;

    rodata[0] = 0;
    result = 0;
    while (result == 0) {
        result = sdos_integrity_step(&it, 100000u, &failed);
    }
    CHECK("a change in the second range is found", result == -1 && failed == 1u);
    rodata[0] = 0x5A;

    record[16 + 24] ^= 1u; /* damage the record itself */
    CHECK_EQ("a damaged record is refused", sdos_integrity_load(&it, record, sizeof record), SDOS_E_CHECKSUM);
    memset(record + 8, 0, 8u);
    CHECK_EQ("an unfilled record is refused", sdos_integrity_load(&it, record, sizeof record), SDOS_E_STATE);
    record[0] = 0;
    CHECK_EQ("no record", sdos_integrity_load(&it, record, sizeof record), SDOS_E_MAGIC);
    put32(record, SDOS_INTEGRITY_MAGIC);
    put32(record + 8, SDOS_INTEGRITY_MAX + 1u);
    CHECK_EQ("too many ranges", sdos_integrity_load(&it, record, sizeof record), SDOS_E_FIELD);
}

static void test_span_subtract(void)
{
    /* The C71x data zone with two unmapped stack guard pages. */
    const sdos_span data = {0xA8B00000u, 0xA9000000u};
    const sdos_span holes[3] = {{0xA8C60000u, 0xA8C61000u}, {0xA8B40000u, 0xA8B41000u}, {0xA9000000u, 0xA9001000u}};
    sdos_span out[4];
    sdos_span small[2];
    size_t n = sdos_span_subtract(data, holes, 3u, out, 4u);
    CHECK_EQ("two holes split the zone in three", n, 3);
    CHECK("pieces in address order around the holes",
          out[0].start == 0xA8B00000u && out[0].end == 0xA8B40000u && out[1].start == 0xA8B41000u &&
              out[1].end == 0xA8C60000u && out[2].start == 0xA8C61000u && out[2].end == 0xA9000000u);
    CHECK_EQ("too little room is reported", sdos_span_subtract(data, holes, 3u, small, 2u), SIZE_MAX);
    CHECK_EQ("no holes leave the zone whole", sdos_span_subtract(data, NULL, 0u, out, 4u), 1);
    {
        const sdos_span all[1] = {{0xA8000000u, 0xAA000000u}};
        CHECK_EQ("a hole covering the zone leaves nothing", sdos_span_subtract(data, all, 1u, out, 4u), 0);
    }
}

static void test_mpax_split(void)
{
    sdos_mpax_block b[16];
    size_t n = sdos_mpax_split(0xA6100000u, 0xA7000000u, b, 16u);
    /* C66x_0's carve-out: 1 + 2 + 4 + 8 MiB. */
    CHECK_EQ("the 15 MiB carve-out takes four segments", n, 4);
    CHECK("segments are naturally aligned powers of two",
          b[0].base == 0xA6100000u && b[0].size_log2 == 20u && b[1].base == 0xA6200000u && b[1].size_log2 == 21u &&
              b[2].base == 0xA6400000u && b[2].size_log2 == 22u && b[3].base == 0xA6800000u &&
              b[3].size_log2 == 23u);
    CHECK_EQ("an aligned 4 GiB span is one segment", sdos_mpax_split(0u, (uint64_t)1u << 32, b, 16u), 1);
    CHECK_EQ("misaligned spans are refused", sdos_mpax_split(0xA6100800u, 0xA6200000u, b, 16u), SIZE_MAX);
    CHECK_EQ("too few segments are reported", sdos_mpax_split(0xA6101000u, 0xA7000000u, b, 4u), SIZE_MAX);
    CHECK_EQ("an empty span needs none", sdos_mpax_split(0xA6100000u, 0xA6100000u, b, 16u), 0);
}

int main(void)
{
    test_stack_margin();
    test_integrity();
    test_span_subtract();
    test_mpax_split();
    return TEST_EXIT();
}
