/*
 * Monitor: the firmware's periodic self-supervision (docs/safety.md).
 *
 * Each period it advances the heartbeat, checks the watchdog, measures every
 * guarded stack, checks a slice of the code and read-only data against the
 * integrity record, collects memory-protection violations, checks that the
 * kernel heap no longer changes, and rewrites the health line at the start of
 * the trace buffer. FreeRTOS runs it in a task above the request handler;
 * bare metal runs it from the polling loop.
 */
#include <string.h>

#include "sdos/platform.h"
#include "sdos/safety.h"

/* Patched after linking by ci/firmware_postlink.py; see sdos/safety.h. The
 * initializer is only the marker the post-link step looks for. */
const uint8_t sdos_integrity_record[SDOS_INTEGRITY_SIZE]
    __attribute__((section(".sdos.integrity"), aligned(8))) = {0x53u, 0x44u, 0x49u, 0x52u, 1u};
static const volatile uint8_t *const integrity_record = sdos_integrity_record;

/* The C stack, from TI's linker. Read through pointers: on the C71x, direct
 * references to absolute symbols can become out-of-range PC-relative code. */
extern char __TI_STACK_END[];
extern char __TI_STACK_SIZE[];
static char *const c_stack_end = __TI_STACK_END;
static char *const c_stack_size = __TI_STACK_SIZE;

#define MAX_STACKS 6u

static struct {
    sdos_runtime *runtime;
    sdos_stack stacks[MAX_STACKS];
    size_t stack_count;
    sdos_integrity integrity;
    int integrity_ok;
    size_t heap_baseline;
    int started;
    uint32_t sequence;
    uint32_t last_tick_ms;
} m;

void sdos_monitor_stack(const char *name, void *base, size_t size)
{
    if (m.stack_count < MAX_STACKS) {
        m.stacks[m.stack_count].name = name;
        m.stacks[m.stack_count].base = (const uint8_t *)base;
        m.stacks[m.stack_count].size = size;
        m.stack_count++;
    }
}

static void publish(void)
{
    const sdos_stats *s = &m.runtime->stats;
    sdos_health h;
    m.sequence++;
    h.sequence = m.sequence;
    h.heartbeat = s->heartbeat;
    h.uptime_ms = sdos_uptime_ms();
    h.state = s->state;
    h.fault = s->fault;
    h.fault_info = s->fault_info;
    h.fault_address = s->fault_address;
    h.stack_margin = s->stack_margin;
    h.integrity_passes = s->integrity_passes;
    h.deadline_misses = s->deadline_misses;
    sdos_trace_health(&h);
}

static void report_fault(sdos_fault fault, uint64_t address, uint32_t info)
{
    int first = m.runtime->stats.fault == (uint8_t)SDOS_FAULT_NONE;
    sdos_runtime_fault(m.runtime, fault, address, info);
    if (first) {
        sdos_trace("FAULT %s at 0x%08lx%08lx, info 0x%lx: safe state, RUN refused until restart",
                   sdos_fault_name((uint32_t)fault), (unsigned long)(address >> 32),
                   (unsigned long)(address & 0xFFFFFFFFu), (unsigned long)info);
    }
    publish();
}

uint32_t sdos_monitor_init(sdos_runtime *runtime)
{
    uint8_t here = 0u;
    uint8_t *stack_base = (uint8_t *)(c_stack_end - (uintptr_t)c_stack_size);
    uint32_t capabilities = SDOS_CAP_HEALTH;
    sdos_status status;
    uint32_t failed = 0u;
    int result = 0;

    m.runtime = runtime;
    runtime->stats.stack_margin = 0xFFFFFFFFu;
    /* The C stack is in use: fill only well below the current frame. */
    if (&here > stack_base + 1024u && &here < (uint8_t *)c_stack_end) {
        sdos_stack_fill(stack_base, (size_t)(&here - stack_base) - 512u);
        sdos_monitor_stack("c", stack_base, (size_t)(uintptr_t)c_stack_size);
    }
    status = sdos_integrity_load(&m.integrity, integrity_record, SDOS_INTEGRITY_SIZE);
    if (status != SDOS_OK) {
        report_fault(SDOS_FAULT_INTEGRITY, (uint64_t)(uintptr_t)sdos_integrity_record, 0x80000000u | (uint32_t)status);
    } else {
        /* One full pass before serving anything. */
        while (result == 0) {
            result = sdos_integrity_step(&m.integrity, 1u << 20, &failed);
        }
        if (result < 0) {
            report_fault(SDOS_FAULT_INTEGRITY, m.integrity.range[failed].address, failed);
        } else {
            m.integrity_ok = 1;
            runtime->stats.integrity_passes = m.integrity.passes;
            capabilities |= SDOS_CAP_INTEGRITY;
            sdos_trace("integrity: %lu read-only ranges match their CRC record", (unsigned long)m.integrity.count);
        }
    }
    capabilities |= sdos_protection_init(runtime);
    if (runtime->cycles != NULL && runtime->cycles_per_us != 0u) {
        capabilities |= SDOS_CAP_BUDGET;
    }
    m.last_tick_ms = sdos_uptime_ms();
    publish();
    return capabilities;
}

