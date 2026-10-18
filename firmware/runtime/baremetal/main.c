/* Course firmware entry on bare metal: one loop polls the mailbox and serves
 * protocol requests. No scheduler and no interrupts, on every DSP. */
#include <ti/csl/csl_types.h>
#include <ti/drv/sciclient/sciclient.h>

#include "sdos/platform.h"
#include "sdos/runtime.h"

static sdos_runtime runtime;

int main(void)
{
    Sciclient_ConfigPrms_t config;
    (void)Sciclient_configPrmsInit(&config);
    config.opModeFlag = SCICLIENT_SERVICE_OPERATION_MODE_POLLED; /* no interrupts here */
    if (Sciclient_init(&config) != CSL_PASS) {
        sdos_trace("Sciclient_init failed");
        return 1;
    }
    sdos_platform_init();
    sdos_identity_init(&runtime.identity, SDOS_RUNTIME_BAREMETAL);
    runtime.algorithms = sdos_builtin_algorithms;
    runtime.algorithm_count = sdos_builtin_algorithm_count;
    runtime.cache_invalidate = sdos_cache_invalidate;
    runtime.cache_writeback = sdos_cache_writeback;
    runtime.cycles = sdos_cycles;
    runtime.uptime_ms = sdos_uptime_ms;
    sdos_trace("%s, bare metal, starting", runtime.identity.firmware);
    sdos_service_run(&runtime);
    for (;;) {
        /* Linux stops the core after the shutdown handshake. */
    }
}
