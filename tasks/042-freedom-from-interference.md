# 042 — Freedom from interference and automotive practices for DSP firmware

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: started. Platform survey done; implementation in progress.
- Done:
  - Survey of the pinned PDK and compilers (see [Findings](#findings)).
- Next:
  1. Protocol 2.0: end-to-end message CRC, new status codes, RUN budget,
     extended STATUS.
  2. Runtime: fault record, safe state, deadline check, sender check.
  3. Common protection code, host-tested: health line, stack guards and
     watermarks, image integrity check, MPAX and MMU planners.
  4. Linker files: code, read-only data, and data zones; no C heap;
     post-link integrity record; validator rules.
  5. C71x least-privilege MMU map, guard pages, exception hook.
  6. C66x MPAX partition, fault polling, protection self-test.
  7. Monitor task (FreeRTOS) and loop checks (bare metal).
  8. Fault-injection builds and the connected-board suite (static only).
  9. Host: health API, `dspctl health` and `dspctl supervise`.
  10. `docs/safety.md` with the interference analysis; update the affected
      documents.
- Decisions: on 2026-10-19 the maintainer asked for this work (quoted under
  [Maintainer requirement](#maintainer-requirement)).
- Verification: none yet.

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