void sdos_monitor_started(void)
{
    m.heap_baseline = sdos_kernel_heap_free();
    m.started = 1;
    sdos_protection_guard(m.stacks, m.stack_count);
    sdos_runtime_ready(m.runtime);
    sdos_trace("monitor: %lu guarded stacks, kernel heap %s; ready", (unsigned long)m.stack_count,
               m.heap_baseline == SIZE_MAX ? "absent" : "frozen");
    publish();
}

void sdos_monitor_tick(void)
{
    sdos_runtime *rt = m.runtime;
    uint32_t margin = 0xFFFFFFFFu;
    uint32_t failed = 0u;
    size_t i;

    if (rt == NULL) {
        return;
    }
    sdos_runtime_monitor(rt);
    sdos_monitor_watchdog();
    for (i = 0u; i < m.stack_count; i++) {
        size_t unused = sdos_stack_unused(&m.stacks[i]);
        if (unused < margin) {
            margin = (uint32_t)unused;
        }
        if (unused < SDOS_STACK_GUARD) {
            report_fault(SDOS_FAULT_STACK, (uint64_t)(uintptr_t)m.stacks[i].base, (uint32_t)i);
        }
    }
    rt->stats.stack_margin = margin;
    if (m.integrity_ok) {
        int result = sdos_integrity_step(&m.integrity, SDOS_INTEGRITY_STEP_BYTES, &failed);
        if (result < 0) {
            report_fault(SDOS_FAULT_INTEGRITY, m.integrity.range[failed].address, failed);
        }
        rt->stats.integrity_passes = m.integrity.passes;
    }
    sdos_protection_poll(rt);
    if (m.started && m.heap_baseline != SIZE_MAX) {
        size_t now = sdos_kernel_heap_free();
        if (now != m.heap_baseline) {
            report_fault(SDOS_FAULT_HEAP, 0u, (uint32_t)(m.heap_baseline - now));
        }
    }
    publish();
}

/* A RUN past the watchdog limit never returns by itself: halt, the safe state
 * Linux recovers from. Recording the fault alone would leave the runaway
 * algorithm on the CPU, and a busy C71x cannot even be powered off by Linux
 * (board, Task 021). Also called from the bare-metal C71x wake tick. */
void sdos_monitor_watchdog(void)
{
    if (m.runtime != NULL && sdos_runtime_overdue(m.runtime)) {
        sdos_monitor_fatal(SDOS_FAULT_WATCHDOG, 0u, m.runtime->run_algorithm);
    }
}

void sdos_monitor_poll(void)
{
    uint32_t now = sdos_uptime_ms(); /* wall time: the cycle counter may stop while the CPU idles */
    if (now - m.last_tick_ms >= SDOS_MONITOR_PERIOD_MS) {
        m.last_tick_ms = now;
        sdos_monitor_tick();
    }
}

void sdos_monitor_record_fatal(sdos_fault fault, uint64_t address, uint32_t info)
{
    if (m.runtime != NULL) {
        report_fault(fault, address, info);
        m.runtime->stats.state = (uint8_t)SDOS_STATE_HALTED;
        publish();
    }
    sdos_trace("HALTED: fatal %s fault; restart the core (dspctl recover)", sdos_fault_name((uint32_t)fault));
}

void sdos_monitor_fatal(sdos_fault fault, uint64_t address, uint32_t info)
{
    sdos_monitor_record_fatal(fault, address, info);
    sdos_halt();
}

const sdos_stack *sdos_monitor_stack_containing(const void *address)
{
    size_t i;
    for (i = 0u; i < m.stack_count; i++) {
        const uint8_t *base = m.stacks[i].base;
        if ((const uint8_t *)address >= base && (const uint8_t *)address < base + m.stacks[i].size) {
            return &m.stacks[i];
        }
    }
    return NULL;
}

int sdos_monitor_guarded(uint64_t address)
{
    size_t i;
    for (i = 0u; i < m.stack_count; i++) {
        uint64_t base = (uint64_t)(uintptr_t)m.stacks[i].base;
        if (address >= base && address < base + SDOS_GUARD_PAGE) {
            return (int)i;
        }
    }
    return -1;
}
