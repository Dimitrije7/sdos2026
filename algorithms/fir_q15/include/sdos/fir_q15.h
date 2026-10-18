#ifndef SDOS_FIR_Q15_H
#define SDOS_FIR_Q15_H

/* Q15 finite impulse response filter: the portable reference that DSP-optimized
 * versions must match bit for bit. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Filter `x` with the `taps` coefficients `h` and write the valid part of the
 * convolution, `x_len - taps + 1` samples, to `y`:
 *
 *     y[i] = sat16(round(sum_k x[i + k] * h[k] / 2^15))
 *
 * Products accumulate in 64 bits, so no tap count up to 2^32 can overflow.
 * Rounding adds 2^14 before the arithmetic shift; results saturate to
 * [-32768, 32767]. Returns the number of samples written, or 0 when any
 * pointer is NULL, `taps` is 0, or `x_len < taps`. */
size_t sdos_fir_q15(const int16_t *x, size_t x_len, const int16_t *h, size_t taps, int16_t *y);

#ifdef __cplusplus
}
#endif

#endif /* SDOS_FIR_Q15_H */
