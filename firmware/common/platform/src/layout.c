/* Layout self-check at boot: exercises the sections the linker files place.
 *
 * The Q15 FIR code runs from wherever fir_memory.cmd put it (by default copied
 * from DDR into L2 SRAM before main()), reads coefficients from .sdos.coeffs,
 * and works in .sdos.scratch_a. The trace records the result and the linker
 * symbols, so a board run shows that load/run initialization worked. */
#include <string.h>

#include "sdos/fir_q15.h"
#include "sdos/platform.h"

/* Defined by the default fir_memory.cmd; a layout without them reads zero. */
extern char sdos_fir_load_start[] __attribute__((weak));
extern char sdos_fir_run_start[] __attribute__((weak));
extern char sdos_fir_run_size[] __attribute__((weak));

/* Linker symbols are absolute addresses. Reading them through initialized
 * pointers uses absolute data relocations; direct references can compile to
 * PC-relative code that cannot reach a small absolute value on the C71x. */
static char *const fir_load = sdos_fir_load_start;
static char *const fir_run = sdos_fir_run_start;
static char *const fir_size = sdos_fir_run_size;

static const int16_t sdos_half_taps[2] __attribute__((section(".sdos.coeffs"))) = {16384, 16384};
static int16_t sdos_scratch_samples[8] __attribute__((section(".sdos.scratch_a")));

int sdos_layout_check(void)
{
    static const int16_t x[4] = {100, 300, -500, 701};
    static const int16_t want[3] = {200, -100, 101};
    int16_t *y = sdos_scratch_samples + 4;
    size_t n;
    memcpy(sdos_scratch_samples, x, sizeof x);
    n = sdos_fir_q15(sdos_scratch_samples, 4u, sdos_half_taps, 2u, y);
    sdos_trace("layout: fir code loaded at %p, runs at %p, %lu bytes; check %s", (void *)fir_load, (void *)fir_run,
               (unsigned long)(uintptr_t)fir_size,
               n == 3u && memcmp(y, want, sizeof want) == 0 ? "passed" : "FAILED");
    return n == 3u && memcmp(y, want, sizeof want) == 0 ? 0 : -1;
}
