/*
 * SDOS.cmd: main linker command file for C66x_0 course firmware (TI C6000
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
 * everything else is a default you may change. ci/firmware_elf.py checks every
 * build against the memory contract, whatever the layout.
 */
#include "sdos_memory.cmd"

#if defined(SDOS_RTOS_FREERTOS)
/* MANDATORY: TI's FreeRTOS C66x port refers to its vector table by this name. */
--symbol_map _Hwi_intcVectorTable=Hwi_intcVectorTable
#endif

-stack 0x4000   /* C stack for main() and interrupts before the scheduler */
-heap  0x8000   /* malloc() */

MEMORY
{
    /* Default split of the 15 MiB DDR carve-out. Change the sizes freely. */
    DDR_META  (RW)   : origin = SDOS_DDR_ORIGIN,              length = 0x00100000  /* resource table, trace */
    DDR_CODE  (RWIX) : origin = SDOS_DDR_ORIGIN + 0x00100000, length = 0x00400000
    DDR_DATA  (RW)   : origin = SDOS_DDR_ORIGIN + 0x00500000, length = SDOS_DDR_USABLE - 0x00500000
    /* Local L2 SRAM, fast and private to this core; FreeRTOS keeps the top
     * 64 KiB as cache, so USABLE depends on the runtime. */
    L2SRAM    (RWIX) : origin = SDOS_L2SRAM_ORIGIN,           length = SDOS_L2SRAM_USABLE
}

SECTIONS
{
    /* MANDATORY: remoteproc finds the resource table by this section name and
     * reads it before starting the core, so it must be in loaded memory. */
    .resource_table : {} > DDR_META, align = 0x1000
    /* MANDATORY: Linux reads the trace through remoteproc, which maps only
     * loadable memory (manifest regions with loadable: true). */
    .tracebuf : {} > DDR_META, align = 0x400   /* zeroed at start, so the trace reads cleanly */

    /* MANDATORY: the K3 remoteproc driver boots a C66x only from a 1 KiB-aligned
     * entry point. */
    .text:_c_int00 : {} > DDR_CODE, align = 0x400
    .text:csl_entry : {} > DDR_CODE

    #include "c66x_runtime.cmd"

    .text   : {} > DDR_CODE
    .const  : {} > DDR_DATA
    .switch : {} > DDR_DATA
    .cinit  : {} > DDR_DATA
    .binit  : {} > DDR_DATA   /* load/run copy table, processed before main() */
    .pinit  : {} > DDR_DATA
    .init_array : {} > DDR_DATA
    .c6xabi.exidx : {} > DDR_DATA
    .c6xabi.extab : {} > DDR_DATA
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
    .sysmem  : {} > DDR_DATA
    .stack   : {} > L2SRAM, align = 0x8

    /* IPC buffers: RPMsg objects and virtqueue state. Default in DDR; NOLOAD
     * because the IPC library initializes them. */
    ipc_data_buffer : type = NOLOAD {} > DDR_DATA, align = 0x8

    /* The course runtime's state and the main task stack, in fast L2. */
    .sdos.runtime    : {} > L2SRAM
    .sdos.task_stack : type = NOLOAD {} > L2SRAM, align = 0x8

    #include "fir_memory.cmd"
    #include "buffers_memory.cmd"
}
