/* C66x adapter: cache attributes, memory protection, cache maintenance, and
 * the cycle counter.
 *
 * The MAR settings follow TI's PDK example c66_cache_mar.c (BSD-3-Clause).
 * Memory protection (docs/safety.md) has two parts:
 *   - the XMC's MPAX segments (TI's csl/arch/c66x/hw_xmc.h) admit only this
 *     core's DDR, its vring pool, and the device windows of
 *     manifest/dsp-cores.json; code is read-only, nothing else executable;
 *   - the L2 page attributes (TI's csl/arch/c66x/hw_dsp_icfg.h) make the L2
 *     code zone read-only, the rest of L2 non-executable, and admit no other
 *     bus master to L2 at all. */
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
#define OWN_MMIO_COUNT SDOS_C66X_0_MMIO_COUNT
#define OWN_MMIO_WINDOWS SDOS_C66X_0_MMIO_WINDOWS
#define SDOS_CORE_L2SRAM_BASE SDOS_C66X_0_L2SRAM_BASE
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
#define OWN_MMIO_COUNT SDOS_C66X_1_MMIO_COUNT
#define OWN_MMIO_WINDOWS SDOS_C66X_1_MMIO_WINDOWS
#define SDOS_CORE_L2SRAM_BASE SDOS_C66X_1_L2SRAM_BASE
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

/* L2 memory protection (hw_dsp_icfg.h at SOC_DSP_ICFG_BASE 0x01000000): one
 * attribute register per 16 KiB page of L2 (DSPICFG_L2_PGSZ). */
#define ICFG_REG(offset) (*(volatile uint32_t *)(0x01000000u + (offset)))
#define L2MPFAR ICFG_REG(0x84A000u)
#define L2MPFSR ICFG_REG(0x84A004u)
#define L2MPFCR ICFG_REG(0x84A008u)
#define L2MPPA(n) ICFG_REG(0x84A200u + 4u * (uint32_t)(n))
#define L2_PAGE 0x4000u
#define L2_PAGES 32u
#define MPPA_LOCAL 0x100u /* this CorePac may access; AID bits 9 to 15 stay 0: no other master may */
#define MPPA_CODE (MPPA_LOCAL | PERM_R | PERM_X)
#define MPPA_DATA (MPPA_LOCAL | PERM_R | PERM_W)
#define L2_FAULT_INFO 0x10000u /* marks an L2 page-protection fault in fault_info */

#define MSMC_PROBE 0x70020000u /* shared on-chip SRAM: outside every device window */

/* Protection zones from the linker files (docs/safety.md). */
extern char sdos_zone_code_start[];
extern char sdos_zone_code_end[];
extern char sdos_zone_rodata_start[];
extern char sdos_zone_rodata_end[];
extern char sdos_zone_l2code_start[];
extern char sdos_zone_l2code_end[];
extern char sdos_zone_l2_end[];
static char *const zone_code_start = sdos_zone_code_start;
static char *const zone_code_end = sdos_zone_code_end;
static char *const zone_rodata_start = sdos_zone_rodata_start;
static char *const zone_rodata_end = sdos_zone_rodata_end;
static char *const zone_l2code_start = sdos_zone_l2code_start;
static char *const zone_l2code_end = sdos_zone_l2code_end;
static char *const zone_l2_end = sdos_zone_l2_end;

/* Device windows from manifest/dsp-cores.json: {base, size}. */
typedef struct window {
    uint64_t base;
    uint64_t size;
} window;
static const window mmio[OWN_MMIO_COUNT] = OWN_MMIO_WINDOWS;

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

/* Add [start, end) clipped to [clip_start, clip_end) with `perms`; later
 * entries take priority, because a higher-numbered segment wins. */
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

/* The segment TI's start-up left covering an address, as found. */
typedef struct original {
    uint32_t low;
    uint64_t base;
    uint64_t end;
} original;

static original found_segment(uint32_t n)
{
    original o;
    uint32_t h = XMPAXH(n);
    o.low = XMPAXL(n);
    o.base = h & BADDR_MASK;
    o.end = (h & SEGSZ_MASK) != 0u ? o.base + ((uint64_t)1u << ((h & SEGSZ_MASK) + 1u)) : o.base;
    return o;
}

