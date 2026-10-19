# 042 — Freedom from interference and automotive practices for DSP firmware

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: every acceptance criterion is board-proven on all three DSPs in
  both runtimes. The C66x protection registers are locked (SM-17). The SoC
  firewalls are surveyed: all open; a run-time configuration of the C71x L2
  firewall is written but waits for the maintainer's approval to run on the
  board. What remains are maintainer decisions.
- Done: everything in the earlier handoffs (git history), and on the board
  on 2026-10-19
  ([evidence](../release/evidence/task-042/20261019T155359Z/)):
  - `course-smoke` passes on every core and runtime, twice in a row, with
    the MPLK lock in place.
  - `fault-injection` passes every case on C66x_0 and C66x_1 in both runtimes
    and on the C71x in both runtimes. The new
    `rewrite_protection` case is refused and recorded (`access`) on every
    core: by the MPLK lock on the C66x, by the read-only page tables on the
    C71x. Outcomes are tabulated in
    [`docs/safety.md`](../docs/safety.md#fault-injection).
  - SM-17: the C66x locks MPAX and the L2 page attributes with MPLK after
    configuration, proves the lock with two harmless writes, and the monitor
    compares every protection register with its snapshot each period.
  - SoC firewall survey (diagnostic `0xFA00000D` in the FI builds,
    [evidence](../release/evidence/task-042/20261019T155359Z/firewall-survey.txt)):
    every firewall on a path into the DSPs' DDR or L2 passes everything, and
    TI's system firmware leaves them unowned, so DSP firmware may claim and
    program them through TI-SCI with no boot-chain change. Details, ids, and
    priv-ids in [`docs/safety.md`](../docs/safety.md#residual-risks).
  - Criterion 2 (C71x stack overflow): TI's exception entry pushed onto the
    stack that faulted and ran the handler there, so an overflow into the
    guard page faulted again and hung the core. The firmware now installs
    its own vector table at start
    ([`vectors.asm`](../firmware/c71x/src/vectors.asm)), whose exception
    entries switch to a 16 KiB exception stack before storing anything and
    then enter TI's dispatcher unchanged. On the board the overflow is
    recorded as `stack` at the guard page and `dspctl recover` restarts the
    core, in both runtimes; `course-smoke` passes twice on the C71x and the
    example DDR layout boots
    ([evidence](../release/evidence/task-042/20261019T164820Z/)).
- Next:
  1. Board-test the C71x L2 firewall (approved, decision below): the C71x
     firmware claims firewall 264 at start and admits only its own priv-id
     (21) to its L2. The code is on the local, unpushed branch
     `task-042-c71x-l2-firewall` (f9e277b) in this clone. Rebase it on main, build, and prove it on the board with
     the start-up readback, `course-smoke` and `fault-injection` on the C71x,
     and the C66x firewall survey (`0xFA00000D`) being refused for 264. If
     it misbehaves or grows beyond small work, drop it and record why.
  2. Maintainer decision still open: RTI watchdogs through the ESM.
- Decisions: on 2026-10-19 the maintainer asked for the MPLK lock and the
  SoC firewalls after all pending board work, then for the C71x exception
  stack as the remaining fix that matters for students. Later the same day:
  the DDR carve-out firewalls (4762, 4763) are not pursued; the C71x L2
  firewall may be tried on the board because it is small; MISRA is dropped
  (Task 024).
- Verification, 2026-10-19: `course-smoke` (twice) and `fault-injection` on
  the board, and the firewall survey on C66x_0 and C71x_0; after the
  exception stack, `fault-injection` and `course-smoke` (twice) again on the
  C71x; evidence above; `make quality`.

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

Out of scope without a maintainer decision: programming the SoC firewalls
(unowned after boot, but SoC-wide), the RTI hardware watchdogs (their reaction is
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
