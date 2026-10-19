/*
 * SDOS.cmd: main linker command file for C71x_0 course firmware (TI C7000
 * linker syntax). This file is yours to edit: memory regions, section
 * placement, ordering, alignment, symbols, and load/run addresses are all
 * native linker statements. Point CMake at another file with
 * -DSDOS_LINKER_SCRIPT=<path> to use a different layout.
 *
 * Hardware facts come from sdos_memory.cmd, generated from
 * manifest/dsp-cores.json: SDOS_<REGION>_ORIGIN, _LENGTH, and _USABLE. A
 * region's end is ORIGIN + USABLE - 1.
 *
 * MANDATORY marks a rule with a hardware, loader, or interoperability reason;
 * PROTECTION marks what the run-time memory protection relies on
 * (docs/safety.md); everything else is a default you may change.
 * ci/firmware_postlink.py and ci/firmware_elf.py check every build.
 */
#include "sdos_memory.cmd"

/* MANDATORY: TI's C7x boot code (ti.csl.init) starts here; it sets up the MMU
 * and caches and then calls the C runtime and main(). */
-e _c_int00_secure
--diag_suppress=10063   /* "entry point other than _c_int00": intended, see above */
--cinit_compression=off
--diag_suppress=10068   /* "no matching section": optional runtime sections */

-stack 0x20000   /* C stack; TI's C7x interrupt handling nests on it */
-heap  0x0       /* PROTECTION: no C heap; the post-link check refuses malloc() */
--args 0x1000

/* Split of the 15 MiB DDR carve-out. Move the boundaries freely, minding the
 * 2 MiB and 4 MiB alignments below, which leave holes: the protection zones
 * follow the boundaries. PROTECTION: keep them 4 KiB-aligned; each boundary
 * that is not 2 MiB-aligned costs one of TI's 16 MMU tables. */
#define META_ORIGIN   (SDOS_DDR_ORIGIN)
#define CODE_ORIGIN   (SDOS_DDR_ORIGIN + 0x00100000)
#define RODATA_ORIGIN (SDOS_DDR_ORIGIN + 0x00a00000)
#define DATA_ORIGIN   (SDOS_DDR_ORIGIN + 0x00b00000)
#define DDR_END       (SDOS_DDR_ORIGIN + SDOS_DDR_USABLE)
/* Local L2 SRAM: code that runs from L2 (load/run copies) first, data after.
 * PROTECTION: a multiple of 16 KiB, the L2 protection page on the C66x, kept
 * the same here. */
#define L2CODE_LENGTH 0x4000
/* MANDATORY: remoteproc cannot load into C71x L2. The J721E C71x has no local
 * reset, so Linux loads the ELF while the module is held in reset, and an A72
 * write to L2 then hangs the interconnect (seen on the board, Task 021). L2
 * holds only NOLOAD data and run copies made at start-up (table(BINIT)). */
#define SDOS_COEFFS_LOAD DDR_RODATA

MEMORY
{
    DDR_META   (RW)   : origin = META_ORIGIN,   length = CODE_ORIGIN - META_ORIGIN    /* resource table, trace, integrity record */
    DDR_CODE   (RX)   : origin = CODE_ORIGIN,   length = RODATA_ORIGIN - CODE_ORIGIN
    DDR_RODATA (R)    : origin = RODATA_ORIGIN, length = DATA_ORIGIN - RODATA_ORIGIN
    DDR_DATA   (RW)   : origin = DATA_ORIGIN,   length = DDR_END - DATA_ORIGIN
    /* Local L2 SRAM, private to this core. */
    L2CODE     (RX)   : origin = SDOS_L2SRAM_ORIGIN,                 length = L2CODE_LENGTH
    L2SRAM     (RW)   : origin = SDOS_L2SRAM_ORIGIN + L2CODE_LENGTH, length = SDOS_L2SRAM_USABLE - L2CODE_LENGTH
}

