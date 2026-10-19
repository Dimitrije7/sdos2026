/* Trace buffer that Linux reads as remoteproc trace0: the health line first,
 * then text lines in a ring.
 *
 * Linux's reader stops at the first NUL byte, so the health line is written
 * before anything else and never contains one; the ring's last byte stays NUL. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "sdos/platform.h"

/* SDOS.cmd places .tracebuf in memory remoteproc maps for Linux to read. */
char sdos_trace_buffer[SDOS_TRACE_SIZE] __attribute__((section(".tracebuf")));

#define RING_START SDOS_HEALTH_SIZE
#define RING_SIZE (SDOS_TRACE_SIZE - 1u - SDOS_HEALTH_SIZE)

static uint32_t trace_next;
static int health_written;

void sdos_trace_health(const sdos_health *health)
{
    sdos_health_format(health, sdos_trace_buffer);
    health_written = 1;
    sdos_cache_writeback(sdos_trace_buffer, SDOS_HEALTH_SIZE);
}

void sdos_trace(const char *format, ...)
{
    char line[160];
    va_list args;
    int n;
    uint32_t i;
    uint32_t ms = sdos_uptime_ms();

    if (!health_written) {
        sdos_health starting;
        memset(&starting, 0, sizeof starting);
        starting.stack_margin = 0xFFFFFFFFu;
        sdos_trace_health(&starting);
    }
    n = snprintf(line, sizeof line, "[%lu.%03lu] ", (unsigned long)(ms / 1000u), (unsigned long)(ms % 1000u));
    va_start(args, format);
    if (n > 0 && (size_t)n < sizeof line) {
        (void)vsnprintf(line + n, sizeof line - (size_t)n, format, args);
    }
    va_end(args);
    line[sizeof line - 2u] = '\0';
    n = (int)strlen(line);
    line[n] = '\n';
    for (i = 0u; i <= (uint32_t)n; i++) {
        sdos_trace_buffer[RING_START + trace_next] = line[i];
        trace_next = (trace_next + 1u) % RING_SIZE;
    }
    sdos_cache_writeback(sdos_trace_buffer + RING_START, SDOS_TRACE_SIZE - RING_START);
}
