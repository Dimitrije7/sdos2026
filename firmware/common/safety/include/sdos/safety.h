/*
 * Portable parts of the firmware safety mechanisms (docs/safety.md).
 *
 * Everything here is plain C99 without hardware access, so the host tests run
 * the same code the DSPs run: stack fill and margin measurement, the image
 * integrity record and its incremental check, and the address arithmetic that
 * plans the C71x MMU map and the C66x MPAX segments.
 */
#ifndef SDOS_SAFETY_H
#define SDOS_SAFETY_H

#include <stddef.h>
#include <stdint.h>

#include "sdos/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Stacks grow down. Each guarded stack is filled with SDOS_STACK_FILL before
 * use; the lowest SDOS_STACK_GUARD bytes must keep it. FreeRTOS fills task
 * stacks with the same byte. */
#define SDOS_STACK_FILL 0xA5u
#define SDOS_STACK_GUARD 256u

typedef struct sdos_stack {
    const char *name;
    const uint8_t *base; /* lowest address */
    size_t size;
} sdos_stack;

void sdos_stack_fill(uint8_t *base, size_t size);
/* Bytes at the low end that still hold the fill: the stack's margin. */
size_t sdos_stack_unused(const sdos_stack *stack);

/* Integrity record, written into the ELF after linking by
 * ci/firmware_postlink.py. Little-endian bytes:
 *   0 magic "SDIR", 4 version 1, 8 range count, 12 CRC-32C of bytes 0..11
 *   followed by the ranges; then per range: u64 address, u32 length,
 *   u32 CRC-32C of the bytes at that address. */
#define SDOS_INTEGRITY_MAGIC 0x52494453u
#define SDOS_INTEGRITY_VERSION 1u
#define SDOS_INTEGRITY_MAX 32u
#define SDOS_INTEGRITY_SIZE (16u + 16u * SDOS_INTEGRITY_MAX)

typedef struct sdos_integrity_range {
    uint64_t address;
    uint32_t length;
    uint32_t crc;
} sdos_integrity_range;

typedef struct sdos_integrity {
    sdos_integrity_range range[SDOS_INTEGRITY_MAX];
    uint32_t count;
    uint32_t next;   /* range being checked */
    uint32_t offset; /* bytes of it already checked */
    uint32_t crc;    /* running CRC of those bytes */
    uint32_t passes; /* completed clean passes over every range */
} sdos_integrity;

/* SDOS_OK; SDOS_E_MAGIC without a record; SDOS_E_STATE for a record the
 * post-link step never filled; SDOS_E_FIELD or SDOS_E_CHECKSUM for a damaged
 * one. The record is read through a volatile pointer: it is patched after
 * compilation, so its compiled-in contents must never be assumed. */
sdos_status sdos_integrity_load(sdos_integrity *it, const volatile uint8_t *record, size_t size);

/* Check up to `budget` bytes. Returns 1 when this step completed a clean
 * pass, 0 while a pass is under way, and -1 when a range does not match; then
 * *failed is its index. A mismatch restarts the pass. */
int sdos_integrity_step(sdos_integrity *it, size_t budget, uint32_t *failed);

/* Address spans [start, end). */
typedef struct sdos_span {
    uint64_t start;
    uint64_t end;
} sdos_span;

/* `whole` minus every hole, in address order. Returns the number of spans
 * written, or SIZE_MAX when they do not fit in `capacity`. */
size_t sdos_span_subtract(sdos_span whole, const sdos_span *holes, size_t hole_count, sdos_span *out,
                          size_t capacity);

/* C66x MPAX segments are naturally aligned powers of two of at least 4 KiB.
 * Cover [start, end) exactly with the fewest of them. Returns the count, or
 * SIZE_MAX when the span is not 4 KiB-aligned or needs more than `capacity`. */
typedef struct sdos_mpax_block {
    uint32_t base;
    uint32_t size_log2; /* 12 (4 KiB) to 32 (4 GiB) */
} sdos_mpax_block;

#define SDOS_MPAX_MIN_LOG2 12u
size_t sdos_mpax_split(uint64_t start, uint64_t end, sdos_mpax_block *out, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* SDOS_SAFETY_H */
