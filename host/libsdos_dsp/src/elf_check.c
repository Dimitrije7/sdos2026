/* Firmware ELF check before loading: the C twin of ci/firmware_elf.py.
 *
 * remoteproc copies each loadable segment to its physical address. A segment
 * outside the core's carve-out would overwrite memory that belongs to Linux or
 * another core, so such a file never reaches the firmware directory. */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

#define MAX_ELF (64u * 1024u * 1024u)
#define PT_LOAD 1u
#define RSC_VDEV 3u
#define VIRTIO_ID_RPMSG 7u

static uint16_t u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (uint16_t)(p[1] << 8));
}

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)u16(p) | ((uint32_t)u16(p + 2) << 16);
}

static uint64_t u64(const uint8_t *p)
{
    return (uint64_t)u32(p) | ((uint64_t)u32(p + 4) << 32);
}

static int in_bounds(size_t size, uint64_t offset, uint64_t length)
{
    return offset <= size && length <= size - offset;
}

static sdos_status check_resource_table(const uint8_t *table, size_t size, char *why, size_t capacity)
{
    uint32_t count;
    uint32_t i;
    if (size < 16u || u32(table) != 1u) {
        (void)snprintf(why, capacity, "resource table is not version 1");
        return SDOS_E_FIELD;
    }
    count = u32(table + 4);
    if (count > (size - 16u) / 4u) {
        (void)snprintf(why, capacity, "resource table claims %u entries", (unsigned)count);
        return SDOS_E_FIELD;
    }
    for (i = 0u; i < count; i++) {
        uint32_t offset = u32(table + 16u + 4u * i);
        if (!in_bounds(size, offset, 28u)) {
            continue;
        }
        if (u32(table + offset) == RSC_VDEV && u32(table + offset + 4u) == VIRTIO_ID_RPMSG &&
            table[offset + 25u] == 2u) {
            return SDOS_OK;
        }
    }
    (void)snprintf(why, capacity, "resource table has no RPMsg vdev with two vrings");
    return SDOS_E_FIELD;
}

/* Inside one of the ranges remoteproc may load for this core. */
static int loadable(size_t core_index, uint64_t address, uint64_t memsz)
{
    size_t r;
    for (r = 0u; r < SDOS_LOAD_RANGE_COUNT; r++) {
        uint64_t lo = sdos_load_ranges[r].base;
        uint64_t span = sdos_load_ranges[r].size;
        if (sdos_load_ranges[r].core == core_index && address >= lo && memsz <= span && address - lo <= span - memsz) {
            return 1;
        }
    }
    return 0;
}

static void segment(const uint8_t *elf, int is64, uint64_t header, uint32_t *kind, uint64_t *address, uint64_t *memsz)
{
    const uint8_t *ph = elf + header;
    *kind = u32(ph);
    *address = is64 ? u64(ph + 24) : u32(ph + 12);
    *memsz = is64 ? u64(ph + 40) : u32(ph + 20);
}

