/* C66x adapter: cache attributes, cache maintenance, and the cycle counter.
 * The MAR settings follow TI's PDK example c66_cache_mar.c (BSD-3-Clause). */
#include <c6x.h>

#include <ti/osal/CacheP.h>

#include "memory_map_defines.inc" /* TI's J721E map, from the installed PDK */

#define SDOS_CORES_MACROS_ONLY
#include "sdos/cores.h"
#include "sdos/platform.h"

/* TI swaps the two C66x IPC pools; the manifest records the result. */
#if defined(BUILD_C66X_1)
#define IPC_DATA_BASE (C66x1_IPC_DATA_BASE)
#if (C66x1_IPC_DATA_BASE) != SDOS_C66X_0_DMA_POOL_BASE
#error "TI's C66x_0 IPC pool differs from manifest/dsp-cores.json"
#endif
#elif defined(BUILD_C66X_2)
#define IPC_DATA_BASE (C66x2_IPC_DATA_BASE)
#if (C66x2_IPC_DATA_BASE) != SDOS_C66X_1_DMA_POOL_BASE
#error "TI's C66x_1 IPC pool differs from manifest/dsp-cores.json"
#endif
#else
#error "build for BUILD_C66X_1 or BUILD_C66X_2"
#endif

#define CPU_HZ 1350000000u /* TDA4VM C66x clock */

void sdos_platform_init(void)
{
    TSCL = 0u; /* any write starts the time stamp counter */
    /* Vrings and IPC buffers are shared with the A72: never cache them. */
    CacheP_setMar((void *)(SHARED_DDR_SPACE_START), (uint32_t)(SHARED_DDR_SPACE_SIZE), CacheP_Mar_DISABLE);
    CacheP_setMar((void *)IPC_DATA_BASE, 0x01000000u, CacheP_Mar_DISABLE);
}

void sdos_cache_invalidate(void *address, size_t length)
{
    CacheP_Inv(address, (uint32_t)length);
}

void sdos_cache_writeback(void *address, size_t length)
{
    CacheP_wbInv(address, (uint32_t)length);
}

uint64_t sdos_cycles(void)
{
    uint32_t low = TSCL; /* reading TSCL latches TSCH */
    uint32_t high = TSCH;
    return ((uint64_t)high << 32) | low;
}

uint32_t sdos_uptime_ms(void)
{
    return (uint32_t)(sdos_cycles() / (CPU_HZ / 1000u));
}
