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
#define TICK_TIMER_BASE 0x02400000u /* DM timer 0: OSAL_FREERTOS_OS_TIMER_ID_C66X_1 */
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
#define TICK_TIMER_BASE 0x02410000u /* DM timer 1: OSAL_FREERTOS_OS_TIMER_ID_C66X_2 */
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

/* Memory protection lock (hw_dsp_icfg.h, DSP_MPLK*): with the key written,
 * LOCK makes the protection registers read-only until UNLOCK with the same
 * key. The key only guards against accidental writes; it is not a secret. */
#define MPLK(n) ICFG_REG(0x84AD00u + 4u * (uint32_t)(n))
#define MPLKCMD ICFG_REG(0x84AD10u)
#define MPLKSTAT ICFG_REG(0x84AD14u)
#define MPLK_UNLOCK 0x1u
#define MPLK_LOCK 0x2u
#define MPLK_KEYR 0x4u
#define MPLK_LOCKED 0x1u
#define PROTECTION_CHANGED 0x400u /* fault_info: a protection register differs from its snapshot */

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

/* DM timer registers (TI osal TimerP_nonos.c, TIMERP_DM_*). */
#define TIMER_REG(offset) (*(volatile uint32_t *)(TICK_TIMER_BASE + (offset)))
#define TIMER_TIOCP_CFG 0x10u
#define TIMER_SOFTRESET 0x1u

void sdos_tick_timer_reset(void)
{
    uint32_t spins = 100000u; /* the reset completes within a few timer clocks */
    TIMER_REG(TIMER_TIOCP_CFG) = TIMER_SOFTRESET;
    while ((TIMER_REG(TIMER_TIOCP_CFG) & TIMER_SOFTRESET) != 0u && spins > 0u) {
        spins--;
    }
}

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

/* Expect a probe to be refused and recorded by `status`; clear the record.
 * The raw registers are traced either way: the board evidence for how this
 * XMC reports a refused access (docs/safety.md). */
static int blocked(const char *what, volatile uint32_t *status, volatile uint32_t *address, volatile uint32_t *clear,
                   uint32_t probe, uint32_t value)
{
    uint32_t s = *status;
    uint32_t a = *address;
    int ok = s != 0u && (a & 0x00FFFFFFu) == (probe & 0x00FFFFFFu);
    *clear = 1u;
    sdos_trace("%s: probe 0x%08lx read %08lx; fault status %08lx, address %08lx: %s", what, (unsigned long)probe,
               (unsigned long)value, (unsigned long)s, (unsigned long)a, ok ? "refused" : "NOT refused");
    return ok;
}

void sdos_protection_report(void)
{
    uint32_t i;
    for (i = 0u; i < SEGMENTS; i += 2u) {
        sdos_trace("mpax: %2lu %08lx %08lx  %2lu %08lx %08lx", (unsigned long)i, (unsigned long)XMPAXH(i),
                   (unsigned long)XMPAXL(i), (unsigned long)(i + 1u), (unsigned long)XMPAXH(i + 1u),
                   (unsigned long)XMPAXL(i + 1u));
    }
    sdos_trace("mpax: fault status %08lx address %08lx; l2: fault status %08lx address %08lx", (unsigned long)XMPFSR,
               (unsigned long)XMPFAR, (unsigned long)L2MPFSR, (unsigned long)L2MPFAR);
}

static uint32_t l2_pages; /* L2 pages with explicit attributes */