static sdos_status check(const uint8_t *elf, size_t size, size_t core_index, char *why, size_t capacity)
{
    const sdos_core_desc *core = &sdos_core_table[core_index];
    int is64;
    uint64_t phoff;
    uint64_t shoff;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
    uint16_t i;
    uint16_t j;
    uint64_t names_offset;

    if (size < 64u || memcmp(elf, "\x7f" "ELF", 4u) != 0 || elf[5] != 1u) {
        (void)snprintf(why, capacity, "not a little-endian ELF file");
        return SDOS_E_FIELD;
    }
    is64 = elf[4] == 2u;
    if (u16(elf + 18) != core->elf_machine) {
        (void)snprintf(why, capacity, "built for machine %u, but %s needs %u", (unsigned)u16(elf + 18), core->name,
                       (unsigned)core->elf_machine);
        return SDOS_E_FIELD;
    }
    {
        uint64_t entry = is64 ? u64(elf + 24) : u32(elf + 24);
        if (entry % core->boot_alignment != 0u) {
            (void)snprintf(why, capacity, "entry point 0x%llx is not 0x%x-aligned, so remoteproc cannot boot %s",
                           (unsigned long long)entry, (unsigned)core->boot_alignment, core->name);
            return SDOS_E_FIELD;
        }
    }
    phoff = is64 ? u64(elf + 32) : u32(elf + 28);
    shoff = is64 ? u64(elf + 40) : u32(elf + 32);
    phentsize = u16(elf + (is64 ? 54 : 42));
    phnum = u16(elf + (is64 ? 56 : 44));
    shentsize = u16(elf + (is64 ? 58 : 46));
    shnum = u16(elf + (is64 ? 60 : 48));
    shstrndx = u16(elf + (is64 ? 62 : 50));
    if (!in_bounds(size, phoff, (uint64_t)phentsize * phnum) || phentsize < (is64 ? 56u : 32u)) {
        (void)snprintf(why, capacity, "program headers are truncated");
        return SDOS_E_FIELD;
    }
    for (i = 0u; i < phnum; i++) {
        uint32_t kind;
        uint64_t address;
        uint64_t memsz;
        segment(elf, is64, phoff + (uint64_t)i * phentsize, &kind, &address, &memsz);
        if (kind != PT_LOAD || memsz == 0u) {
            continue;
        }
        if (!loadable(core_index, address, memsz)) {
            (void)snprintf(why, capacity, "segment 0x%llx+0x%llx is not in memory %s may load (manifest/dsp-cores.json)",
                           (unsigned long long)address, (unsigned long long)memsz, core->name);
            return SDOS_E_DESCRIPTOR;
        }
        for (j = (uint16_t)(i + 1u); j < phnum; j++) {
            uint32_t other_kind;
            uint64_t other;
            uint64_t other_size;
            segment(elf, is64, phoff + (uint64_t)j * phentsize, &other_kind, &other, &other_size);
            if (other_kind == PT_LOAD && other_size != 0u && address < other + other_size && other < address + memsz) {
                (void)snprintf(why, capacity, "segments 0x%llx and 0x%llx overlap", (unsigned long long)address,
                               (unsigned long long)other);
                return SDOS_E_DESCRIPTOR;
            }
        }
    }
    if (shoff == 0u || shstrndx >= shnum || !in_bounds(size, shoff, (uint64_t)shentsize * shnum) ||
        shentsize < (is64 ? 64u : 40u)) {
        (void)snprintf(why, capacity, "section headers are missing or truncated");
        return SDOS_E_FIELD;
    }
    {
        const uint8_t *names = elf + shoff + (uint64_t)shstrndx * shentsize;
        names_offset = is64 ? u64(names + 24) : u32(names + 16);
    }
    for (i = 0u; i < shnum; i++) {
        const uint8_t *sh = elf + shoff + (uint64_t)i * shentsize;
        uint64_t name = names_offset + u32(sh);
        uint64_t offset = is64 ? u64(sh + 24) : u32(sh + 16);
        uint64_t length = is64 ? u64(sh + 32) : u32(sh + 20);
        static const char wanted[] = ".resource_table";
        if (in_bounds(size, name, sizeof wanted) && memcmp(elf + name, wanted, sizeof wanted) == 0) {
            if (!in_bounds(size, offset, length)) {
                (void)snprintf(why, capacity, "resource table is truncated");
                return SDOS_E_FIELD;
            }
            return check_resource_table(elf + offset, (size_t)length, why, capacity);
        }
    }
    (void)snprintf(why, capacity, "no .resource_table section; remoteproc cannot set up RPMsg");
    return SDOS_E_FIELD;
}

sdos_status sdos__elf_check(const char *path, size_t core_index, char *why, size_t capacity)
{
    FILE *file = fopen(path, "rb");
    uint8_t *data;
    long size;
    sdos_status status;

    if (file == NULL) {
        (void)snprintf(why, capacity, "cannot open the file");
        return SDOS_E_TRANSPORT;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 || (unsigned long)size > MAX_ELF ||
        fseek(file, 0, SEEK_SET) != 0) {
        (void)fclose(file);
        (void)snprintf(why, capacity, "file size is not readable or exceeds %u bytes", (unsigned)MAX_ELF);
        return SDOS_E_LENGTH;
    }
    data = malloc((size_t)size + 1u);
    if (data == NULL || fread(data, 1u, (size_t)size, file) != (size_t)size) {
        free(data);
        (void)fclose(file);
        (void)snprintf(why, capacity, "cannot read the file");
        return SDOS_E_TRANSPORT;
    }
    (void)fclose(file);
    status = check(data, (size_t)size, core_index, why, capacity);
    free(data);
    return status;
}
