/* Course firmware entry on bare metal: one loop polls the mailbox, serves
 * protocol requests, and runs the monitor between requests. No scheduler and
 * no interrupts, on every DSP (docs/safety.md). */
#include <string.h>

#include <ti/csl/csl_types.h>
#include <ti/drv/sciclient/sciclient.h>

#include "sdos/fault_injection.h"
#include "sdos/platform.h"
#include "sdos/runtime.h"

static sdos_runtime runtime __attribute__((section(".sdos.runtime")));

/* No scheduler, so no kernel heap is in use. */
size_t sdos_kernel_heap_free(void)
{
    return SIZE_MAX;
}

int main(void)
{
    Sciclient_ConfigPrms_t config;
    uint32_t capabilities;
    int32_t status;
    sdos_tick_timer_reset(); /* unused here, but a FreeRTOS run before may have left it firing */
    (void)memset(&runtime, 0, sizeof runtime); /* .sdos.runtime may be NOLOAD (C71x L2) */
    runtime.cycles = sdos_cycles;
    runtime.uptime_ms = sdos_uptime_ms;
    runtime.cycles_per_us = sdos_cycles_per_us();
    sdos_platform_init();
    capabilities = sdos_monitor_init(&runtime);
    if (sdos_layout_check() != 0) {
        sdos_runtime_fault(&runtime, SDOS_FAULT_LAYOUT, 0u, 0u);
    }
    sdos_identity_init(&runtime.identity, SDOS_RUNTIME_BAREMETAL);
    runtime.identity.capabilities |= capabilities;
    runtime.algorithms = sdos_builtin_algorithms;
    runtime.algorithm_count = sdos_builtin_algorithm_count;
    runtime.cache_invalidate = sdos_cache_invalidate;
    runtime.cache_writeback = sdos_cache_writeback;
#if defined(SDOS_FAULT_INJECTION)
    sdos_fault_injection_install(&runtime);
#endif
    (void)Sciclient_configPrmsInit(&config);
    config.opModeFlag = SCICLIENT_SERVICE_OPERATION_MODE_POLLED; /* no interrupts here */
    status = Sciclient_init(&config);
    if (status == CSL_PASS) {
        sdos_trace("%s, bare metal, starting", runtime.identity.firmware);
        sdos_service_run(&runtime);
    } else {
        sdos_runtime_fault(&runtime, SDOS_FAULT_IPC, (uint32_t)status, 1u);
        sdos_trace("Sciclient_init failed: %ld", (long)status);
        sdos_protection_report();
    }
    for (;;) {
        /* Keep the health line current until Linux stops the core. */
        sdos_monitor_poll();
    }
}
