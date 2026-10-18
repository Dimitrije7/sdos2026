/*
 * buffers_memory.cmd: coefficient, scratch, and uninitialized data, included
 * inside SECTIONS. Firmware puts an object in one of these sections with
 * __attribute__((section(".sdos.coeffs"))) and so on.
 *
 * The two scratch areas are an intentional overlay (UNION): they share run
 * addresses because the firmware never uses both at once. Overlays share run
 * addresses only, never load images, so the validator accepts them.
 *
 * NOINIT data is neither loaded nor zeroed. Its contents after a firmware load,
 * a core reset, or power loss are undefined; it does not persist.
 */
.sdos.coeffs  : {} > L2SRAM, align = 0x80, RUN_START(sdos_coeffs_start), SIZE(sdos_coeffs_size)
UNION : > L2SRAM, align = 0x80
{
    .sdos.scratch_a : type = NOLOAD {}
    .sdos.scratch_b : type = NOLOAD {}
}
.sdos.noinit  : type = NOINIT {} > DDR_DATA, align = 0x80
