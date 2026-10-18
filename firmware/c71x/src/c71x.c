/* C71x adapter: MMU, cache maintenance, and the cycle counter.
 * The MMU map follows TI's PDK example c7x_mmu.c (BSD-3-Clause). */
#include <c7x.h>

#include <ti/osal/CacheP.h>
#include <ti/osal/soc/osal_soc.h>
#include "Mmu.h" /* FreeRTOS C7x port, from the installed PDK */

#include "memory_map_defines.inc"

#define SDOS_CORES_MACROS_ONLY
#include "sdos/cores.h"
#include "sdos/platform.h"

#if (C7x_1_IPC_DATA_BASE) != SDOS_C71X_0_DMA_POOL_BASE
#error "TI's C71x_0 IPC pool differs from manifest/dsp-cores.json"
#endif

#define CPU_HZ 1000000000u /* TDA4VM C71x clock */
#define MSMC_SIZE 0x00800000u

static void map(bool secure)
{
    Mmu_MapAttrs attrs;
    Mmu_initMapAttrs(&attrs);
    attrs.ns = secure ? 0 : 1;
    attrs.attrIndx = Mmu_AttrIndx_MAIR0; /* device memory */
    (void)Mmu_map(0x00000000u, 0x00000000u, 0x20000000u, &attrs, secure);
    (void)Mmu_map(0x20000000u, 0x20000000u, 0x20000000u, &attrs, secure);
    (void)Mmu_map(0x40000000u, 0x40000000u, 0x20000000u, &attrs, secure);
    (void)Mmu_map(0x60000000u, 0x60000000u, 0x10000000u, &attrs, secure);
    (void)Mmu_map(0x78000000u, 0x78000000u, 0x08000000u, &attrs, secure); /* CLEC */
    attrs.attrIndx = Mmu_AttrIndx_MAIR7; /* cached memory */
    (void)Mmu_map(0x80000000u, 0x80000000u, 0x20000000u, &attrs, secure);
    (void)Mmu_map(0xA0000000u, 0xA0000000u, 0x20000000u, &attrs, secure);
    (void)Mmu_map(0x70000000u, 0x70000000u, MSMC_SIZE, &attrs, secure);
    (void)Mmu_map(0x41C00000u, 0x41C00000u, 0x00100000u, &attrs, secure); /* OCMC */
    attrs.attrIndx = Mmu_AttrIndx_MAIR4; /* uncached: vrings and IPC buffers */
    (void)Mmu_map(SHARED_DDR_SPACE_START, SHARED_DDR_SPACE_START, 0x02000000u, &attrs, secure);
    (void)Mmu_map(C7x_1_IPC_DATA_BASE, C7x_1_IPC_DATA_BASE, 0x00100000u, &attrs, secure);
}

/* Called by the FreeRTOS C7x port before main(). */
void InitMmu(void);
void InitMmu(void)
{
    map(false);
    map(true);
    OsalCfgClecAccessCtrl(false);
}

void sdos_platform_init(void)
{
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
    return (uint64_t)__TSC;
}

uint32_t sdos_uptime_ms(void)
{
    return (uint32_t)(sdos_cycles() / (CPU_HZ / 1000u));
}
