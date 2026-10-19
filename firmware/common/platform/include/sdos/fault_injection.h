/*
 * Fault-injection algorithms (docs/safety.md). Present only in firmware built
 * with -DSDOS_FAULT_INJECTION=ON; HELLO then reports SDOS_CAP_FAULT_INJECTION.
 * Expected outcome of each, per family, is in docs/safety.md and checked by
 * tests/target/sdos_board/fault_injection.py.
 */
#ifndef SDOS_FAULT_INJECTION_H
#define SDOS_FAULT_INJECTION_H

#include "sdos/runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDOS_ALG_FI_WRITE_FOREIGN 0xFA000001u /* write into the other C66x's carve-out */
#define SDOS_ALG_FI_READ_FOREIGN 0xFA000002u  /* read Linux's memory */
#define SDOS_ALG_FI_WRITE_CODE 0xFA000003u    /* write into the code zone */
#define SDOS_ALG_FI_EXEC_DATA 0xFA000004u     /* execute from the data zone */
#define SDOS_ALG_FI_STACK 0xFA000005u         /* recurse into the stack guard */
#define SDOS_ALG_FI_HANG 0xFA000006u          /* never return */
#define SDOS_ALG_FI_OVERRUN 0xFA000007u       /* busy for params (uint32) microseconds */
#define SDOS_ALG_FI_CORRUPT_IMAGE 0xFA000008u /* change read-only data the CRC record covers */
#define SDOS_ALG_FI_HEAP 0xFA000009u          /* allocate from the kernel heap after start-up */
#define SDOS_ALG_FI_WRITE_L2_CODE 0xFA00000Au /* write into the L2 code zone */
#define SDOS_ALG_FI_READ_SHARED 0xFA00000Bu   /* read shared on-chip SRAM (MSMC), outside every device window */

/* Replace the runtime's algorithm table with the built-ins plus these. */
void sdos_fault_injection_install(sdos_runtime *runtime);

#ifdef __cplusplus
}
#endif

#endif /* SDOS_FAULT_INJECTION_H */
