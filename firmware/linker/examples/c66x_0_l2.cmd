/*
 * Example layout: C66x_0 bare metal running from local L2 SRAM.
 * Build: make build PRESET=c66x_0-baremetal-release-l2
 *
 * All code, constants, data, stack, and heap run from the 288 KiB L2, which
 * the bare-metal runtime leaves entirely as SRAM. DDR keeps only what must or
 * should stay there: the resource table and trace (any loadable memory would
 * do), and the 266 KiB of IPC buffers, which do not fit beside the code.
 * The FIR module still uses fir_memory.cmd; here its run address is already
 * L2, so the copy stays as an example of load/run placement.
 *
 * Protection (docs/safety.md): the first 128 KiB of L2 hold the code and
 * become read-only and executable through the L2 page protection; the rest of
 * L2 is writable and never executable. The MPAX protects the FIR load image
 * in DDR and denies DDR outside the carve-out. Constants live in writable L2
 * here; the integrity check covers them.
 */
#include "sdos_memory.cmd"

-stack 0x2000
-heap  0x0

MEMORY
{
    DDR_META (RW)   : origin = SDOS_DDR_ORIGIN,            length = 0x00100000
    DDR_CODE (RX)   : origin = SDOS_DDR_ORIGIN + 0x100000, length = 0x00100000  /* FIR load image only */
    DDR_DATA (RW)   : origin = SDOS_DDR_ORIGIN + 0x200000, length = SDOS_DDR_USABLE - 0x200000
    L2CODE   (RX)   : origin = SDOS_L2SRAM_ORIGIN,           length = 0x20000   /* 8 protection pages */
    L2SRAM   (RW)   : origin = SDOS_L2SRAM_ORIGIN + 0x20000, length = SDOS_L2SRAM_USABLE - 0x20000
}

/* PROTECTION: the DDR code zone is the FIR load image; no read-only data
 * zone; the L2 code zone holds everything that runs. */
sdos_zone_code_start   = SDOS_DDR_ORIGIN + 0x100000;
sdos_zone_code_end     = SDOS_DDR_ORIGIN + 0x200000;
sdos_zone_rodata_start = SDOS_DDR_ORIGIN + 0x200000;
sdos_zone_rodata_end   = SDOS_DDR_ORIGIN + 0x200000;
sdos_zone_l2code_start = SDOS_L2SRAM_ORIGIN;
sdos_zone_l2code_end   = SDOS_L2SRAM_ORIGIN + 0x20000;
sdos_zone_l2_end       = SDOS_L2SRAM_ORIGIN + SDOS_L2SRAM_USABLE;

SECTIONS
{
    .resource_table : {} > DDR_META, align = 0x1000   /* MANDATORY: see SDOS.cmd */
    .tracebuf       : {} > DDR_META, align = 0x400    /* MANDATORY: see SDOS.cmd */
    .sdos.integrity : {} > DDR_META, align = 0x8      /* PROTECTION: see SDOS.cmd */
    .text:_c_int00  : {} > L2CODE, align = 0x400      /* MANDATORY: 1 KiB-aligned entry */
    .csl_vect       : {} > L2CODE, align = 0x400      /* MANDATORY: ISTP alignment */
    .text           : {} > L2CODE
    .text:csl_entry : {} > L2CODE
    GROUP : > L2SRAM { .neardata .rodata .bss }       /* MANDATORY: one DP window */
    .const  : {} > L2SRAM
    .switch : {} > L2SRAM
    .cinit  : {} > L2SRAM
    .binit  : {} > L2SRAM
    .init_array : {} > L2SRAM
    .c6xabi.exidx : {} > L2SRAM
    .c6xabi.extab : {} > L2SRAM
    .far     : {} > L2SRAM
    .fardata : {} > L2SRAM
    .data    : {} > L2SRAM
    .cio     : {} > L2SRAM
    .args    : {} > L2SRAM
    .stack   : {} > L2SRAM, align = 0x8
    .sdos.runtime    : {} > L2SRAM
    ipc_data_buffer  : type = NOLOAD {} > DDR_DATA, align = 0x8
    #include "fir_memory.cmd"
    #include "buffers_memory.cmd"
}
