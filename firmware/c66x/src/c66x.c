/* C66x adapter: cache attributes, memory protection, cache maintenance, and
 * the cycle counter.
 *
 * The MAR settings follow TI's PDK example c66_cache_mar.c (BSD-3-Clause).
 * Memory protection uses the XMC's MPAX segments (register layout from TI's
 * csl/arch/c66x/hw_xmc.h); docs/safety.md describes the partition. */
#include <c6x.h>

#include <ti/osal/CacheP.h>

#include "memory_map_defines.inc" /* TI's J721E map, from the installed PDK */

#define SDOS_CORES_MACROS_ONLY
#include "sdos/cores.h"
#include "sdos/platform.h"
#include "sdos/safety.h"

/* TI swaps the two C66x IPC pools; the manifest records the result. */
#if defined(BUILD_C66X_1)
#define IPC_DATA_BASE (C66x1_IPC_DATA_BASE)
#if (C66x1_IPC_DATA_BASE) != SDOS_C66X_0_DMA_POOL_BASE
#error "TI's C66x_0 IPC pool differs from manifest/dsp-cores.json"
#endif
#define OWN_DDR_BASE SDOS_C66X_0_DDR_BASE
#define OWN_DDR_SIZE SDOS_C66X_0_DDR_SIZE
#define OWN_POOL_BASE SDOS_C66X_0_DMA_POOL_BASE
#define OWN_POOL_SIZE SDOS_C66X_0_DMA_POOL_SIZE
#define NEIGHBOUR_DDR SDOS_C66X_1_DDR_BASE /* self-test target: never ours */
#elif defined(BUILD_C66X_2)
#define IPC_DATA_BASE (C66x2_IPC_DATA_BASE)
#if (C66x2_IPC_DATA_BASE) != SDOS_C66X_1_DMA_POOL_BASE
#error "TI's C66x_1 IPC pool differs from manifest/dsp-cores.json"
#endif
#define OWN_DDR_BASE SDOS_C66X_1_DDR_BASE
#define OWN_DDR_SIZE SDOS_C66X_1_DDR_SIZE
#define OWN_POOL_BASE SDOS_C66X_1_DMA_POOL_BASE
#define OWN_POOL_SIZE SDOS_C66X_1_DMA_POOL_SIZE
#define NEIGHBOUR_DDR SDOS_C66X_0_DDR_BASE
#else
#error "build for BUILD_C66X_1 or BUILD_C66X_2"
#endif

#define CPU_HZ 1350000000u /* TDA4VM C66x clock */

/* XMC registers (hw_xmc.h, SOC_DSP_XMC_CTRL_BASE). */
#define XMC_BASE 0x08000000u
#define XMC_REG(offset) (*(volatile uint32_t *)(XMC_BASE + (offset)))
#define XMPAXL(n) XMC_REG(8u * (uint32_t)(n))
#define XMPAXH(n) XMC_REG(8u * (uint32_t)(n) + 4u)
#define XMPFAR XMC_REG(0x200u)
#define XMPFSR XMC_REG(0x204u)
#define XMPFCR XMC_REG(0x208u)
#define XPFCMD XMC_REG(0x300u)
#define XPFACS XMC_REG(0x304u)
#define RADDR_MASK 0xFFFFFF00u
#define BADDR_MASK 0xFFFFF000u
#define SEGSZ_MASK 0x1Fu
#define PERM_MASK 0x3Fu
#define PERM_R 0x24u /* UR | SR */
#define PERM_W 0x12u /* UW | SW */
#define PERM_X 0x09u /* UX | SX */
#define FIRST_SEGMENT 2u
#define SEGMENTS 16u

/* Protection zones from the linker files (docs/safety.md). */
extern char sdos_zone_code_start[];
extern char sdos_zone_code_end[];
extern char sdos_zone_rodata_start[];
extern char sdos_zone_rodata_end[];
static char *const zone_code_start = sdos_zone_code_start;
static char *const zone_code_end = sdos_zone_code_end;
static char *const zone_rodata_start = sdos_zone_rodata_start;
static char *const zone_rodata_end = sdos_zone_rodata_end;

void sdos_platform_init(void)
{
    TSCL = 0u; /* any write starts the time stamp counter */
    /* Vrings and IPC buffers are shared with the A72: never cache them. */
    CacheP_setMar((void *)(SHARED_DDR_SPACE_START), (uint32_t)(SHARED_DDR_SPACE_SIZE), CacheP_Mar_DISABLE);
    CacheP_setMar((void *)IPC_DATA_BASE, 0x01000000u, CacheP_Mar_DISABLE);
}

typedef struct plan {
    sdos_mpax_block block[SEGMENTS - FIRST_SEGMENT];
    uint32_t perms[SEGMENTS - FIRST_SEGMENT];
    size_t count;
} plan;

/* Add [start, end) clipped to the carve-out with `perms`; later entries take
 * priority, because a higher-numbered MPAX segment wins where they overlap. */
static int add(plan *p, uint64_t start, uint64_t end, uint64_t clip_start, uint64_t clip_end, uint32_t perms)
{
    size_t n;
    size_t i;
    start = start > clip_start ? start : clip_start;
    end = end < clip_end ? end : clip_end;
    if (end <= start) {
        return 0;
    }
    n = sdos_mpax_split(start, end, p->block + p->count, (SEGMENTS - FIRST_SEGMENT) - p->count);
    if (n == SIZE_MAX) {
        return -1;
    }
    for (i = 0u; i < n; i++) {
        p->perms[p->count + i] = perms;
    }
    p->count += n;
    return 0;
}

