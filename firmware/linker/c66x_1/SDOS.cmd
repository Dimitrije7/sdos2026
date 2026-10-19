/*
 * SDOS.cmd: main linker command file for C66x_1 course firmware (TI C6000
 * linker syntax). This file is yours to edit: memory regions, section
 * placement, ordering, alignment, symbols, and load/run addresses are all
 * native linker statements. Point CMake at another file with
 * -DSDOS_LINKER_SCRIPT=<path> to use a different layout.
 *
 * Hardware facts come from sdos_memory.cmd, generated from
 * manifest/dsp-cores.json: SDOS_<REGION>_ORIGIN, _LENGTH, and _USABLE (what the
 * runtime leaves after configuring cache). A region's end is ORIGIN + USABLE - 1.
 *
 * MANDATORY marks a rule with a hardware, loader, or interoperability reason;
 * PROTECTION marks what the run-time memory protection relies on
 * (docs/safety.md); everything else is a default you may change.
 * ci/firmware_postlink.py and ci/firmware_elf.py check every build.
 */
#include "sdos_memory.cmd"

#if defined(SDOS_RTOS_FREERTOS)
/* MANDATORY: TI's FreeRTOS C66x port refers to its vector table by this name. */
--symbol_map _Hwi_intcVectorTable=Hwi_intcVectorTable
#endif

-stack 0x4000   /* C stack for main() and interrupts before the scheduler */
-heap  0x0      /* PROTECTION: no C heap; the post-link check refuses malloc() */

/* Split of the 15 MiB DDR carve-out. Move the boundaries freely: the
 * protection zones below follow them. PROTECTION: keep them 4 KiB-aligned;
 * boundaries on larger powers of two need fewer of the 14 MPAX segments. */
#define META_ORIGIN   (SDOS_DDR_ORIGIN)
#define CODE_ORIGIN   (SDOS_DDR_ORIGIN + 0x00100000)
#define RODATA_ORIGIN (SDOS_DDR_ORIGIN + 0x00500000)
#define DATA_ORIGIN   (SDOS_DDR_ORIGIN + 0x00600000)
#define DDR_END       (SDOS_DDR_ORIGIN + SDOS_DDR_USABLE)

MEMORY
{
    DDR_META   (RW)   : origin = META_ORIGIN,   length = CODE_ORIGIN - META_ORIGIN    /* resource table, trace, integrity record */
    DDR_CODE   (RX)   : origin = CODE_ORIGIN,   length = RODATA_ORIGIN - CODE_ORIGIN
    DDR_RODATA (R)    : origin = RODATA_ORIGIN, length = DATA_ORIGIN - RODATA_ORIGIN
    DDR_DATA   (RW)   : origin = DATA_ORIGIN,   length = DDR_END - DATA_ORIGIN
    /* Local L2 SRAM, fast and private to this core; FreeRTOS keeps the top
     * 64 KiB as cache, so USABLE depends on the runtime. */
    L2SRAM     (RWIX) : origin = SDOS_L2SRAM_ORIGIN, length = SDOS_L2SRAM_USABLE
}

/* PROTECTION: the zones the MPAX maps read-only and executable (code) and
 * read-only (constants). The rest of the carve-out is writable and never
 * executable; the rest of DDR is denied. */
sdos_zone_code_start   = CODE_ORIGIN;
sdos_zone_code_end     = RODATA_ORIGIN;
sdos_zone_rodata_start = RODATA_ORIGIN;
sdos_zone_rodata_end   = DATA_ORIGIN;

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

    /* MANDATORY: the K3 remoteproc driver boots a C66x only from a 1 KiB-aligned
     * entry point. */
    .text:_c_int00 : {} > DDR_CODE, align = 0x400
    .text:csl_entry : {} > DDR_CODE

    #include "c66x_runtime.cmd"

    .text   : {} > DDR_CODE
    /* Constants and initialization tables: read-only after loading. */
    .const  : {} > DDR_RODATA
    .switch : {} > DDR_RODATA
    .cinit  : {} > DDR_RODATA
    .binit  : {} > DDR_RODATA   /* load/run copy table, processed before main() */
    .pinit  : {} > DDR_RODATA
    .init_array : {} > DDR_RODATA
    .c6xabi.exidx : {} > DDR_RODATA
    .c6xabi.extab : {} > DDR_RODATA
    /* MANDATORY: near (DP-relative) data must stay within one 32 KiB window of
     * the data page pointer, so .neardata, .rodata, and .bss stay together. */
    GROUP : > DDR_DATA
    {
        .neardata
        .rodata
        .bss
    }
    .far     : {} > DDR_DATA
    .fardata : {} > DDR_DATA
    .data    : {} > DDR_DATA
    .cio     : {} > DDR_DATA
    .args    : {} > DDR_DATA
    .ppinfo  : {} > DDR_DATA
    .ppdata  : {} > DDR_DATA
    .stack   : {} > L2SRAM, align = 0x8

    /* IPC buffers: RPMsg objects and virtqueue state. Default in DDR; NOLOAD
     * because the IPC library initializes them. */
    ipc_data_buffer : type = NOLOAD {} > DDR_DATA, align = 0x8

    /* The course runtime's state and the task stacks, in fast L2. Each stack
     * has its own section; the monitor checks the fill at its low end. */
    .sdos.runtime       : {} > L2SRAM
    .sdos.stack.main    : type = NOLOAD {} > L2SRAM, align = 0x8
    .sdos.stack.monitor : type = NOLOAD {} > L2SRAM, align = 0x8
    .sdos.stack.ipc     : type = NOLOAD {} > L2SRAM, align = 0x8

    #include "fir_memory.cmd"
    #include "buffers_memory.cmd"
}