/* PROTECTION: the zones the MMU maps read-only and executable (code, in DDR
 * and in L2) and read-only (constants). The rest of the carve-out and of L2 is
 * writable and never executable; nothing else in DDR is mapped. */
sdos_zone_code_start   = CODE_ORIGIN;
sdos_zone_code_end     = RODATA_ORIGIN;
sdos_zone_rodata_start = RODATA_ORIGIN;
sdos_zone_rodata_end   = DATA_ORIGIN;
sdos_zone_l2code_start = SDOS_L2SRAM_ORIGIN;
sdos_zone_l2code_end   = SDOS_L2SRAM_ORIGIN + L2CODE_LENGTH;
sdos_zone_l2_end       = SDOS_L2SRAM_ORIGIN + SDOS_L2SRAM_USABLE;

SECTIONS
{
    /* MANDATORY: remoteproc finds the resource table by this section name and
     * reads it before starting the core, so it must be in loaded memory. */
    .resource_table : {} > DDR_META, align = 0x1000
    /* MANDATORY: Linux reads the trace, and the health line at its start,
     * through remoteproc, which maps only loadable memory. */
    .tracebuf : {} > DDR_META, align = 0x400   /* zeroed at start, so the trace reads cleanly */
    /* PROTECTION: the CRC record of the read-only image, written after linking. */
    .sdos.integrity : {} > DDR_META, align = 0x8

    /* MANDATORY: the K3 remoteproc driver boots a C71x only from a 2 MiB-aligned
     * entry point; _c_int00_secure comes from TI's boot.oe71. */
    .text:_c_int00_secure : {} > DDR_CODE, align = 0x200000
    /* MANDATORY until shown otherwise: TI's validated C7x layout aligns the
     * interrupt and secure vector tables (from Hwi_disp_always.oe71) to 4 MiB
     * and 2 MiB for the C7x vector base registers. */
    .vecs                 : {} > DDR_CODE, align = 0x400000
    .secure_vecs          : {} > DDR_CODE, align = 0x200000
    /* MANDATORY: the firmware's own table (vectors.asm), installed at start;
     * its exception entries switch to the exception stack, so a stack overflow
     * is recorded. 2 MiB-aligned like .secure_vecs; the firmware checks it. */
    .sdos.vecs            : {} > DDR_CODE, align = 0x200000
    .text                 : {} > DDR_CODE

    #include "c71x_runtime.cmd"

    /* Constants and initialization tables: read-only after loading. */
    .const  : {} > DDR_RODATA
    .switch : {} > DDR_RODATA
    .cinit  : {} > DDR_RODATA
    .binit  : {} > DDR_RODATA   /* load/run copy table, processed before main() */
    .init_array : {} > DDR_RODATA

    .bss    : {} > DDR_DATA
    .data   : {} > DDR_DATA
    .args   : {} > DDR_DATA
    .cio    : {} > DDR_DATA
    /* MANDATORY: TI's C7x interrupt handling needs a 128 KiB-aligned stack.
     * PROTECTION: its lowest page becomes the read-only guard. */
    .stack  : {} > DDR_DATA, align = 0x20000

    ipc_data_buffer : type = NOLOAD {} > DDR_DATA, align = 0x8
    /* MANDATORY: FreeRTOS C7x task stacks must be 8 KiB aligned.
     * PROTECTION: each stack's lowest page becomes its read-only guard. */
    .sdos.stack.main    : type = NOLOAD {} > DDR_DATA, align = 0x2000
    .sdos.stack.monitor : type = NOLOAD {} > DDR_DATA, align = 0x2000
    .sdos.stack.ipc     : type = NOLOAD {} > DDR_DATA, align = 0x2000
    .sdos.runtime       : type = NOLOAD {} > L2SRAM   /* MANDATORY: not loadable, see above; zeroed by main() */

    #include "fir_memory.cmd"
    #include "buffers_memory.cmd"
}