static uint32_t protect_l2(sdos_runtime *runtime)
{
    const uint32_t base = SDOS_CORE_L2SRAM_BASE;
    const uint32_t code_lo = (uint32_t)(uintptr_t)zone_l2code_start;
    const uint32_t code_hi = (uint32_t)(uintptr_t)zone_l2code_end;
    const uint32_t end = (uint32_t)(uintptr_t)zone_l2_end;
    uint32_t page;
    uint32_t before = L2MPPA(0);
    (void)runtime; /* an unenforced page protection is reported, not a fault */

    for (page = 0u; page < L2_PAGES && base + (page + 1u) * L2_PAGE <= end; page++) {
        uint32_t at = base + page * L2_PAGE;
        L2MPPA(page) = at >= code_lo && at < code_hi ? MPPA_CODE : MPPA_DATA;
    }
    L2MPFCR = 1u;
    l2_pages = page;
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
        if (!blocked("l2", &L2MPFSR, &L2MPFAR, &L2MPFCR, code_lo, value)) {
            sdos_trace("l2: this C66x did not refuse a write into the L2 code zone: L2 page protection is not "
                       "enforced; protection capability withheld");
            return 0u;
        }
    }
    return SDOS_CAP_PROTECTION;
}

static const uint32_t lock_key[4] = {0x534F4453u, 0x4B434F4Cu, 0x50414D2Du, 0x00000001u}; /* "SDOSLOCK-MAP" */

static void lock_command(uint32_t command)
{
    uint32_t i;
    MPLKCMD = MPLK_KEYR;
    for (i = 0u; i < 4u; i++) {
        MPLK(i) = lock_key[i];
    }
    MPLKCMD = command;
}

/* What the protection registers must hold from start-up on: the monitor
 * compares them every period, locked or not. */
static uint32_t expected_mpax[SEGMENTS][2];
static uint32_t expected_mppa[L2_PAGES];
static uint32_t expected_pages;
static int snapshot_taken;

static void take_snapshot(uint32_t pages)
{
    uint32_t i;
    for (i = 0u; i < SEGMENTS; i++) {
        expected_mpax[i][0] = XMPAXL(i);
        expected_mpax[i][1] = XMPAXH(i);
    }
    for (i = 0u; i < pages; i++) {
        expected_mppa[i] = L2MPPA(i);
    }
    expected_pages = pages;
    snapshot_taken = 1;
}

/* Lock the configuration and prove it: a write that would change the deny
 * segment's translation, which no access uses, and one to the attributes of
 * a page past the end of L2 must both leave the registers unchanged. */
static void lock_protection(uint32_t pages)
{
    uint32_t mpax = XMPAXL(0);
    uint32_t spare = pages < L2_PAGES ? L2MPPA(L2_PAGES - 1u) : 0u;
    int mpax_locked;
    int mppa_locked = 1;
    int recorded;
    unsigned int key = _disable_interrupts();
    lock_command(MPLK_LOCK);
    XMPAXL(0) = mpax ^ 0x100u;
    mpax_locked = XMPAXL(0) == mpax;
    if (!mpax_locked) {
        XMPAXL(0) = mpax; /* not covered by the lock: put it back */
    }
    if (pages < L2_PAGES) {
        L2MPPA(L2_PAGES - 1u) = spare ^ 0x1u; /* UX */
        mppa_locked = L2MPPA(L2_PAGES - 1u) == spare;
        if (!mppa_locked) {
            L2MPPA(L2_PAGES - 1u) = spare;
        }
    }
    /* The refused write to a locked MPAX register is recorded like a refused
     * memory write, at the register's address (board, Task 042): note it and
     * clear it, so the monitor does not take the test for a fault. */
    recorded = XMPFSR != 0u && XMPFAR == XMC_BASE;
    XMPFCR = 1u;
    L2MPFCR = 1u;
    _restore_interrupts(key);
    take_snapshot(pages);
    sdos_trace("lock: %s; MPAX %s%s, L2 page attributes %s; every register is also checked against its snapshot",
               (MPLKSTAT & MPLK_LOCKED) != 0u ? "locked" : "NOT locked", mpax_locked ? "locked" : "NOT locked",
               recorded ? " (a write attempt is recorded)" : "", mppa_locked ? "locked" : "NOT locked");
}

