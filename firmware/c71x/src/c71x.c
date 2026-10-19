/* C71x adapter: least-privilege MMU map, exception handling, cache
 * maintenance, and the cycle counter.
 *
 * TI's PDK example c7x_mmu.c (BSD-3-Clause) maps all of DDR as cached and
 * executable. This map grants only what this core owns (docs/safety.md):
 *
 *   code zone           read-only, executable, cached
 *   read-only data zone read-only, not executable, cached
 *   rest of carve-out   read-write, not executable, cached
 *   vring pool          read-write, not executable, uncached
 *   local L2 SRAM       read-write, executable (load/run copies land here)
 *   device windows      read-write, not executable, device memory
 *
 * Every other address, including Linux's memory and the other cores', is
 * unmapped and faults. After start-up the page tables become read-only and
 * the lowest page of every guarded stack becomes read-only, so an overflow
 * faults instead of corrupting what lies below. */
#include <c7x.h>

#include <ti/osal/CacheP.h>
#include <ti/osal/HwiP.h>
#include <ti/osal/soc/osal_soc.h>
#include "Exception.h" /* csl/arch/c7x, from the installed PDK */
#include "Mmu.h"

#include "memory_map_defines.inc"

#define SDOS_CORES_MACROS_ONLY
#include "sdos/cores.h"
#include "sdos/platform.h"
#include "sdos/safety.h"

#if (C7x_1_IPC_DATA_BASE) != SDOS_C71X_0_DMA_POOL_BASE
#error "TI's C71x_0 IPC pool differs from manifest/dsp-cores.json"
#endif

#define CPU_HZ 1000000000u /* TDA4VM C71x clock */
#define PAGE 0x1000u
#define MMU_OK 0x4D4D5531u /* "1UMM": every mapping succeeded */
#define IERR_PAGE_FAULT 0x1u

/* Protection zones and the page tables, from the linker files. */
extern char sdos_zone_code_start[];
extern char sdos_zone_code_end[];
extern char sdos_zone_rodata_start[];
extern char sdos_zone_rodata_end[];
extern char sdos_mmu_tables_start[];
extern char sdos_mmu_tables_end[];
static char *const zone_code_start = sdos_zone_code_start;
static char *const zone_code_end = sdos_zone_code_end;
static char *const zone_rodata_start = sdos_zone_rodata_start;
static char *const zone_rodata_end = sdos_zone_rodata_end;
static char *const mmu_tables_start = sdos_mmu_tables_start;
static char *const mmu_tables_end = sdos_mmu_tables_end;

/* Device windows from manifest/dsp-cores.json: {base, size}. */
typedef struct window {
    uint64_t base;
    uint64_t size;
} window;
static const window mmio[SDOS_C71X_0_MMIO_COUNT] = SDOS_C71X_0_MMIO_WINDOWS;
static const sdos_span l2 = {SDOS_C71X_0_L2SRAM_BASE, (uint64_t)SDOS_C71X_0_L2SRAM_BASE + SDOS_C71X_0_L2SRAM_SIZE};
static const sdos_span carveout = {SDOS_C71X_0_DDR_BASE, (uint64_t)SDOS_C71X_0_DDR_BASE + SDOS_C71X_0_DDR_SIZE};
static const sdos_span pool = {SDOS_C71X_0_DMA_POOL_BASE,
                               (uint64_t)SDOS_C71X_0_DMA_POOL_BASE + SDOS_C71X_0_DMA_POOL_SIZE};

/* InitMmu runs before C initialization, so its result goes through memory C
 * initialization does not touch. */
static volatile uint32_t mmu_status __attribute__((section(".sdos.noinit")));

typedef enum kind { DEVICE, LOCAL, CODE, RODATA, DATA, UNCACHED } kind;

static void attributes(Mmu_MapAttrs *a, kind k, bool secure)
{
    Mmu_initMapAttrs(a);
    a->ns = secure ? false : true;
    a->privExecute = k == LOCAL || k == CODE;
    a->accessPerm = k == CODE || k == RODATA ? Mmu_AccessPerm_PRIV_RO_USER_NONE : Mmu_AccessPerm_PRIV_RW_USER_NONE;
    /* Device type for peripherals and, as in TI's map, for local L2 SRAM. */
    a->attrIndx = k == DEVICE || k == LOCAL ? Mmu_AttrIndx_MAIR0
                  : k == UNCACHED          ? Mmu_AttrIndx_MAIR4
                                           : Mmu_AttrIndx_MAIR7;
}

static int map(uint64_t start, uint64_t end, kind k, bool secure)
{
    Mmu_MapAttrs a;
    if (end <= start) {
        return 0;
    }
    attributes(&a, k, secure);
    return Mmu_map(start, start, (size_t)(end - start), &a, secure) ? 0 : 1;
}

static sdos_span clip(const char *start, const char *end, sdos_span to)
{
    sdos_span s;
    s.start = (uintptr_t)start > to.start ? (uintptr_t)start : to.start;
    s.end = (uintptr_t)end < to.end ? (uintptr_t)end : to.end;
    if (s.end < s.start) {
        s.end = s.start;
    }
    return s;
}

