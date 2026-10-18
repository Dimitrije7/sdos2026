/*
 * c71x_runtime.cmd: sections from TI's C7x FreeRTOS port, included inside
 * SECTIONS by the C71x SDOS.cmd. Both runtimes link the port (bare metal for
 * its startup and interrupt handling only), so both need them.
 */
/* MANDATORY: the MMU tables are written before C initialization runs, so they
 * must not be initialized by it. */
GROUP : > DDR_DATA
{
    .data.Mmu_tableArray        : type = NOINIT
    .data.Mmu_tableArraySlot    : type = NOINIT
    .data.Mmu_level1Table       : type = NOINIT
    .data.Mmu_tableArray_NS     : type = NOINIT
    .data.Mmu_tableArraySlot_NS : type = NOINIT
    .data.Mmu_level1Table_NS    : type = NOINIT
}
.bss:taskStackSection : type = NOLOAD {} > DDR_DATA, align = 0x2000
