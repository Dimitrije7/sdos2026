/*
 * libsdos_dsp: control the BeagleBone AI-64 DSPs from a Linux application.
 *
 * A DSP is named by its logical name (c66x_0, c66x_1, c71x_0), never by a
 * remoteprocN number. The library hides remoteproc sysfs and rpmsg_char, and
 * speaks protocol 2.0 (docs/protocol.md) to the firmware.
 *
 * Two backends share this API: "linux" drives the real kernel interfaces, and
 * "mock" runs the DSP runtime in-process so applications and tests work on a
 * PC. Every call returns an sdos_status; sdos_dsp_error() gives the detail.
 * A handle is not thread-safe; use one handle per thread.
 */
#ifndef SDOS_DSP_H
#define SDOS_DSP_H

#include <stddef.h>
#include <stdint.h>

#include "sdos/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDOS_DSP_API_VERSION 0x000200u /* 0.2.0: provisional until Task 027 is accepted */

typedef struct sdos_dsp sdos_dsp;

typedef enum sdos_dsp_state {
    SDOS_DSP_ABSENT = 0,  /* the device does not exist on this system */
    SDOS_DSP_OFFLINE = 1,
    SDOS_DSP_RUNNING = 2,
    SDOS_DSP_CRASHED = 3,
    SDOS_DSP_OTHER = 4    /* any other remoteproc state, such as attached */
} sdos_dsp_state;

typedef struct sdos_dsp_options {
    const char *backend;  /* "linux" (default) or "mock" */
    const char *root;     /* filesystem root for sysfs, /dev, and /lib/firmware; default "/" */
    uint32_t timeout_ms;  /* per request and per state change; default 2000 */
} sdos_dsp_options;

typedef struct sdos_dsp_info {
    const char *name;     /* c66x_0 */
    const char *label;    /* C66x_0 */
    const char *device;   /* 4d80800000.dsp */
    sdos_dsp_state state;
    char remoteproc[32];  /* remoteprocN today; informational only */
    char firmware[128];   /* firmware name remoteproc will load */
} sdos_dsp_info;

/* A range of the DSP's shared buffer region, mapped into this process. */
typedef struct sdos_buffer {
    uint8_t *data;
    uint32_t offset;
    uint32_t length;
    uint32_t sequence;
    sdos_buf_state state;
    uint32_t generation; /* DSP start count when acquired; a restart frees quarantine */
} sdos_buffer;

/* The DSPs this library knows, in manifest order. */
size_t sdos_dsp_count(void);
const char *sdos_dsp_name(size_t index);

sdos_status sdos_dsp_open(const char *name, const sdos_dsp_options *options, sdos_dsp **dsp);
void sdos_dsp_close(sdos_dsp *dsp);
const char *sdos_dsp_error(const sdos_dsp *dsp);

/* Lifecycle. load needs the DSP offline and refuses an ELF whose segments
 * leave the core's carve-out. start waits until the firmware answers HELLO
 * with a compatible protocol version. recover is stop, then start. */
sdos_status sdos_dsp_info_get(sdos_dsp *dsp, sdos_dsp_info *info);
sdos_status sdos_dsp_load(sdos_dsp *dsp, const char *elf_path);
sdos_status sdos_dsp_start(sdos_dsp *dsp, sdos_identity *identity);
sdos_status sdos_dsp_stop(sdos_dsp *dsp);
sdos_status sdos_dsp_recover(sdos_dsp *dsp, sdos_identity *identity);

/* Control messages to running firmware. */
sdos_status sdos_dsp_hello(sdos_dsp *dsp, sdos_identity *identity);
sdos_status sdos_dsp_echo(sdos_dsp *dsp, const void *data, size_t length, void *reply, size_t capacity,
                          size_t *reply_length);
sdos_status sdos_dsp_status(sdos_dsp *dsp, sdos_stats *stats);

/* Shared buffers. acquire maps a range the host then owns; run hands the
 * input and output to the DSP and returns them to the host on success. A
 * timeout quarantines both until the DSP restarts. */
sdos_status sdos_dsp_buffer_acquire(sdos_dsp *dsp, uint32_t offset, uint32_t length, sdos_buffer *buffer);
sdos_status sdos_dsp_buffer_release(sdos_dsp *dsp, sdos_buffer *buffer);
sdos_status sdos_dsp_run(sdos_dsp *dsp, uint32_t algorithm, sdos_buffer *input, sdos_buffer *output,
                         const void *params, size_t params_length, sdos_run_result *result);
/* RUN with parameters only: empty input and output, so no shared buffer is
 * needed (for algorithms that take no data, such as the fault-injection
 * cases). Works on every backend, the board's included. */
sdos_status sdos_dsp_run_control(sdos_dsp *dsp, uint32_t algorithm, const void *params, size_t params_length,
                                 sdos_run_result *result);

/* Time budget for each later RUN on this handle, in microseconds; 0 (the
 * default) means the firmware's watchdog limit, SDOS_WATCHDOG_US. A RUN that
 * finishes late fails with SDOS_E_DEADLINE and its output is invalid. */
sdos_status sdos_dsp_set_run_budget(sdos_dsp *dsp, uint32_t budget_us);

/* The firmware's health line (docs/safety.md), read through the remoteproc
 * trace, so it works even when the firmware no longer answers messages. A
 * line being rewritten while read is read again. */
sdos_status sdos_dsp_health(sdos_dsp *dsp, sdos_health *health);

/* The firmware trace buffer as text, newest data last. */
sdos_status sdos_dsp_trace(sdos_dsp *dsp, char *text, size_t capacity, size_t *length);

const char *sdos_dsp_state_name(sdos_dsp_state state);

#ifdef __cplusplus
}
#endif

#endif /* SDOS_DSP_H */
