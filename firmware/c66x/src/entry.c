/* Reset entry of every C66x firmware (the linker files set -e sdos_entry).
 *
 * Linux stops a C66x by switching off its core, but the C66x subsystem, with
 * its L2 memory and caches, stays powered. A restart can therefore find the
 * previous run's cache lines, dirty ones included, and its L2 cache mode: on
 * the board every FreeRTOS start after the first hung before its first trace
 * line (Task 021). TI's start-up configures the caches but never invalidates
 * them, so this entry discards every cache line, returns L2 to all-SRAM (each
 * runtime sets its own cache mode afterwards), and empties the XMC prefetch
 * buffer before TI's C runtime runs.
 *
 * It runs before the stack and the data pointer exist: register accesses and
 * constants only. Its code may itself still be cached from the previous run;
 * that copy is this same code, because the linker files keep the entry at a
 * fixed place of its own.
 */
#include <stdint.h>

/* A branch, not a call: a call would save the return address on the stack,
 * which does not exist yet (checked in the disassembly: nothing uses B15). */
void sdos_entry(void) __attribute__((noreturn, section(".text:sdos_entry")));
__asm("\t.ref\t_c_int00"); /* TI's C runtime entry, in the RTS boot module */

/* hw_dsp_icfg.h (SOC_DSP_ICFG_BASE 0x01000000) and hw_xmc.h. */
#define REG(address) (*(volatile uint32_t *)(address))
#define L2CFG 0x01840000u
#define L2INV 0x01845008u
#define L1PINV 0x01845028u
#define L1DINV 0x01845048u
#define L2MODE_MASK 0x7u
#define XPFCMD 0x08000300u

void sdos_entry(void)
{
    REG(L1DINV) = 1u; /* discard, never write back: nothing the last run cached is valid */
    while ((REG(L1DINV) & 1u) != 0u) {
    }
    REG(L2INV) = 1u;
    while ((REG(L2INV) & 1u) != 0u) {
    }
    REG(L1PINV) = 1u;
    while ((REG(L1PINV) & 1u) != 0u) {
    }
    REG(L2CFG) &= ~L2MODE_MASK;
    (void)REG(L2CFG); /* the CPU stalls on this read until the mode change is done */
    REG(XPFCMD) = 1u;
    __asm("\tB\t_c_int00\n\tNOP\t5");
    for (;;) {
    }
}
