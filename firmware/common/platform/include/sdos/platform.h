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

#include "sdos/protocol.h"
#include "sdos/runtime.h"
#include "sdos/safety.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Family adapter (firmware/c66x, firmware/c71x). */
void sdos_platform_init(void); /* caches or MMU and the cycle counter; before IPC */
void sdos_cache_invalidate(void *address, size_t length);
void sdos_cache_writeback(void *address, size_t length); /* write back and invalidate */
/* Soft-reset this core's DM timer (the FreeRTOS tick), first thing at start.
 * The timer stays powered when Linux stops the core, so a previous FreeRTOS
 * run leaves it counting with its interrupt pending; on the board that
 * interrupt stopped the next start before its first trace line (Task 021). */
void sdos_tick_timer_reset(void);
uint64_t sdos_cycles(void);
uint32_t sdos_uptime_ms(void);
uint32_t sdos_cycles_per_us(void);

/* Memory protection (docs/safety.md). init configures it, or confirms what
 * the C71x boot code configured, and runs its self-test; it records any
 * failure as a fault. poll reports violations the hardware recorded without
 * stopping the CPU. Both return SDOS_CAP_PROTECTION when protection is on. */
uint32_t sdos_protection_init(sdos_runtime *runtime);
void sdos_protection_poll(sdos_runtime *runtime);
/* Trace the protection hardware's raw state (segments, fault registers), for
 * diagnosing a failed self-test or a start-up step the protection refused. */
void sdos_protection_report(void);
/* After start-up: protect the lowest SDOS_GUARD_PAGE bytes of each stack
 * against writes (C71x MMU). FreeRTOS fills a task stack when it creates the
 * task, so the guard can only go up afterwards. Known gap (Task 042): TI's C7x
 * exception entry runs the handler on the faulting stack, so an overflow into
 * the guard faults again inside the handler and the core hangs; the fault is
 * contained but not reported. */
void sdos_protection_guard(const sdos_stack *stacks, size_t count);
#if defined(BUILD_C7X)
#define SDOS_GUARD_PAGE 0x1000u
#else
#define SDOS_GUARD_PAGE 0u /* no MMU: the monitor's fill check guards C66x stacks */
#endif
/* After Linux asked this core to shut down and was acknowledged: interrupts
 * off, then wait in the CPU's low-power idle state for good. A C71x must be
 * idle before the device manager can power it off; a busy C71x left its
 * subsystem stuck in transition on the board (Task 021). */
void sdos_idle_forever(void);
/* Bare metal, between two polls: let the CPU idle until the next event. The
 * device manager powers a C71x off only while it idles, and Linux never
 * announces the stop, so a C71x that polls without pause cannot be stopped
 * (board, Task 021). C71x: idle until a 1 ms wake tick; C66x: returns at once,
 * Linux stops it by local reset. */
void sdos_wait_for_event(void);
/* Stop this core for good after a fatal fault: interrupts off, health line
 * written, nothing else runs. */
void sdos_halt(void);

/* Runtime adapter (firmware/runtime/<rtos>): free bytes of the kernel heap,
 * or SIZE_MAX where there is no kernel heap. */
size_t sdos_kernel_heap_free(void);

/* Trace buffer that Linux reads through the resource table (dspctl trace).
 * The first SDOS_HEALTH_SIZE bytes are the health line; the log follows. */
#define SDOS_TRACE_SIZE 0x10000u
extern char sdos_trace_buffer[SDOS_TRACE_SIZE];
void sdos_trace(const char *format, ...);
void sdos_trace_health(const sdos_health *health);

/* Monitor (common/platform/src/monitor.c). */
#define SDOS_MONITOR_PERIOD_MS 20u
#define SDOS_INTEGRITY_STEP_BYTES 16384u /* per monitor period */
/* After sdos_platform_init: fills and registers the guarded stacks, loads the
 * integrity record and checks the whole image once, configures protection.
 * Returns the safety capabilities established. */
uint32_t sdos_monitor_init(sdos_runtime *runtime);
/* Register a stack the runtime created, after filling it. */
void sdos_monitor_stack(const char *name, void *base, size_t size);
/* Start-up is over: from now on the kernel heap must not change. */
void sdos_monitor_started(void);
/* One period of supervision; runs from a monitor task or the bare-metal loop. */
void sdos_monitor_tick(void);
/* Bare metal: tick when a period has passed since the last tick. */
void sdos_monitor_poll(void);
/* Halt if a RUN has exceeded the watchdog limit; safe in an interrupt. */
void sdos_monitor_watchdog(void);
/* Record a fatal fault, publish it, and halt. Not from an exception handler on
 * the C71x: there, record it and leave the handler first (c71x.c). */
void sdos_monitor_fatal(sdos_fault fault, uint64_t address, uint32_t info);
/* The recording half of sdos_monitor_fatal: the record is HALTED. */
void sdos_monitor_record_fatal(sdos_fault fault, uint64_t address, uint32_t info);
/* Index of the registered stack whose guard page holds `address`, or -1. */
int sdos_monitor_guarded(uint64_t address);
/* The registered stack that holds `address`, or NULL. */
const sdos_stack *sdos_monitor_stack_containing(const void *address);

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
