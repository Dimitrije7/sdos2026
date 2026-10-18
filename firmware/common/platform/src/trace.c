/* Trace buffer: text lines in a ring that Linux reads as remoteproc trace0. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "sdos/platform.h"

/* SDOS.cmd places .tracebuf in memory remoteproc maps for Linux to read. */
char sdos_trace_buffer[SDOS_TRACE_SIZE] __attribute__((section(".tracebuf")));

static uint32_t trace_next;

void sdos_trace(const char *format, ...)
{
    char line[160];
    va_list args;
    int n;
    uint32_t i;
    uint32_t ms = sdos_uptime_ms();

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
        sdos_trace_buffer[trace_next] = line[i];
        trace_next = (trace_next + 1u) % (SDOS_TRACE_SIZE - 1u); /* the last byte stays NUL */
    }
    sdos_cache_writeback(sdos_trace_buffer, SDOS_TRACE_SIZE);
}
