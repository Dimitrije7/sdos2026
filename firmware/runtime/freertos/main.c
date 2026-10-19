/* Course firmware entry under FreeRTOS: one task serves protocol requests and
 * a higher-priority monitor task supervises it (docs/safety.md). */
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
#define STACK_ALIGN 8192 /* the C7x port needs 8 KiB-aligned stacks of at least 16 KiB */
#else
#define STACK_ALIGN 8
#endif

#define MAIN_PRIORITY 2
/* Above every course and IPC task, so a runaway request cannot starve it. */
#define MONITOR_PRIORITY (configMAX_PRIORITIES - 2)

/* Each stack in its own section, so the linker files can place it and the
 * C71x MMU can guard the page below its usable part. */
static uint8_t main_stack[32u * 1024u] __attribute__((section(".sdos.stack.main"), aligned(STACK_ALIGN)));
static uint8_t monitor_stack[16u * 1024u] __attribute__((section(".sdos.stack.monitor"), aligned(STACK_ALIGN)));
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
    if (Sciclient_init(&config) != CSL_PASS) {
        sdos_runtime_fault(&runtime, SDOS_FAULT_IPC, 0u, 1u);
        sdos_trace("Sciclient_init failed");
        return; /* the monitor keeps the health line current */
    }
    sdos_trace("%s, FreeRTOS, starting", runtime.identity.firmware);
    sdos_service_run(&runtime);
}

int main(void)
{
    TaskP_Params params;
    OS_init();
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