uint32_t sdos_protection_init(sdos_runtime *runtime)
{
    const uint64_t ddr_start = OWN_DDR_BASE;
    const uint64_t ddr_end = (uint64_t)OWN_DDR_BASE + OWN_DDR_SIZE;
    const original low = found_segment(0u);
    const original high = found_segment(1u);
    unsigned int key;
    uint32_t value;
    uint32_t capability;
    int enforced;
    plan p;
    size_t i;

    if ((MPLKSTAT & MPLK_LOCKED) != 0u) { /* the subsystem stayed powered since a previous run */
        lock_command(MPLK_UNLOCK);
        sdos_trace("lock: unlocked the lock a previous run left (%s)",
                   (MPLKSTAT & MPLK_LOCKED) == 0u ? "done" : "FAILED");
    }
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
    /* Self-tests. On the board (Task 042) this XMC refuses and records writes
     * outside the grants but lets reads through: isolation against corruption
     * holds, reading another partition's memory does not fail. The write
     * probe goes to MSMC, which no one owns, and writes back the value it
     * read, so it is harmless even where it is not refused; the line leaves
     * the cache first, because a cache writeback is not checked. The read of
     * the other C66x's memory is traced for the record. */
    sdos_protection_report();
    {
        volatile uint32_t *probe = (volatile uint32_t *)(uintptr_t)MSMC_PROBE;
        value = *probe;
        sdos_cache_writeback((void *)(uintptr_t)MSMC_PROBE, 64u);
        *probe = value;
        _mfence();
        enforced = blocked("mpax write", &XMPFSR, &XMPFAR, &XMPFCR, MSMC_PROBE, value);
    }
    value = *(volatile uint32_t *)(uintptr_t)NEIGHBOUR_DDR;
    (void)blocked("mpax read", &XMPFSR, &XMPFAR, &XMPFCR, NEIGHBOUR_DDR, value);
    if (enforced) {
        sdos_trace("mpax: %lu segments; writes reach only this core's DDR (0x%08lx..0x%08lx), its vring pool, and "
                   "%lu device windows; code read-only; reads are not isolated by this XMC", (unsigned long)p.count,
                   (unsigned long)ddr_start, (unsigned long)(ddr_end - 1u), (unsigned long)OWN_MMIO_COUNT);
    } else {
        sdos_trace("mpax: %lu segments programmed, but this C66x refused no write outside them; protection "
                   "capability withheld", (unsigned long)p.count);
    }
    capability = protect_l2(runtime) & (enforced ? SDOS_CAP_PROTECTION : 0u);
    lock_protection(l2_pages);
    return capability;
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
    if (snapshot_taken) {
        uint32_t i;
        for (i = 0u; i < SEGMENTS; i++) {
            if (XMPAXL(i) != expected_mpax[i][0] || XMPAXH(i) != expected_mpax[i][1]) {
                sdos_runtime_fault(runtime, SDOS_FAULT_PROTECTION, XMC_BASE + 8u * i, PROTECTION_CHANGED | i);
            }
        }
        for (i = 0u; i < expected_pages; i++) {
            if (L2MPPA(i) != expected_mppa[i]) {
                sdos_runtime_fault(runtime, SDOS_FAULT_PROTECTION, 0x0184A200u + 4u * i,
                                   PROTECTION_CHANGED | 0x100u | i);
            }
        }
    }
}

#if defined(SDOS_FAULT_INJECTION)
/* Fault injection: try to change a protection register. 0: the write was
 * refused (the lock held); 1: it took effect, which the readback check must
 * then report. The deny segment's translation is harmless to change. */
int sdos_protection_tamper(void)
{
    uint32_t before = XMPAXL(0);
    XMPAXL(0) = before ^ 0x100u;
    return XMPAXL(0) != before;
}
#endif

void sdos_protection_guard(const sdos_stack *stacks, size_t count)
{
    (void)stacks;
    (void)count; /* no MMU: the monitor's fill check guards C66x stacks */
}

void sdos_halt(void)
{
    sdos_idle_forever(); /* idle, not a busy loop: Linux can then power the core off */
}

void sdos_wait_for_event(void)
{
    /* nothing: Linux stops a C66x by local reset, busy or not */
}

void sdos_idle_forever(void)
{
    (void)_disable_interrupts();
    for (;;) {
        __asm(" IDLE");
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
