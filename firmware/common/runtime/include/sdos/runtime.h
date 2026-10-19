/*
 * DSP-side request handling, common to every core and both runtimes.
 *
 * Firmware supplies the identity, the shared region, its algorithms, and the
 * cache and clock hooks; `sdos_runtime_handle` does the rest. The host-side
 * mock links this same file, so PC tests exercise the code the DSPs run.
 */
#ifndef SDOS_RUNTIME_H
#define SDOS_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include "sdos/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* What an algorithm receives. Input and output may not overlap. */
typedef struct sdos_run_args {
    const uint8_t *input;
    uint32_t input_length;
    uint8_t *output;
    uint32_t output_capacity;
    const uint8_t *params;
    uint16_t params_length;
} sdos_run_args;

/* Returns SDOS_OK and the number of output bytes, or an error status. */
typedef sdos_status (*sdos_algorithm_fn)(const sdos_run_args *args, uint32_t *output_length);

typedef struct sdos_algorithm {
    uint32_t id;
    const char *name;
    sdos_algorithm_fn run;
} sdos_algorithm;

typedef struct sdos_runtime {
    sdos_identity identity;
    uint8_t *region;      /* shared buffer region as this core addresses it */
    uint32_t region_size;
    const sdos_algorithm *algorithms;
    size_t algorithm_count;
    /* Cache maintenance over a range of the region; NULL when coherent. */
    void (*cache_invalidate)(void *address, size_t length);
    void (*cache_writeback)(void *address, size_t length);
    uint64_t (*cycles)(void);    /* free-running cycle counter; NULL reports 0 */
    uint32_t (*uptime_ms)(void); /* NULL reports 0 */
    uint32_t cycles_per_us;      /* 0: budgets and the watchdog are not measured */
    /* stats.state and the fault fields are the fault record; see
     * sdos_runtime_fault. A monitor task may write them concurrently with the
     * request handler, so they are only ever set, never cleared. */
    sdos_stats stats;
    volatile int busy;
    volatile uint64_t run_start;  /* cycles when the current RUN started */
    volatile uint32_t run_algorithm;
    /* Per-instance scratch, so no request state lives on a small DSP stack
     * and two runtimes in one process (the host mock) never share it. */
    sdos_run_request request;
    uint8_t payload[SDOS_PAYLOAD_MAX];
} sdos_runtime;

/* Handle one request and write the response. Returns the response length,
 * or 0 when nothing can be answered (the request was shorter than a header). */
size_t sdos_runtime_handle(sdos_runtime *runtime, const uint8_t *request, size_t length, uint8_t *response,
                           size_t capacity);

/* Record a fault and enter the safe state: RUN is refused with SDOS_E_FAULTED
 * while HELLO and STATUS still answer. The first fault is kept and later ones
 * are dropped: the first is the cause, the rest are usually consequences.
 * Only a restart clears it. */
void sdos_runtime_fault(sdos_runtime *runtime, sdos_fault fault, uint64_t address, uint32_t info);

/* Start-up checks passed: accept RUN, unless a fault was recorded. */
void sdos_runtime_ready(sdos_runtime *runtime);

/* Periodic supervision, from a monitor task or the bare-metal loop: advances
 * the heartbeat and faults a RUN that has exceeded SDOS_WATCHDOG_US. */
void sdos_runtime_monitor(sdos_runtime *runtime);

/* Algorithms every course firmware carries. */
#define SDOS_ALG_COPY 1u     /* output = input */
#define SDOS_ALG_GAIN_Q15 2u /* params: int16 gain (Q15); samples: int16 */
#define SDOS_ALG_FIR_Q15 3u  /* params: uint16 taps, then taps int16 Q15 coefficients; samples: int16 */
#define SDOS_FIR_MAX_TAPS ((SDOS_RUN_PARAMS_MAX - 2u) / 2u)

extern const sdos_algorithm sdos_builtin_algorithms[];
extern const size_t sdos_builtin_algorithm_count;

#ifdef __cplusplus
}
#endif

#endif /* SDOS_RUNTIME_H */
