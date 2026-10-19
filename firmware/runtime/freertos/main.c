/* Course firmware entry under FreeRTOS: one task serves protocol requests and
 * a higher-priority monitor task supervises it (docs/safety.md). */
#include <string.h>

#include <ti/csl/csl_types.h>
#include <ti/drv/sciclient/sciclient.h>
#include <ti/osal/TaskP.h>
#include <ti/osal/osal.h>

#include "FreeRTOS.h"
#include "portable.h"

#include "sdos/fault_injection.h"
#include "sdos/platform.h"
#include "sdos/runtime.h"

#if defined(BUILD_C7X)
#define STACK_ALIGN 8192 /* the C7x port needs 8 KiB-aligned stacks */
/* A C7x task frame holds the wide vector registers: with 16 KiB the monitor
 * task stopped on its first switch (board, Task 021). TI's own C7x idle and
 * timer tasks get 256 KiB. */
#define MONITOR_STACK_SIZE (64u * 1024u)
#define MAIN_STACK_SIZE (64u * 1024u) /* about 21 KiB in use on the board */
#else
#define STACK_ALIGN 8
#define MONITOR_STACK_SIZE (16u * 1024u)
#define MAIN_STACK_SIZE (32u * 1024u)
#endif

#define MAIN_PRIORITY 2
/* Above every course and IPC task, so a runaway request cannot starve it. */
#define MONITOR_PRIORITY (configMAX_PRIORITIES - 2)

/* Each stack in its own section, so the linker files can place it and the
 * C71x MMU can guard the page below its usable part. */
static uint8_t main_stack[MAIN_STACK_SIZE] __attribute__((section(".sdos.stack.main"), aligned(STACK_ALIGN)));
static uint8_t monitor_stack[MONITOR_STACK_SIZE] __attribute__((section(".sdos.stack.monitor"), aligned(STACK_ALIGN)));
static sdos_runtime runtime __attribute__((section(".sdos.runtime")));

size_t sdos_kernel_heap_free(void)
{
    return xPortGetFreeHeapSize();
}

static void monitor_task(void *arg0, void *arg1)
{
    (void)arg0;
    (void)arg1;
    for (;;) {
        sdos_monitor_tick();
        TaskP_sleep(SDOS_MONITOR_PERIOD_MS); /* ticks of 1 ms (configTICK_RATE_HZ) */
    }
}

static void main_task(void *arg0, void *arg1)
{
    Sciclient_ConfigPrms_t config;
    TaskP_Params params;
    uint32_t capabilities;
    int32_t status;
    (void)arg0;
    (void)arg1;
    runtime.cycles = sdos_cycles;
    runtime.uptime_ms = sdos_uptime_ms;
    runtime.cycles_per_us = sdos_cycles_per_us();
    sdos_platform_init();
    capabilities = sdos_monitor_init(&runtime);
    sdos_monitor_stack("main", main_stack, sizeof main_stack);
    sdos_monitor_stack("monitor", monitor_stack, sizeof monitor_stack);
    TaskP_Params_init(&params);
    params.priority = MONITOR_PRIORITY;
    params.stack = monitor_stack;
    params.stacksize = sizeof monitor_stack;
    if (TaskP_create(monitor_task, &params) == NULL) {
        sdos_monitor_fatal(SDOS_FAULT_SELF_TEST, 0u, 1u);
    }
    if (sdos_layout_check() != 0) {
        sdos_runtime_fault(&runtime, SDOS_FAULT_LAYOUT, 0u, 0u);
    }
    sdos_identity_init(&runtime.identity, SDOS_RUNTIME_FREERTOS);
    runtime.identity.capabilities |= capabilities;
    runtime.algorithms = sdos_builtin_algorithms;
    runtime.algorithm_count = sdos_builtin_algorithm_count;
    runtime.cache_invalidate = sdos_cache_invalidate;
    runtime.cache_writeback = sdos_cache_writeback;
#if defined(SDOS_FAULT_INJECTION)
    sdos_fault_injection_install(&runtime);
#endif
    /* The IPC library routes mailbox interrupts through the system firmware. */
    (void)Sciclient_configPrmsInit(&config);
    config.opModeFlag = SCICLIENT_SERVICE_OPERATION_MODE_POLLED; /* already started so in main() */
    status = Sciclient_init(&config);
    if (status != CSL_PASS) {
        sdos_runtime_fault(&runtime, SDOS_FAULT_IPC, (uint32_t)status, 1u);
        sdos_trace("Sciclient_init failed: %ld", (long)status);
        sdos_protection_report();
        return; /* the monitor keeps the health line current */
    }
    sdos_trace("%s, FreeRTOS, starting", runtime.identity.firmware);
    sdos_service_run(&runtime);
}