/* Expect a probe to be refused and recorded by `status`; clear the record. */
static int blocked(volatile uint32_t *status, volatile uint32_t *address, volatile uint32_t *clear, uint32_t probe)
{
    int ok = *status != 0u && (*address & 0x00FFFFFFu) == (probe & 0x00FFFFFFu);
    *clear = 1u;
    return ok;
}

static uint32_t protect_l2(sdos_runtime *runtime)
{
    const uint32_t base = SDOS_CORE_L2SRAM_BASE;
    const uint32_t code_lo = (uint32_t)(uintptr_t)zone_l2code_start;
    const uint32_t code_hi = (uint32_t)(uintptr_t)zone_l2code_end;
    const uint32_t end = (uint32_t)(uintptr_t)zone_l2_end;
    uint32_t page;
    uint32_t before = L2MPPA(0);

    for (page = 0u; page < L2_PAGES && base + (page + 1u) * L2_PAGE <= end; page++) {
        uint32_t at = base + page * L2_PAGE;
        L2MPPA(page) = at >= code_lo && at < code_hi ? MPPA_CODE : MPPA_DATA;
    }
    L2MPFCR = 1u;
    sdos_trace("l2: %lu pages protected (page 0 was %08lx); code 0x%08lx..0x%08lx read-only, the rest not "
               "executable, no other master admitted", (unsigned long)page, (unsigned long)before,
               (unsigned long)code_lo, (unsigned long)code_hi);
    if (code_hi > code_lo) {
        /* Self-test: a write into the L2 code zone must be refused. The line
         * leaves L1D first, so the write reaches the L2 controller. */
        volatile uint32_t *probe = (volatile uint32_t *)(uintptr_t)code_lo;
        uint32_t value = *probe;
        sdos_cache_writeback((void *)(uintptr_t)code_lo, 64u);
        *probe = value;
        _mfence(); /* the write has reached L2 before the status is read */
        if (!blocked(&L2MPFSR, &L2MPFAR, &L2MPFCR, code_lo)) {
            sdos_runtime_fault(runtime, SDOS_FAULT_SELF_TEST, code_lo, L2_FAULT_INFO);
            sdos_trace("l2: self-test FAILED: a write into the L2 code zone was not blocked");
            return 0u;
        }
    }
    return SDOS_CAP_PROTECTION;
}

