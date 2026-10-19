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
 *   L2 code zone        read-only, executable, after start (load/run copies
 *                       land here before main(), when it is still writable)
 *   rest of L2 SRAM     read-write, not executable
 *   device windows      read-write, not executable, device memory: only the
 *                       peripherals the drivers use (manifest/dsp-cores.json)
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
#include "Hwi.h"
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
extern char sdos_zone_l2code_start[];
extern char sdos_zone_l2code_end[];
static char *const zone_code_start = sdos_zone_code_start;
static char *const zone_code_end = sdos_zone_code_end;
static char *const zone_rodata_start = sdos_zone_rodata_start;
static char *const zone_rodata_end = sdos_zone_rodata_end;
static char *const mmu_tables_start = sdos_mmu_tables_start;
static char *const mmu_tables_end = sdos_mmu_tables_end;
static char *const zone_l2code_start = sdos_zone_l2code_start;
static char *const zone_l2code_end = sdos_zone_l2code_end;

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

typedef enum kind { DEVICE, LOCAL, LOCAL_CODE, LOCAL_DATA, CODE, RODATA, DATA, UNCACHED } kind;

static void attributes(Mmu_MapAttrs *a, kind k, bool secure)
{
    int local = k == LOCAL || k == LOCAL_CODE || k == LOCAL_DATA;
    Mmu_initMapAttrs(a);
    a->ns = secure ? false : true;
    a->privExecute = k == LOCAL || k == LOCAL_CODE || k == CODE;
    a->accessPerm = k == CODE || k == RODATA || k == LOCAL_CODE ? Mmu_AccessPerm_PRIV_RO_USER_NONE
                                                                : Mmu_AccessPerm_PRIV_RW_USER_NONE;
    /* Device type for peripherals and, as in TI's map, for local L2 SRAM. */
    a->attrIndx = k == DEVICE || local ? Mmu_AttrIndx_MAIR0
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
    failures += map(l2.start, l2.end, LOCAL, secure); /* split by sdos_protection_init after the copies */
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
/* Exceptions record the fault, then leave exception context into
 * exception_halted, which idles. A C71x idling inside its exception handler
 * could not be powered off by Linux (board, Task 021); one idling in normal
 * context can, so dspctl recover works after any fault. */
static void exception_hook(void)
{
    uint64_t ierr = __IERR;
    uint64_t iear = __IEAR;
    uint64_t iesr = __IESR;
    uint32_t info = (uint32_t)(ierr & 0xFFFFu) | ((uint32_t)(iesr & 0xFFFFu) << 16);
    if ((ierr & IERR_PAGE_FAULT) != 0u) {
        sdos_monitor_record_fatal(sdos_monitor_guarded(iear) >= 0 ? SDOS_FAULT_STACK : SDOS_FAULT_ACCESS, iear,
                                  info);
    } else {
        sdos_monitor_record_fatal(SDOS_FAULT_EXCEPTION, Exception_Module_state.nrp, info);
    }
}

void exception_halted(void);
void exception_halted(void)
{
    sdos_halt();
}

/* TI's return hook contract (Exception_asm.asm): return from the exception
 * with RETE to an address of our choice, with the TSR saved at the exception
 * (ECSP[1]), as TI's c7x_mma_error_injection example does. */
static void exception_return(void)
{
    __asm(" addkpc.d1 $PCR_OFFSET(exception_halted), a0");
    __asm(" ldd.d1 *ECSP[1], a1");
    __asm(" rete.s1 a0, a1");
}

/* After the load/run copies: the L2 code zone read-only and executable, the
 * rest of L2 writable and not executable. */
static int split_l2(void)
{
    sdos_span code;
    sdos_span pieces[2];
    size_t n;
    size_t i;
    int failures = 0;
    int pass;
    code.start = (uintptr_t)zone_l2code_start;
    code.end = (uintptr_t)zone_l2code_end;
    n = sdos_span_subtract(l2, &code, 1u, pieces, 2u);
    if (n == SIZE_MAX) {
        return 1;
    }
    for (pass = 0; pass < 2; pass++) {
        bool secure = pass == 1;
        for (i = 0u; i < n; i++) {
            failures += map(pieces[i].start, pieces[i].end, LOCAL_DATA, secure);
        }
        failures += map(code.start, code.end, LOCAL_CODE, secure);
    }
    return failures;
}

static int remap_read_only(uint64_t start, uint64_t end)
{
    int failures = 0;
    kind k = start >= l2.start && end <= l2.end ? LOCAL_DATA : RODATA;
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
    static const Exception_Hooks hooks = {exception_hook, NULL, exception_return};
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
    if (split_l2() != 0) {
        sdos_runtime_fault(runtime, SDOS_FAULT_PROTECTION, l2.start, 0x300u);
        return 0u;
    }
    sdos_trace("mmu: only this core's memory and %lu device windows mapped; code read-only, data not executable, "
               "L2 code 0x%08lx..0x%08lx read-only, page tables 0x%08lx..0x%08lx read-only",
               (unsigned long)SDOS_C71X_0_MMIO_COUNT, (unsigned long)(uintptr_t)zone_l2code_start,
               (unsigned long)(uintptr_t)zone_l2code_end, (unsigned long)tables, (unsigned long)(tables_end - 1u));
    return SDOS_CAP_PROTECTION;
}

void sdos_protection_poll(sdos_runtime *runtime)
{
    (void)runtime; /* the C71x MMU raises an exception on every violation */
}

void sdos_protection_report(void)
{
    sdos_trace("mmu: status %lu; a refused access raises an exception, which the fault record names",
               (unsigned long)mmu_status);
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
    sdos_idle_forever(); /* idle, not a busy loop: Linux can then power the core off */
}

#define TICK_TIMER_BASE 0x02420000u /* DM timer 2: OSAL_FREERTOS_OS_TIMER_ID_C7X_1 */
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

#if defined(BAREMETAL)
#include <ti/csl/arch/csl_arch.h> /* the C7x CLEC API */
#include <ti/osal/TimerP.h>

/* The wake tick is wired the way TI's C7x FreeRTOS port wires its own tick
 * (port_c7x.c): DM timer 2, through the CLEC straight to CPU interrupt 16, no
 * device-manager route. Bare metal runs no scheduler, so the timer is free. */
#define WAKE_TIMER_ID 2u      /* OSAL_FREERTOS_OS_TIMER_ID_C7X_1 */
#define WAKE_INTERRUPT 16u    /* configTIMER_INT_NUM, FreeRTOSConfig_c7x_1.h */
#define WAKE_EVENT (992u + CSLR_COMPUTE_CLUSTER0_CLEC_SOC_EVENTS_IN_TIMER2_INTR_PEND_0)
#define WAKE_PERIOD_US 1000u

static volatile uint32_t wake_ms; /* wall time while idling: the TSC stops in idle (board, Task 021) */
static int wake_state;            /* 0: not set up, 1: running, -1: unavailable */

/* Also the bare-metal watchdog: it stays enabled while a request runs, so a
 * runaway algorithm is halted here, the only place bare metal can preempt it. */
static void wake_tick(uintptr_t arg)
{
    (void)arg;
    wake_ms += WAKE_PERIOD_US / 1000u;
    sdos_monitor_watchdog();
}

/* Start the wake tick and wait, busy, for its first interrupt. After a fault
 * halt the first tick sometimes never came (board, Task 042: 1 start in 3 to 4,
 * with the timer's interrupt pending and never taken; root cause open), and a
 * core idling on a tick that never comes is lost. */
static TimerP_Handle wake_start(void)
{
    CSL_CLEC_EVTRegs *clec = (CSL_CLEC_EVTRegs *)CSL_COMPUTE_CLUSTER0_CLEC_REGS_BASE;
    CSL_ClecEventConfig event;
    TimerP_Params params;
    TimerP_Handle timer;
    uint32_t key;
    uint64_t deadline;
    event.secureClaimEnable = 0u;
    event.evtSendEnable = 1u;
    event.rtMap = CSL_CLEC_RTMAP_CPU_4;
    event.extEvtNum = 0u;
    event.c7xEvtNum = WAKE_INTERRUPT;
    CSL_clecClearEvent(clec, WAKE_EVENT);
    (void)CSL_clecConfigEventLevel(clec, WAKE_EVENT, 0u); /* pulse, as TI's port */
    (void)CSL_clecConfigEvent(clec, WAKE_EVENT, &event);
    TimerP_Params_init(&params);
    params.runMode = TimerP_RunMode_CONTINUOUS;
    params.startMode = TimerP_StartMode_USER;
    params.periodType = TimerP_PeriodType_MICROSECS;
    params.period = WAKE_PERIOD_US;
    params.intNum = WAKE_INTERRUPT;
    params.eventId = TimerP_USE_DEFAULT;
    timer = TimerP_create(WAKE_TIMER_ID, wake_tick, &params);
    if (timer == NULL) {
        return NULL;
    }
    wake_ms = sdos_uptime_ms(); /* continue from the busy start-up time */
    key = Hwi_enable();
    if (TimerP_start(timer) == TimerP_OK) {
        uint32_t before = wake_ms;
        deadline = sdos_cycles() + (uint64_t)20u * 1000u * sdos_cycles_per_us();
        while (wake_ms == before && sdos_cycles() < deadline) {
        }
        if (wake_ms != before) {
            return timer; /* interrupts stay enabled: only the wake tick is */
        }
    }
    Hwi_restore(key);
    (void)TimerP_stop(timer);
    (void)TimerP_delete(timer);
    sdos_tick_timer_reset();
    return NULL;
}

void sdos_wait_for_event(void)
{
    if (wake_state == 0) {
        int attempt;
        wake_state = -1;
        for (attempt = 1; attempt <= 3 && wake_state < 0; attempt++) {
            if (wake_start() != NULL) {
                wake_state = 1;
            } else {
                sdos_trace("wake tick: no interrupt on attempt %d", attempt);
            }
        }
        if (wake_state < 0) {
            sdos_trace("wake tick unavailable: this core polls without idling and Linux cannot stop it");
        }
    }
    if (wake_state > 0) {
        (void)Hwi_enable(); /* idle only with the wake tick able to end it */
        __asm(" idle");
    }
}
#else
void sdos_wait_for_event(void)
{
    /* FreeRTOS: the idle task idles the CPU */
}
#endif

void sdos_idle_forever(void)
{
    (void)HwiP_disable();
    for (;;) {
        __asm(" idle");
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
#if defined(BAREMETAL)
    if (wake_state > 0) {
        return wake_ms;
    }
#endif
    return (uint32_t)(sdos_cycles() / (CPU_HZ / 1000u));
}
