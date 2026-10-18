/*
 * c66x_runtime.cmd: sections each C66x runtime adds, included inside SECTIONS
 * by every C66x SDOS.cmd. The linker defines SDOS_RTOS_FREERTOS or
 * SDOS_RTOS_BAREMETAL (cmake/SdosTiFirmware.cmake).
 */
#if defined(SDOS_RTOS_FREERTOS)
    /* MANDATORY: TI's FreeRTOS C66x port installs this interrupt vector table
     * through ISTP, which requires 1 KiB alignment. */
    .hwi_vect : {} > DDR_CODE, align = 0x400
#else
    /* MANDATORY: the CSL interrupt controller's vector table, installed through
     * ISTP by the bare-metal OSAL, which requires 1 KiB alignment. */
    .csl_vect : {} > DDR_CODE, align = 0x400
#endif