uint32_t sdos_protection_init(sdos_runtime *runtime)
{
    const uint64_t ddr_start = OWN_DDR_BASE;
    const uint64_t ddr_end = (uint64_t)OWN_DDR_BASE + OWN_DDR_SIZE;
    const original low = found_segment(0u);
    const original high = found_segment(1u);
    unsigned int key;
    plan p;
    size_t i;

    sdos_trace("mpax: segment 0 %08lx %08lx, segment 1 %08lx %08lx (as found)", (unsigned long)XMPAXH(0),
               (unsigned long)low.low, (unsigned long)XMPAXH(1), (unsigned long)high.low);
    for (i = FIRST_SEGMENT; i < SEGMENTS; i++) {
        if ((XMPAXH(i) & SEGSZ_MASK) != 0u) { /* configured before us: record what is replaced */
            sdos_trace("mpax: segment %lu %08lx %08lx was enabled; replaced", (unsigned long)i,
                       (unsigned long)XMPAXH(i), (unsigned long)XMPAXL(i));
        }
    }
    p.count = 0u;
    /* Device windows first (lowest priority), then this core's DDR. */
    for (i = 0u; i < OWN_MMIO_COUNT; i++) {
        if (add(&p, mmio[i].base, mmio[i].base + mmio[i].size, 0u, (uint64_t)1u << 32, PERM_R | PERM_W) != 0) {
            p.count = SEGMENTS; /* over budget */
            break;
        }
    }
    if (p.count > SEGMENTS - FIRST_SEGMENT ||
        add(&p, ddr_start, ddr_end, ddr_start, ddr_end, PERM_R | PERM_W) != 0 ||
        add(&p, OWN_POOL_BASE, (uint64_t)OWN_POOL_BASE + OWN_POOL_SIZE, 0u, (uint64_t)1u << 32, PERM_R | PERM_W) != 0 ||
        add(&p, (uintptr_t)zone_rodata_start, (uintptr_t)zone_rodata_end, ddr_start, ddr_end, PERM_R) != 0 ||
        add(&p, (uintptr_t)zone_code_start, (uintptr_t)zone_code_end, ddr_start, ddr_end, PERM_R | PERM_X) != 0) {
        sdos_runtime_fault(runtime, SDOS_FAULT_PROTECTION, 0u, 2u);
        sdos_trace("mpax: the windows and zones need more than %u segments; protection not configured",
                   (unsigned)(SEGMENTS - FIRST_SEGMENT));
        return 0u;
    }
    /* Each grant keeps the translation of the segment that covered it. */
    for (i = 0u; i < p.count; i++) {
        const original *o = p.block[i].base >= high.base && p.block[i].base < high.end ? &high : &low;
        if (p.block[i].base < o->base || p.block[i].base >= o->end) {
            sdos_runtime_fault(runtime, SDOS_FAULT_PROTECTION, p.block[i].base, 1u);
            sdos_trace("mpax: no segment translates 0x%08lx; protection not configured",
                       (unsigned long)p.block[i].base);
            return 0u;
        }
    }
    key = _disable_interrupts();
    /* Grants first, so the code and data in use stay reachable throughout. */
    for (i = 0u; i < SEGMENTS - FIRST_SEGMENT; i++) {
        uint32_t n = FIRST_SEGMENT + (uint32_t)i;
        if (i < p.count) {
            const original *o = p.block[i].base >= high.base && p.block[i].base < high.end ? &high : &low;
            uint32_t offset = (uint32_t)(p.block[i].base - o->base);
            XMPAXL(n) = (((o->low & RADDR_MASK) + (offset >> 4)) & RADDR_MASK) | (o->low & 0xC0u) | p.perms[i];
            XMPAXH(n) = p.block[i].base | (p.block[i].size_log2 - 1u);
        } else {
            XMPAXH(n) = 0u; /* disabled */
        }
    }
    /* Then deny everything else, below and above 2 GiB. */
    XMPAXL(1) = high.low & ~PERM_MASK;
    XMPAXL(0) = low.low & ~PERM_MASK;
    XPFCMD = 1u; /* drop prefetched data fetched under the old permissions */
    (void)XPFACS;
    XMPFCR = 1u;
    _restore_interrupts(key);

    /* Self-tests: the other C66x's memory and shared on-chip SRAM must both
     * be refused and recorded. */
    (void)*(volatile uint32_t *)(uintptr_t)NEIGHBOUR_DDR;
    if (!blocked(&XMPFSR, &XMPFAR, &XMPFCR, NEIGHBOUR_DDR)) {
        sdos_runtime_fault(runtime, SDOS_FAULT_SELF_TEST, NEIGHBOUR_DDR, 1u);
        sdos_trace("mpax: self-test FAILED: a read of 0x%08lx was not blocked", (unsigned long)NEIGHBOUR_DDR);
        return 0u;
    }
    (void)*(volatile uint32_t *)(uintptr_t)MSMC_PROBE;
    if (!blocked(&XMPFSR, &XMPFAR, &XMPFCR, MSMC_PROBE)) {
        sdos_runtime_fault(runtime, SDOS_FAULT_SELF_TEST, MSMC_PROBE, 2u);
        sdos_trace("mpax: self-test FAILED: a read of 0x%08lx was not blocked", (unsigned long)MSMC_PROBE);
        return 0u;
    }
    sdos_trace("mpax: %lu segments; only this core's DDR (0x%08lx..0x%08lx), its vring pool, and %lu device "
               "windows reachable; code read-only; self-tests passed", (unsigned long)p.count,
               (unsigned long)ddr_start, (unsigned long)(ddr_end - 1u), (unsigned long)OWN_MMIO_COUNT);
    return protect_l2(runtime);
}

void sdos_protection_poll(sdos_runtime *runtime)
{
    uint32_t status = XMPFSR;
    if (status != 0u) {
        sdos_runtime_fault(runtime, SDOS_FAULT_ACCESS, XMPFAR, status);
        XMPFCR = 1u;
    }
    status = L2MPFSR; /* this core, or another master, against L2 page protection */
    if (status != 0u) {
        sdos_runtime_fault(runtime, SDOS_FAULT_ACCESS, L2MPFAR, L2_FAULT_INFO | (status & 0xFFFFu));
        L2MPFCR = 1u;
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
