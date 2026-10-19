/*
 * Example layout: C71x_0 FreeRTOS with the DDR carve-out repartitioned.
 * Build: make build PRESET=c71x_0-freertos-release-ddr
 *
 * Differs from the default SDOS.cmd in every placement decision a project
 * makes: data first and code at the top of the carve-out, the FIR code run
 * directly from DDR (no load/run copy), coefficients at a fixed address, and
 * module placement written here instead of in the fragments. The MANDATORY
 * and PROTECTION rules from SDOS.cmd still hold, with their reasons there.
 */
#include "sdos_memory.cmd"

-e _c_int00_secure
--diag_suppress=10063
--cinit_compression=off
--diag_suppress=10068
-stack 0x20000
-heap  0x0
--args 0x1000

MEMORY
{
    /* The code region must hold three aligned slots: the 2 MiB-aligned entry
     * point and the 4 MiB- and 2 MiB-aligned vector tables. Below 8 MiB that
     * does not fit, which the linker reports as an alignment failure. */
    DDR_DATA   (RW) : origin = SDOS_DDR_ORIGIN,            length = 0x00500000
    DDR_RODATA (R)  : origin = SDOS_DDR_ORIGIN + 0x500000, length = 0x00100000
    DDR_META   (RW) : origin = SDOS_DDR_ORIGIN + 0x600000, length = 0x00100000
    DDR_CODE   (RX) : origin = SDOS_DDR_ORIGIN + 0x700000, length = SDOS_DDR_USABLE - 0x700000
}

sdos_zone_code_start   = SDOS_DDR_ORIGIN + 0x700000;
sdos_zone_code_end     = SDOS_DDR_ORIGIN + SDOS_DDR_USABLE;
sdos_zone_rodata_start = SDOS_DDR_ORIGIN + 0x500000;
sdos_zone_rodata_end   = SDOS_DDR_ORIGIN + 0x600000;

SECTIONS
{
    .resource_table : {} > DDR_META, align = 0x1000
    .tracebuf       : {} > DDR_META, align = 0x400
    .sdos.integrity : {} > DDR_META, align = 0x8
    .text:_c_int00_secure : {} > DDR_CODE, align = 0x200000
    .vecs           : {} > DDR_CODE, align = 0x400000
    .secure_vecs    : {} > DDR_CODE, align = 0x200000
    .text           : {} > DDR_CODE
    .sdos.fir_text  : { *libsdos_fir_q15.a<*>(.text) } > DDR_CODE,
                      RUN_START(sdos_fir_run_start), LOAD_START(sdos_fir_load_start), SIZE(sdos_fir_run_size)
    #include "c71x_runtime.cmd"
    .bss    : {} > DDR_DATA
    .data   : {} > DDR_DATA
    .cinit  : {} > DDR_RODATA
    .binit  : {} > DDR_RODATA
    .init_array : {} > DDR_RODATA
    .const  : {} > DDR_RODATA
    .switch : {} > DDR_RODATA
    .args   : {} > DDR_DATA
    .cio    : {} > DDR_DATA
    .stack  : {} > DDR_DATA, align = 0x20000
    ipc_data_buffer  : type = NOLOAD {} > DDR_DATA, align = 0x8
    .sdos.stack.main    : type = NOLOAD {} > DDR_DATA, align = 0x2000
    .sdos.stack.monitor : type = NOLOAD {} > DDR_DATA, align = 0x2000
    .sdos.stack.ipc     : type = NOLOAD {} > DDR_DATA, align = 0x2000
    .sdos.runtime    : {} > DDR_DATA
    .sdos.coeffs     : {} load = 0xa8780000  /* fixed address: free space in DDR_META after the table and trace */
    .sdos.scratch_a  : type = NOLOAD {} > DDR_DATA, align = 0x80
    .sdos.scratch_b  : type = NOLOAD {} > DDR_DATA, align = 0x80
    .sdos.noinit     : type = NOINIT {} > DDR_DATA
}
