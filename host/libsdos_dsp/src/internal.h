/* Shared between the session logic and the backends; not installed. */
#ifndef SDOS_DSP_INTERNAL_H
#define SDOS_DSP_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "sdos/cores.h"
#include "sdos/dsp.h"

#define SDOS_PATH_MAX 512

typedef struct sdos_backend_ops {
    const char *name;
    /* State, remoteproc name, and firmware name; ABSENT when the device is missing. */
    sdos_status (*query)(sdos_dsp *dsp, sdos_dsp_info *info);
    /* Copy a checked ELF into the firmware directory and select it. */
    sdos_status (*install)(sdos_dsp *dsp, const char *elf_path);
    /* Start (1) or stop (0) and wait for the state to settle. */
    sdos_status (*set_running)(sdos_dsp *dsp, int running);
    sdos_status (*connect)(sdos_dsp *dsp);
    void (*disconnect)(sdos_dsp *dsp);
    sdos_status (*send)(sdos_dsp *dsp, const uint8_t *message, size_t length);
    sdos_status (*receive)(sdos_dsp *dsp, uint8_t *message, size_t capacity, size_t *length, uint32_t timeout_ms);
    sdos_status (*map_region)(sdos_dsp *dsp, uint8_t **base, uint32_t *size);
    /* Make host writes visible to the DSP, or DSP writes visible to the host. */
    void (*sync_for_device)(sdos_dsp *dsp, uint32_t offset, uint32_t length);
    void (*sync_for_cpu)(sdos_dsp *dsp, uint32_t offset, uint32_t length);
    sdos_status (*trace)(sdos_dsp *dsp, char *text, size_t capacity, size_t *length);
    void (*destroy)(sdos_dsp *dsp);
} sdos_backend_ops;

struct sdos_dsp {
    const sdos_core_desc *core;
    size_t core_index;       /* position in sdos_core_table, in manifest order */
    const sdos_backend_ops *ops;
    void *backend;
    char root[SDOS_PATH_MAX];
    uint32_t timeout_ms;
    uint32_t next_request;
    uint32_t generation;
    uint32_t stale_responses;
    int connected;
    int have_identity;
    sdos_identity identity;
    uint8_t *region;
    uint32_t region_size;
    char error[256];
};

/* Record a detailed error message and return `status`. */
sdos_status sdos__fail(sdos_dsp *dsp, sdos_status status, const char *format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

/* Check that an ELF targets this core and loads only where remoteproc may load
 * for it (manifest/dsp-cores.json), without overlapping segments. */
sdos_status sdos__elf_check(const char *path, size_t core_index, char *why, size_t capacity);

uint64_t sdos__now_ms(void);

/* snprintf that reports truncation: 0 on success, -1 when `out` was too small. */
int sdos__format(char *out, size_t capacity, const char *format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

sdos_status sdos__linux_init(sdos_dsp *dsp);
sdos_status sdos__mock_init(sdos_dsp *dsp);

/* Test seam: how the Linux backend opens an RPMsg endpoint. The default uses
 * the rpmsg_char ioctls; tests substitute a socketpair. Returns an fd or -1. */
typedef int (*sdos_rpmsg_open_fn)(sdos_dsp *dsp, const char *ctrl_path, uint32_t endpoint, char *why, size_t capacity);
void sdos__linux_set_rpmsg_open(sdos_rpmsg_open_fn open_fn);

#endif /* SDOS_DSP_INTERNAL_H */
