# 042 — Freedom from interference and automotive practices for DSP firmware

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: board-proven on all three DSPs in both runtimes, with one
  criterion open (C71x stack overflow, criterion 2) and one platform
  limitation recorded (C66x reads). Locking the C66x protection registers and
  the SoC firewalls come next, as the maintainer asked on 2026-10-19.
- Done: everything in the earlier handoff (git history), and on the board
  on 2026-10-19
  ([evidence](../release/evidence/task-042/20261019T153122Z/)):
  - `fault-injection` passes every case on C66x_0 and C66x_1 in both runtimes
    and on the C71x in both runtimes, except the C71x stack case. Every core
    recovers with `dspctl recover`. The outcomes are tabulated in
    [`docs/safety.md`](../docs/safety.md#fault-injection).
  - C66x MPAX: the XMC refuses and records writes outside the grants (status
    0x110) but lets reads through. The self-test now probes with a write (to
    unowned MSMC, writing back the value it read). Reads of foreign memory
    are a recorded confidentiality gap, not interference.
  - C66x L2 page protection is enforced: a write into the L2 code zone is
    refused and recorded.
  - Writes must leave the cache before the check sees them: the C66x checks
    the CPU's own access, not a cache writeback (`write_through` in
    `fault_injection.c`).
  - Device windows confirmed. The first run granted the MCU NAVSS secure proxy
    by mistake (DSPs use the MAIN one) and `Sciclient_init` failed; fixed in
    the manifest.
  - C71x: exceptions record the fault, leave exception context through TI's
    return hook (RETE, as TI's MMA example does), and idle, so Linux can
    power the core off. The watchdog now halts a runaway RUN: the FreeRTOS
    monitor task does it, and on bare-metal C71x the 1 ms wake tick does.
  - SM-16 clean restart; see `docs/safety.md` and `docs/firmware.md`.
- Next:
  1. Lock the C66x MPAX and L2 attribute registers (MPLK) after
     configuration; prove it with a fault-injection case that tries to
     rewrite them.
  2. SoC firewalls against other bus masters (Linux, DMA, the other DSPs)
     writing a DSP's DDR and the C71x L2: investigate what the boot
     firmware and TI-SCI allow at run time, without changing the boot chain
     unless the maintainer approves.
  3. C71x stack overflow (criterion 2): the handler runs on the faulting
     stack; it needs a separate exception stack, which means changing TI's
     exception entry.
  4. Maintainer decisions still open: RTI watchdogs through the ESM; the
     MISRA subset (Task 024).
- Decisions: on 2026-10-19 the maintainer asked for the MPLK lock and the
  SoC firewalls after all pending board work.
- Verification, 2026-10-19: `course-smoke` (twice) and `fault-injection` on
  the board, evidence above; `make quality`.

## Depends on

021, 022, 041 for the firmware, validator, and linker files it hardens (board-
free work proceeds now). The board proof needs 013 and 041's board session.

## Maintainer requirement

Received 2026-10-19, verbatim:

> I want to get as close to automotive grade and freedom from interference as
> possible even if this is not one such platform, practices should be
> implemented. this must be drastically improved. go.

This followed the assessment that the layout had no run-time protection: the
C71x MMU mapped all DDR as cached and executable, the C66x configured no memory
protection, stacks had no guards, there was no time monitoring, and message
headers had no checksum.

## Scope

Apply the practices ISO 26262-6 Annex D lists for freedom from interference
between software elements, as far as this hardware and the unchanged Linux
configuration allow. The BeagleBone AI-64, Linux on the A72, TI's FreeRTOS port,
and the PDK are not safety-qualified, so the project claims no ASIL. Each
mechanism is either implemented and verified, or recorded with the reason it
cannot be.

Out of scope without a maintainer decision: SoC firewalls (they live in TI's
boot firmware configuration), the RTI hardware watchdogs (their reaction is
routed through the ESM, which can reset the SoC), and rebuilding TI's FreeRTOS
kernel library.

## Acceptance criteria

Spatial interference:

1. C71x: the MMU maps only the core's own carve-out, vring pool, local L2, and
   device windows. Code is read-only and executable; read-only data is read-only
   and not executable; data, trace, and the vring pool are not executable;
   device windows are not executable. Every other address faults.
2. C71x: a page below each guarded stack is unmapped, so an overflow faults.
   The exception hook records the fault and stops the core in a safe state.
3. C66x: MPAX denies DDR outside the core's own carve-out and vring pool, makes
   code read-only and read-only data non-writable, and removes execute
   permission elsewhere. Violations are read from the fault registers and
   reported. A start-up self-test proves that a denied access is caught.
4. Stacks are painted at start; guard bands are checked periodically; peak use
   is reported.
5. No C heap. The FreeRTOS kernel heap may change only during start-up; a later
   change is a fault.
6. Every build's loadable image passes a write-xor-execute rule, and its code
   and read-only data carry a CRC record written after linking. Firmware checks
   it at start and in the background, and checks the load/run copies.

Temporal interference:

7. Each RUN carries a time budget. Overruns are reported. Under FreeRTOS a
   monitor task detects a request that runs past the watchdog limit.
8. A health line with a heartbeat, state, fault record, and stack use is kept
   at the start of the trace buffer, readable by Linux even when IPC is stuck.
9. `dspctl supervise` detects a stale heartbeat or a fault and restarts the
   core, capturing the trace.

Communication interference:

10. Protocol 2.0 protects every message with a CRC-32C, rejects messages from
    any processor other than the A72 Linux host, and keeps the existing length,
    descriptor, sequence, and data-checksum checks.

Fault handling and verification:

11. A fault puts the firmware into a safe state: RUN is refused with a distinct
    status, HELLO and STATUS still answer, and the fault record says what
    happened. Fatal faults stop the core with the record written.
12. Fault-injection builds exercise each mechanism. The connected-board suite
    runs them and checks detection and recovery. Board evidence goes under
    `release/evidence/task-042/`.
13. `docs/safety.md` maps every interference class to its mechanism, its
    verification, and its status.

## Findings

From the pinned PDK 11.02.01.08 and compilers, 2026-10-19:

- The C7x MMU (`csl/arch/c7x/Mmu.h`) supports per-mapping access permission,
  execute permission, and memory type with a 4 KiB granule. TI's table pool has
  16 tables per security state. TI calls `InitMmu()` before C initialization.
- The C7x exception module (`csl/arch/c7x/Exception.c`) calls a registered
  `exceptionHook` for internal exceptions and page faults.
- The C66x XMC has 16 MPAX segments with supervisor and user read, write, and
  execute bits, and records violations in XMPFAR and XMPFSR
  (`csl/arch/c66x/dsp_xmc.h`, base 0x08000000).
- TI's FreeRTOS kernel is built with stack-overflow checking method 2 and
  `heap_4` (32 KiB on the C66x, 192 KiB on the C71x); the port defines the
  overflow hook. No course build links `malloc`.
- Both TI linkers accept `crc_table()` syntactically but refuse to generate a
  table for these targets ("linker will only generate a CRC table when creating
  an executable file", also for a minimal absolute link). The integrity record
  is therefore written after linking.
- Linked segments already separate writable from executable memory, so a
  write-xor-execute rule holds and can be enforced.

## Plan references

Section 3 (one memory-map contract), Sections 6.1 and 6.6 (protocol and
recovery), and the DSP and host invariants in `AGENTS.md`.
