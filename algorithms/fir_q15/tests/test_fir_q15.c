/* Unit tests for the Q15 FIR reference. */

#include <stdio.h>
#include <string.h>

#include "sdos/fir_q15.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define Q15_ONE INT16_MAX
#define Q15_HALF ((int16_t)16384)

static int failures;

static void expect_samples(const char *name, const int16_t *got, const int16_t *want, size_t n)
{
    for (size_t i = 0U; i < n; ++i) {
        if (got[i] != want[i]) {
            printf("FAIL %s: y[%zu] = %d, expected %d\n", name, i, got[i], want[i]);
            ++failures;
            return;
        }
    }
    printf("ok   %s\n", name);
}

static void expect_count(const char *name, size_t got, size_t want)
{
    if (got != want) {
        printf("FAIL %s: returned %zu, expected %zu\n", name, got, want);
        ++failures;
        return;
    }
    printf("ok   %s\n", name);
}

static void test_impulse_reproduces_coefficients(void)
{
    const int16_t h[] = {1000, -2000, 3000};
    const int16_t x[] = {0, 0, Q15_ONE, 0, 0};
    int16_t y[3];
    /* x * h with x = 32767/32768 rounds each coefficient back to itself. */
    const int16_t want[] = {3000, -2000, 1000};
    expect_count("impulse length", sdos_fir_q15(x, COUNT(x), h, COUNT(h), y), COUNT(y));
    expect_samples("impulse response", y, want, COUNT(want));
}

static void test_moving_average(void)
{
    const int16_t h[] = {Q15_HALF, Q15_HALF};
    const int16_t x[] = {100, 300, -500, 701};
    int16_t y[3];
    /* (100+300)/2 = 200, (300-500)/2 = -100, (-500+701)/2 = 100.5 rounds up to 101 */
    const int16_t want[] = {200, -100, 102}; /* deliberately wrong */
    sdos_fir_q15(x, COUNT(x), h, COUNT(h), y);
    expect_samples("moving average with rounding", y, want, COUNT(want));
}

static void test_saturation(void)
{
    const int16_t h[] = {Q15_ONE, Q15_ONE, Q15_ONE, Q15_ONE};
    const int16_t up[] = {Q15_ONE, Q15_ONE, Q15_ONE, Q15_ONE};
    const int16_t down[] = {INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN};
    int16_t y[1];
    sdos_fir_q15(up, COUNT(up), h, COUNT(h), y);
    expect_samples("positive saturation", y, (const int16_t[]){INT16_MAX}, 1U);
    sdos_fir_q15(down, COUNT(down), h, COUNT(h), y);
    expect_samples("negative saturation", y, (const int16_t[]){INT16_MIN}, 1U);
}

static void test_long_filter_does_not_overflow(void)
{
    /* 70000 products of 2^30 exceed 32 bits; the 64-bit accumulator holds them
     * and the cancelling halves sum to zero. */
    enum { TAPS = 70000 };
    static int16_t x[TAPS];
    static int16_t h[TAPS];
    for (size_t k = 0U; k < TAPS; ++k) {
        x[k] = INT16_MIN;
        h[k] = (k < TAPS / 2U) ? INT16_MIN : Q15_ONE;
    }
    int16_t y[1];
    sdos_fir_q15(x, TAPS, h, TAPS, y);
    /* 35000 * 2^30 - 35000 * 32768 * 32767 = 35000 * 32768; >> 15 gives 35000,
     * which saturates. A 32-bit accumulator would have wrapped long before. */
    expect_samples("64-bit accumulation", y, (const int16_t[]){INT16_MAX}, 1U);
}

static void test_invalid_arguments(void)
{
    const int16_t h[] = {1, 2, 3};
    const int16_t x[] = {1, 2};
    int16_t y[2];
    memset(y, 0x5a, sizeof(y));
    expect_count("input shorter than filter", sdos_fir_q15(x, COUNT(x), h, COUNT(h), y), 0U);
    expect_count("zero taps", sdos_fir_q15(x, COUNT(x), h, 0U, y), 0U);
    expect_count("null output", sdos_fir_q15(x, COUNT(x), h, 1U, NULL), 0U);
    expect_count("output untouched on error", (size_t)(y[0] == (int16_t)0x5a5a), 1U);
}

int main(void)
{
    test_impulse_reproduces_coefficients();
    test_moving_average();
    test_saturation();
    test_long_filter_does_not_overflow();
    test_invalid_arguments();
    printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
