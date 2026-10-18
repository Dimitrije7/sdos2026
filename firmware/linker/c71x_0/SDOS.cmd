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
 * everything else is a default you may change. ci/firmware_elf.py checks every
 * build against the memory contract, whatever the layout.
 */
#include "sdos_memory.cmd"

/* MANDATORY: TI's C7x boot code (ti.csl.init) starts here; it sets up the MMU
 * and caches and then calls the C runtime and main(). */
-e _c_int00_secure
--diag_suppress=10063   /* "entry point other than _c_int00": intended, see above */
--cinit_compression=off
--diag_suppress=10068   /* "no matching section": optional runtime sections */

-stack 0x20000   /* C stack; TI's C7x interrupt handling nests on it */
-heap  0x20000   /* malloc() */
--args 0x1000

MEMORY
{
    /* Default split of the 15 MiB DDR carve-out. Change the sizes freely, but
     * mind the 2 MiB and 4 MiB alignments below, which leave holes. */
    DDR_META  (RW)   : origin = SDOS_DDR_ORIGIN,              length = 0x00100000  /* resource table, trace */
    DDR_CODE  (RWIX) : origin = SDOS_DDR_ORIGIN + 0x00100000, length = 0x00900000
    DDR_DATA  (RW)   : origin = SDOS_DDR_ORIGIN + 0x00a00000, length = SDOS_DDR_USABLE - 0x00a00000
    /* Local L2 SRAM, private to this core. */
    L2SRAM    (RWIX) : origin = SDOS_L2SRAM_ORIGIN,           length = SDOS_L2SRAM_USABLE
}

SECTIONS
{
    /* MANDATORY: remoteproc finds the resource table by this section name and
     * reads it before starting the core, so it must be in loaded memory. */
    .resource_table : {} > DDR_META, align = 0x1000
    /* MANDATORY: Linux reads the trace through remoteproc, which maps only
     * loadable memory. */
    .tracebuf : {} > DDR_META, align = 0x400   /* zeroed at start, so the trace reads cleanly */

    /* MANDATORY: the K3 remoteproc driver boots a C71x only from a 2 MiB-aligned
     * entry point; _c_int00_secure comes from TI's boot.oe71. */
    .text:_c_int00_secure : {} > DDR_CODE, align = 0x200000
    /* MANDATORY until shown otherwise: TI's validated C7x layout aligns the
     * interrupt and secure vector tables (from Hwi_disp_always.oe71) to 4 MiB
     * and 2 MiB for the C7x vector base registers. */
    .vecs                 : {} > DDR_CODE, align = 0x400000
    .secure_vecs          : {} > DDR_CODE, align = 0x200000
    /* Default, copied from TI's layout without a known hardware reason. */
    .text                 : {} > DDR_CODE, align = 0x200000

    #include "c71x_runtime.cmd"

    .bss    : {} > DDR_DATA
    .data   : {} > DDR_DATA
    .cinit  : {} > DDR_DATA
    .binit  : {} > DDR_DATA   /* load/run copy table, processed before main() */
    .init_array : {} > DDR_DATA
    .const  : {} > DDR_DATA
    .switch : {} > DDR_DATA
    .args   : {} > DDR_DATA
    .cio    : {} > DDR_DATA
    .sysmem : {} > DDR_DATA
    /* MANDATORY: TI's C7x interrupt handling needs a 128 KiB-aligned stack. */
    .stack  : {} > DDR_DATA, align = 0x20000

    ipc_data_buffer : type = NOLOAD {} > DDR_DATA, align = 0x8
    /* MANDATORY: FreeRTOS C7x task stacks must be 8 KiB aligned. */
    .sdos.task_stack : type = NOLOAD {} > DDR_DATA, align = 0x2000
    .sdos.runtime    : {} > L2SRAM

    #include "fir_memory.cmd"
    #include "buffers_memory.cmd"
}
