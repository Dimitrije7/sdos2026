/* Portable safety mechanisms. See sdos/safety.h and docs/safety.md. */
#include "sdos/safety.h"

#include <string.h>

void sdos_stack_fill(uint8_t *base, size_t size)
{
    memset(base, (int)SDOS_STACK_FILL, size);
}

size_t sdos_stack_unused(const sdos_stack *stack)
{
    size_t n = 0u;
    while (n < stack->size && stack->base[n] == (uint8_t)SDOS_STACK_FILL) {
        n++;
    }
    return n;
}

static uint32_t get32(const volatile uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

sdos_status sdos_integrity_load(sdos_integrity *it, const volatile uint8_t *record, size_t size)
{
    uint8_t copy[SDOS_INTEGRITY_SIZE];
    uint32_t count;
    uint32_t crc;
    uint32_t i;

    memset(it, 0, sizeof *it);
    if (record == NULL || size < SDOS_INTEGRITY_SIZE) {
        return SDOS_E_MAGIC;
    }
    for (i = 0u; i < SDOS_INTEGRITY_SIZE; i++) {
        copy[i] = record[i];
    }
    if (get32(copy) != SDOS_INTEGRITY_MAGIC) {
        return SDOS_E_MAGIC;
    }
    count = get32(copy + 8);
    if (get32(copy + 4) != SDOS_INTEGRITY_VERSION || count > SDOS_INTEGRITY_MAX) {
        return SDOS_E_FIELD;
    }
    if (count == 0u) {
        return SDOS_E_STATE;
    }
    crc = sdos_crc32c(0u, copy, 12u);
    crc = sdos_crc32c(crc, copy + 16, 16u * count);
    if (crc != get32(copy + 12)) {
        return SDOS_E_CHECKSUM;
    }
    for (i = 0u; i < count; i++) {
        const uint8_t *r = copy + 16u + 16u * i;
        it->range[i].address = (uint64_t)get32(r) | ((uint64_t)get32(r + 4) << 32);
        it->range[i].length = get32(r + 8);
        it->range[i].crc = get32(r + 12);
    }
    it->count = count;
    return SDOS_OK;
}

int sdos_integrity_step(sdos_integrity *it, size_t budget, uint32_t *failed)
{
    while (budget > 0u && it->count > 0u) {
        const sdos_integrity_range *r = &it->range[it->next];
        const uint8_t *bytes = (const uint8_t *)(uintptr_t)r->address;
        uint32_t chunk = r->length - it->offset;
        if ((size_t)chunk > budget) {
            chunk = (uint32_t)budget;
        }
        it->crc = sdos_crc32c(it->crc, bytes + it->offset, chunk);
        it->offset += chunk;
        budget -= chunk;
        if (it->offset < r->length) {
            continue;
        }
        if (it->crc != r->crc) {
            *failed = it->next;
            it->next = 0u;
            it->offset = 0u;
            it->crc = 0u;
            return -1;
        }
        it->offset = 0u;
        it->crc = 0u;
        it->next++;
        if (it->next == it->count) {
            it->next = 0u;
            it->passes++;
            return 1;
        }
    }
    return 0;
}

size_t sdos_span_subtract(sdos_span whole, const sdos_span *holes, size_t hole_count, sdos_span *out,
                          size_t capacity)
{
    size_t n = 0u;
    uint64_t at = whole.start;
    while (at < whole.end) {
        /* The hole that starts lowest at or after `at`, or covers it. */
        uint64_t next_hole = whole.end;
        uint64_t hole_end = whole.end;
        size_t i;
        for (i = 0u; i < hole_count; i++) {
            if (holes[i].end > at && holes[i].start < next_hole && holes[i].start < holes[i].end) {
                next_hole = holes[i].start > at ? holes[i].start : at;
                hole_end = holes[i].end;
            }
        }
        if (next_hole > at) {
            if (n == capacity) {
                return SIZE_MAX;
            }
            out[n].start = at;
            out[n].end = next_hole;
            n++;
        }
        at = hole_end > next_hole ? hole_end : next_hole;
    }
    return n;
}

size_t sdos_mpax_split(uint64_t start, uint64_t end, sdos_mpax_block *out, size_t capacity)
{
    const uint64_t page = (uint64_t)1u << SDOS_MPAX_MIN_LOG2;
    size_t n = 0u;
    if ((start & (page - 1u)) != 0u || (end & (page - 1u)) != 0u || end > ((uint64_t)1u << 32) || start > end) {
        return SIZE_MAX;
    }
    while (start < end) {
        uint32_t log2 = SDOS_MPAX_MIN_LOG2;
        /* Grow while the next size is still aligned at `start` and fits. */
        while (log2 < 32u && (start & (((uint64_t)1u << (log2 + 1u)) - 1u)) == 0u &&
               start + ((uint64_t)1u << (log2 + 1u)) <= end) {
            log2++;
        }
        if (n == capacity) {
            return SIZE_MAX;
        }
        out[n].base = (uint32_t)start;
        out[n].size_log2 = log2;
        n++;
        start += (uint64_t)1u << log2;
    }
    return n;
}