static int map_all(bool secure)
{
    sdos_span zones[2];
    sdos_span pieces[4];
    int failures = 0;
    size_t n;
    size_t i;
    size_t w;

    for (w = 0u; w < SDOS_C71X_0_MMIO_COUNT; w++) {
        sdos_span device;
        device.start = mmio[w].base;
        device.end = mmio[w].base + mmio[w].size;
        n = sdos_span_subtract(device, &l2, 1u, pieces, 4u);
        for (i = 0u; i < n && n != SIZE_MAX; i++) {
            failures += map(pieces[i].start, pieces[i].end, DEVICE, secure);
        }
    }
    failures += map(l2.start, l2.end, LOCAL, secure);
    zones[0] = clip(zone_code_start, zone_code_end, carveout);
    zones[1] = clip(zone_rodata_start, zone_rodata_end, carveout);
    n = sdos_span_subtract(carveout, zones, 2u, pieces, 4u);
    if (n == SIZE_MAX) {
        return failures + 1;
    }
    for (i = 0u; i < n; i++) {
        failures += map(pieces[i].start, pieces[i].end, DATA, secure);
    }
    failures += map(zones[0].start, zones[0].end, CODE, secure);
    failures += map(zones[1].start, zones[1].end, RODATA, secure);
    failures += map(pool.start, pool.end, UNCACHED, secure);
    return failures;
}

/* Called by the FreeRTOS C7x port before C initialization, MMU off. */
void InitMmu(void);
void InitMmu(void)
{
    int failures = map_all(false) + map_all(true);
    mmu_status = failures == 0 ? MMU_OK : (uint32_t)failures;
    OsalCfgClecAccessCtrl(false);
}

/* After an exception the core cannot safely continue: record and halt. A
 * page fault inside a stack's guard page is a stack overflow. */
static void exception_hook(void)
{
    uint64_t ierr = __IERR;
    uint64_t iear = __IEAR;
    uint64_t iesr = __IESR;
    uint32_t info = (uint32_t)(ierr & 0xFFFFu) | ((uint32_t)(iesr & 0xFFFFu) << 16);
    if ((ierr & IERR_PAGE_FAULT) != 0u) {
        sdos_monitor_fatal(sdos_monitor_guarded(iear) >= 0 ? SDOS_FAULT_STACK : SDOS_FAULT_ACCESS, iear, info);
    }
    sdos_monitor_fatal(SDOS_FAULT_EXCEPTION, Exception_Module_state.nrp, info);
}

static int remap_read_only(uint64_t start, uint64_t end)
{
    int failures = 0;
    kind k = start >= l2.start && end <= l2.end ? LOCAL : RODATA;
    Mmu_MapAttrs a;
    int pass;
    for (pass = 0; pass < 2; pass++) {
        bool secure = pass == 1;
        attributes(&a, k, secure);
        a.accessPerm = Mmu_AccessPerm_PRIV_RO_USER_NONE;
        a.privExecute = false;
        failures += Mmu_map(start, start, (size_t)(end - start), &a, secure) ? 0 : 1;
    }
    return failures;
}

uint32_t sdos_protection_init(sdos_runtime *runtime)
{
    static const Exception_Hooks hooks = {exception_hook, NULL, NULL};
    uint64_t tables = (uintptr_t)mmu_tables_start;
    uint64_t tables_end = ((uintptr_t)mmu_tables_end + PAGE - 1u) & ~(uint64_t)(PAGE - 1u);

    Exception_registerHooks(&hooks);
    if (mmu_status != MMU_OK) {
        sdos_runtime_fault(runtime, SDOS_FAULT_PROTECTION, 0u, mmu_status);
        sdos_trace("mmu: %lu mappings failed; protection incomplete", (unsigned long)mmu_status);
        return 0u;
    }
    if (remap_read_only(tables, tables_end) != 0) {
        sdos_runtime_fault(runtime, SDOS_FAULT_PROTECTION, tables, 0x100u);
        return 0u;
    }
    sdos_trace("mmu: only this core's memory and device windows mapped; code read-only, data not executable, "
               "page tables 0x%08lx..0x%08lx read-only", (unsigned long)tables, (unsigned long)(tables_end - 1u));
    return SDOS_CAP_PROTECTION;
}

void sdos_protection_poll(sdos_runtime *runtime)
{
    (void)runtime; /* the C71x MMU raises an exception on every violation */
}

void sdos_protection_guard(const sdos_stack *stacks, size_t count)
{
    size_t i;
    int failures = 0;
    for (i = 0u; i < count; i++) {
        uint64_t base = (uintptr_t)stacks[i].base;
        if (base % PAGE == 0u && stacks[i].size > 2u * PAGE) {
            failures += remap_read_only(base, base + PAGE);
        }
    }
    if (failures != 0) {
        sdos_monitor_fatal(SDOS_FAULT_PROTECTION, 0u, 0x200u);
    }
}

void sdos_halt(void)
{
    (void)HwiP_disable();
    for (;;) {
        /* fatal fault: Linux restarts the core */
    }
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

uint32_t sdos_cycles_per_us(void)
{
    return CPU_HZ / 1000000u;
}

uint32_t sdos_uptime_ms(void)
{
    return (uint32_t)(sdos_cycles() / (CPU_HZ / 1000u));
}
