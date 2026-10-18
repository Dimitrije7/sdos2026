/*
 * fir_memory.cmd: placement of the Q15 FIR module (algorithms/fir_q15),
 * included inside SECTIONS. An example of separate load and run addresses:
 * remoteproc loads the code into DDR, and the runtime library copies it into
 * L2 SRAM before main(), through the BINIT copy table that table(BINIT) asks
 * the linker to build. The symbols let firmware report where it ran from.
 */
.sdos.fir_text : {
    *libsdos_fir_q15.a<*>(.text)
} load = DDR_CODE, run = L2SRAM, align = 0x20, table(BINIT),
  LOAD_START(sdos_fir_load_start), RUN_START(sdos_fir_run_start), RUN_SIZE(sdos_fir_run_size)