/* Sciclient starts here, polled, before the kernel: Sciclient is only used
 * during start-up, and in interrupt mode every C66x request after the first
 * timed out on the board (Task 021). Later Sciclient_init calls keep this
 * configuration. */
static void sciclient_start(void)
{
    Sciclient_ConfigPrms_t config;
    (void)Sciclient_configPrmsInit(&config);
    config.opModeFlag = SCICLIENT_SERVICE_OPERATION_MODE_POLLED;
    if (Sciclient_init(&config) != CSL_PASS) {
        sdos_trace("boot: Sciclient_init failed");
    }
}

#if defined(BUILD_C66X)
/* TI's OS_init does two things on a J721E C66x (osal TaskP_freertos.c): it
 * starts Sciclient and asks the device manager to route this core's tick (its
 * DM timer) into event 21 or 20, stopping for good if that is refused. When
 * Linux stops the core the route stays programmed in this subsystem's
 * interrupt router, and the device manager then refuses to set it again or to
 * release it, so every start after the first hung in OS_init (Task 021). This
 * replaces OS_init: it requests the same route and accepts a refusal only when
 * the route is verifiably in place. The router is read through the C66x RAT,
 * which Sciclient_init maps; its outputs reach only this C66x. */
#if defined(BUILD_C66X_1)
#define TICK_TIMER TISCI_DEV_TIMER0
#define TICK_CORE TISCI_DEV_C66SS0_CORE0
#define TICK_EVENT 21u
#define TICK_ROUTER 0x20AC0000u /* C66SS0 interrupt router */
#define TICK_INPUT 1u           /* DM timer 0, as the device manager routes it (board, Task 021) */
#else
#define TICK_TIMER TISCI_DEV_TIMER1
#define TICK_CORE TISCI_DEV_C66SS1_CORE0
#define TICK_EVENT 20u
#define TICK_ROUTER 0x20AD0000u /* C66SS1 interrupt router */
#define TICK_INPUT 2u           /* DM timer 1 */
#endif
#define ROUTER_OUTPUT(event) (*(volatile uint32_t *)(TICK_ROUTER + 4u + 4u * ((event) - 10u)))
#define ROUTER_ENABLE 0x10000u
#define ROUTER_INPUT_MASK 0x3FFu

static int kernel_init(void)
{
    struct tisci_msg_rm_irq_set_req req;
    struct tisci_msg_rm_irq_set_resp resp;
    uint32_t route;
    (void)memset(&req, 0, sizeof req);
    req.valid_params = TISCI_MSG_VALUE_RM_DST_ID_VALID | TISCI_MSG_VALUE_RM_DST_HOST_IRQ_VALID;
    req.src_id = TICK_TIMER;
    req.src_index = 0u;
    req.dst_id = TICK_CORE;
    req.dst_host_irq = TICK_EVENT;
    req.secondary_host = TISCI_MSG_VALUE_RM_UNUSED_SECONDARY_HOST;
    if (Sciclient_rmIrqSet(&req, &resp, SCICLIENT_SERVICE_WAIT_FOREVER) == CSL_PASS) {
        return 0;
    }
    route = ROUTER_OUTPUT(TICK_EVENT);
    if ((route & ROUTER_ENABLE) != 0u && (route & ROUTER_INPUT_MASK) == TICK_INPUT) {
        sdos_trace("boot: tick route from a previous run is in place (router %08lx); using it",
                   (unsigned long)route);
        return 0;
    }
    sdos_trace("boot: the device manager refused the tick route and none is in place (router %08lx)",
               (unsigned long)route);
    return -1;
}
#else
static int kernel_init(void)
{
    OS_init();
    return 0;
}
#endif

int main(void)
{
    TaskP_Params params;
    sdos_tick_timer_reset(); /* before the kernel can take its interrupt */
    (void)memset(&runtime, 0, sizeof runtime); /* .sdos.runtime may be NOLOAD (C71x L2) */
    sciclient_start();
    if (kernel_init() != 0) {
        sdos_monitor_fatal(SDOS_FAULT_SELF_TEST, 0u, 0x10u); /* no tick: the kernel cannot run */
    }
    TaskP_Params_init(&params);
    params.priority = MAIN_PRIORITY;
    params.stack = main_stack;
    params.stacksize = sizeof main_stack;
    if (TaskP_create(main_task, &params) == NULL) {
        OS_stop();
    }
    OS_start(); /* does not return */
    return 0;
}
