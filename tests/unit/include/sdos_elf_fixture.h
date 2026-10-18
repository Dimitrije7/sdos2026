/* Write a minimal little-endian ELF32 firmware file for tests: chosen machine,
 * PT_LOAD segments, and a .resource_table with an RPMsg vdev. Mirrors
 * ci/test_firmware_elf.py so no TI binary is needed. */
#ifndef SDOS_ELF_FIXTURE_H
#define SDOS_ELF_FIXTURE_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void sdos_fx_put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void sdos_fx_put32(uint8_t *p, uint32_t v)
{
    sdos_fx_put16(p, v & 0xFFFFu);
    sdos_fx_put16(p + 2, v >> 16);
}

/* Returns 0 on success. `load_address` holds one segment of `load_size` bytes. */
static int sdos_write_elf(const char *path, uint16_t machine, uint32_t load_address, uint32_t load_size)
{
    static const char names[] = "\0.shstrtab\0.resource_table";
    uint8_t elf[512];
    uint8_t *table;
    uint8_t *section;
    FILE *file;
    const uint32_t phoff = 52u, table_off = 84u, table_size = 16u + 4u + 4u + 24u + 40u;
    const uint32_t names_off = table_off + table_size, shoff = 256u;

    memset(elf, 0, sizeof elf);
    memcpy(elf, "\x7f" "ELF\x01\x01\x01", 7u);
    sdos_fx_put16(elf + 16, 2u);
    sdos_fx_put16(elf + 18, machine);
    sdos_fx_put32(elf + 20, 1u);
    sdos_fx_put32(elf + 24, load_address);
    sdos_fx_put32(elf + 28, phoff);
    sdos_fx_put32(elf + 32, shoff);
    sdos_fx_put16(elf + 40, 52u);
    sdos_fx_put16(elf + 42, 32u);
    sdos_fx_put16(elf + 44, 1u);
    sdos_fx_put16(elf + 46, 40u);
    sdos_fx_put16(elf + 48, 3u);
    sdos_fx_put16(elf + 50, 1u);
    /* one PT_LOAD */
    sdos_fx_put32(elf + phoff, 1u);
    sdos_fx_put32(elf + phoff + 4, table_off);
    sdos_fx_put32(elf + phoff + 8, load_address);
    sdos_fx_put32(elf + phoff + 12, load_address);
    sdos_fx_put32(elf + phoff + 20, load_size);
    sdos_fx_put32(elf + phoff + 24, 6u);
    /* resource table: version 1, one entry: an RPMsg vdev with two vrings */
    table = elf + table_off;
    sdos_fx_put32(table, 1u);
    sdos_fx_put32(table + 4, 1u);
    sdos_fx_put32(table + 16, 20u);
    sdos_fx_put32(table + 20, 3u);  /* RSC_VDEV */
    sdos_fx_put32(table + 24, 7u);  /* VIRTIO_ID_RPMSG */
    table[20 + 25] = 2u;            /* two vrings */
    memcpy(elf + names_off, names, sizeof names);
    /* sections: null, .shstrtab, .resource_table */
    section = elf + shoff + 40u;
    sdos_fx_put32(section, 1u);
    sdos_fx_put32(section + 4, 3u);
    sdos_fx_put32(section + 16, names_off);
    sdos_fx_put32(section + 20, (uint32_t)sizeof names);
    section += 40u;
    sdos_fx_put32(section, 11u);
    sdos_fx_put32(section + 4, 1u);
    sdos_fx_put32(section + 12, load_address);
    sdos_fx_put32(section + 16, table_off);
    sdos_fx_put32(section + 20, table_size);
    file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }
    if (fwrite(elf, 1u, shoff + 3u * 40u, file) != shoff + 3u * 40u) {
        (void)fclose(file);
        return -1;
    }
    return fclose(file);
}

#endif /* SDOS_ELF_FIXTURE_H */
