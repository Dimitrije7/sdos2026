#include "sdos/fir_q15.h"

static int16_t saturate_q15(int64_t value)
{
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

size_t sdos_fir_q15(const int16_t *x, size_t x_len, const int16_t *h, size_t taps, int16_t *y)
{
    if (x == NULL || h == NULL || y == NULL || taps == 0U || x_len < taps) {
        return 0U;
    }
    const size_t outputs = x_len - taps + 1U;
    for (size_t i = 0U; i < outputs; ++i) {
        int64_t acc = 0;
        for (size_t k = 0U; k < taps; ++k) {
            acc += (int32_t)x[i + k] * (int32_t)h[k];
        }
        /* Arithmetic right shift of a negative value is implementation-defined
         * in C99; GCC, Clang, and the TI compilers all shift arithmetically. */
        y[i] = saturate_q15((acc + (1 << 14)) >> 15);
    }
    return outputs;
}
