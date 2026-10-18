/*
 * What each DSP target provides to the course runtime.
 *
 * firmware/c66x and firmware/c71x implement the cache, clock, and memory
 * hooks; firmware/runtime/<freertos|baremetal> provides main(). The IPC
 * service and the trace buffer are common to all six targets.
 */
#ifndef SDOS_PLATFORM_H
#define SDOS_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#include "sdos/runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Family adapter (firmware/c66x, firmware/c71x). */
void sdos_platform_init(void); /* caches or MMU and the cycle counter; before IPC */
void sdos_cache_invalidate(void *address, size_t length);
void sdos_cache_writeback(void *address, size_t length); /* write back and invalidate */
uint64_t sdos_cycles(void);
uint32_t sdos_uptime_ms(void);

/* Trace buffer that Linux reads through the resource table (dspctl trace). */
#define SDOS_TRACE_SIZE 0x10000u
extern char sdos_trace_buffer[SDOS_TRACE_SIZE];
void sdos_trace(const char *format, ...);

/* The resource table for this core; see resource_table.c. */
extern const void *const sdos_resource_table_address;

/* Bring up IPC with Linux and answer requests until Linux shuts the core
 * down. Under FreeRTOS this blocks in RPMessage_recv; bare metal polls. */
void sdos_service_run(sdos_runtime *runtime);

/* Run the FIR from its linked location over linker-placed coefficients and
 * scratch, and trace the layout; 0 when the result is correct. */
int sdos_layout_check(void);

/* Identity of this build, filled in from CMake definitions. */
void sdos_identity_init(sdos_identity *identity, uint8_t runtime_kind);

#ifdef __cplusplus
}
#endif

#endif /* SDOS_PLATFORM_H */