uint32_t sdos_protection_init(sdos_runtime *runtime)
{
    const uint64_t ddr_start = OWN_DDR_BASE;
    const uint64_t ddr_end = (uint64_t)OWN_DDR_BASE + OWN_DDR_SIZE;
    uint32_t l0 = XMPAXL(0);
    uint32_t l1 = XMPAXL(1);
    uint32_t h1 = XMPAXH(1);
    uint64_t base1 = h1 & BADDR_MASK;
    uint64_t end1 = (h1 & SEGSZ_MASK) != 0u ? base1 + ((uint64_t)1u << ((h1 & SEGSZ_MASK) + 1u)) : base1;
    plan p;
    size_t i;
    uint32_t status;

    sdos_trace("mpax: segment 0 %08lx %08lx, segment 1 %08lx %08lx (as found)", (unsigned long)XMPAXH(0),
               (unsigned long)l0, (unsigned long)h1, (unsigned long)l1);
    for (i = FIRST_SEGMENT; i < SEGMENTS; i++) {
        if ((XMPAXH(i) & SEGSZ_MASK) != 0u) { /* configured before us: record what is replaced */
            sdos_trace("mpax: segment %lu %08lx %08lx was enabled; replaced", (unsigned long)i,
                       (unsigned long)XMPAXH(i), (unsigned long)XMPAXL(i));
        }
    }
    /* Every grant is translated the way segment 1 translates DDR. */
    if (ddr_start < base1 || ddr_end > end1 || OWN_POOL_BASE < base1 ||
        (uint64_t)OWN_POOL_BASE + OWN_POOL_SIZE > end1) {
        sdos_runtime_fault(runtime, SDOS_FAULT_PROTECTION, base1, 1u);
        sdos_trace("mpax: segment 1 does not cover this core's DDR; protection not configured");
        return 0u;
    }
    p.count = 0u;
    if (add(&p, ddr_start, ddr_end, ddr_start, ddr_end, PERM_R | PERM_W) != 0 ||
        add(&p, OWN_POOL_BASE, (uint64_t)OWN_POOL_BASE + OWN_POOL_SIZE, 0u, (uint64_t)1u << 32, PERM_R | PERM_W) != 0 ||
        add(&p, (uintptr_t)zone_rodata_start, (uintptr_t)zone_rodata_end, ddr_start, ddr_end, PERM_R) != 0 ||
        add(&p, (uintptr_t)zone_code_start, (uintptr_t)zone_code_end, ddr_start, ddr_end, PERM_R | PERM_X) != 0) {
        sdos_runtime_fault(runtime, SDOS_FAULT_PROTECTION, 0u, 2u);
        sdos_trace("mpax: the zones need more than %u segments; protection not configured",
                   (unsigned)(SEGMENTS - FIRST_SEGMENT));
        return 0u;
    }
    /* Grants first, so the code running now stays executable throughout. */
    for (i = 0u; i < SEGMENTS - FIRST_SEGMENT; i++) {
        uint32_t n = FIRST_SEGMENT + (uint32_t)i;
        if (i < p.count) {
            uint32_t offset = (uint32_t)(p.block[i].base - base1);
            XMPAXL(n) = (((l1 & RADDR_MASK) + (offset >> 4)) & RADDR_MASK) | (l1 & 0xC0u) | p.perms[i];
            XMPAXH(n) = p.block[i].base | (p.block[i].size_log2 - 1u);
        } else {
            XMPAXH(n) = 0u; /* disabled */
        }
    }
    /* Then deny the rest of DDR, and execution from everything below it. */
    XMPAXL(1) = l1 & ~PERM_MASK;
    XMPAXL(0) = l0 & ~PERM_X;
    XPFCMD = 1u; /* drop prefetched data fetched under the old permissions */
    (void)XPFACS;
    XMPFCR = 1u;

    /* Self-test: reading the other C66x's memory must be blocked and recorded. */
    (void)*(volatile uint32_t *)(uintptr_t)NEIGHBOUR_DDR;
    status = XMPFSR;
    if (status == 0u || XMPFAR != NEIGHBOUR_DDR) {
        sdos_runtime_fault(runtime, SDOS_FAULT_SELF_TEST, XMPFAR, status);
        sdos_trace("mpax: self-test FAILED: a read of 0x%08lx was not blocked", (unsigned long)NEIGHBOUR_DDR);
        return 0u;
    }
    XMPFCR = 1u;
    sdos_trace("mpax: %lu segments; DDR outside 0x%08lx..0x%08lx and the vring pool denied, code read-only; "
               "self-test passed", (unsigned long)p.count, (unsigned long)ddr_start, (unsigned long)(ddr_end - 1u));
    return SDOS_CAP_PROTECTION;
}

void sdos_protection_poll(sdos_runtime *runtime)
{
    uint32_t status = XMPFSR;
    if (status != 0u) {
        sdos_runtime_fault(runtime, SDOS_FAULT_ACCESS, XMPFAR, status);
        XMPFCR = 1u;
    }
}

void sdos_protection_guard(const sdos_stack *stacks, size_t count)
{
    (void)stacks;
    (void)count; /* no MMU: the monitor's fill check guards C66x stacks */
}

void sdos_halt(void)
{
    (void)_disable_interrupts();
    for (;;) {
        /* fatal fault: Linux restarts the core */
    }
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

uint32_t sdos_cycles_per_us(void)
{
    return CPU_HZ / 1000000u;
}

uint32_t sdos_uptime_ms(void)
{
    return (uint32_t)(sdos_cycles() / (CPU_HZ / 1000u));
}
