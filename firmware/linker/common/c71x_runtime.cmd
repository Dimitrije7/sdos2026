/*
 * c71x_runtime.cmd: sections from TI's C7x FreeRTOS port, included inside
 * SECTIONS by the C71x SDOS.cmd. Both runtimes link the port (bare metal for
 * its startup and interrupt handling only), so both need them.
 */
/* MANDATORY: the MMU tables are written before C initialization runs, so they
 * must not be initialized by it. PROTECTION: after start-up the firmware maps
 * them read-only, so they occupy whole pages of their own (the group starts on
 * a page and its last member is padded to one) and the symbols tell it where. */
GROUP : > DDR_DATA, align = 0x1000, RUN_START(sdos_mmu_tables_start), RUN_END(sdos_mmu_tables_end)
{
    .data.Mmu_tableArray        : type = NOINIT
    .data.Mmu_tableArraySlot    : type = NOINIT
    .data.Mmu_level1Table       : type = NOINIT
    .data.Mmu_tableArray_NS     : type = NOINIT
    .data.Mmu_tableArraySlot_NS : type = NOINIT
    .data.Mmu_level1Table_NS    : type = NOINIT, palign = 0x1000   /* pads the group to a page */
}
.bss:taskStackSection : type = NOLOAD {} > DDR_DATA, align = 0x2000
/* PROTECTION: TI's port declares these two pointers writable (const void *)
 * but places them in .const sections. They go with the data, so the read-only
 * zone holds only data that is really read-only. */
.sdos.ti_writable_const : {
    *(.const:Hwi_vectorTableBase)
    *(.const:Hwi_vectorTableBase_SS)
} > DDR_DATA
