/* Course firmware entry under FreeRTOS: one task serves protocol requests. */
#include <ti/csl/csl_types.h>
#include <ti/drv/sciclient/sciclient.h>
#include <ti/osal/TaskP.h>
#include <ti/osal/osal.h>

#include "sdos/platform.h"
#include "sdos/runtime.h"

#if defined(BUILD_C7X)
#define STACK_ALIGN 8192 /* the C7x port needs 8 KiB-aligned stacks of at least 16 KiB */
#else
#define STACK_ALIGN 8
#endif

static uint8_t main_stack[32u * 1024u] __attribute__((aligned(STACK_ALIGN)));
static sdos_runtime runtime;

static void main_task(void *arg0, void *arg1)
{
    Sciclient_ConfigPrms_t config;
    (void)arg0;
    (void)arg1;
    /* The IPC library routes mailbox interrupts through the system firmware. */
    (void)Sciclient_configPrmsInit(&config);
    if (Sciclient_init(&config) != CSL_PASS) {
        sdos_trace("Sciclient_init failed");
        return;
    }
    sdos_platform_init();
    sdos_identity_init(&runtime.identity, SDOS_RUNTIME_FREERTOS);
    runtime.algorithms = sdos_builtin_algorithms;
    runtime.algorithm_count = sdos_builtin_algorithm_count;
    runtime.cache_invalidate = sdos_cache_invalidate;
    runtime.cache_writeback = sdos_cache_writeback;
    runtime.cycles = sdos_cycles;
    runtime.uptime_ms = sdos_uptime_ms;
    sdos_trace("%s, FreeRTOS, starting", runtime.identity.firmware);
    sdos_service_run(&runtime);
}

int main(void)
{
    TaskP_Params params;
    OS_init();
    TaskP_Params_init(&params);
    params.priority = 2;
    params.stack = main_stack;
    params.stacksize = sizeof main_stack;
    if (TaskP_create(main_task, &params) == NULL) {
        OS_stop();
    }
    OS_start(); /* does not return */
    return 0;
}
