# 042 — Freedom from interference and automotive practices for DSP firmware

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: every board-free part is implemented and passes `make quality`
  (lint, cppcheck, Python tests, six host configurations, all 20 firmware
  presets with warnings as errors). Left: the board proof, and the decisions
  listed under Next.
- Done:
  - Protocol 2.0 (`firmware/common/protocol/`): 20-byte header with CRC-32C
    over header and payload; `message corrupted`, `firmware faulted`,
    `deadline missed`; RUN time budget; 64-byte STATUS with the fault record
    and safety counters; the shared health-line format and parser. Tests: every
    single-bit error detected, golden bytes computed independently.
  - Runtime: fault record and safe state, start-up `busy`, budget measurement,
    watchdog check, corrupted-request counting (`test_runtime`).
  - `firmware/common/safety/`: stack fill and margin, integrity record and
    incremental check, span subtraction, MPAX split (`test_safety`).
  - Monitor (`firmware/common/platform/src/monitor.c`): heartbeat, watchdog,
    stack guards, integrity slices, protection polling, frozen kernel heap,
    health line; a FreeRTOS monitor task and bare-metal polling.
  - C71x (`c71x.c`): least-privilege MMU map from linker zones and manifest
    device windows; page tables and stack guard pages read-only after start;
    exception hook that records and halts.
  - C66x (`c66x.c`): MPAX partition with priority overlays, fault polling,
    start-up self-test that requires a blocked read of the other C66x.
  - Sender check and guarded IPC stack in `ipc_service.c`.
  - Linker files: `#define`d boundaries, `DDR_RODATA`, zone symbols, per-stack
    sections, `.sdos.integrity`, no heap, page-padded C71x MMU tables, TI's two
    writable `.const` pointers moved to data; both examples updated.
  - Post-link step `ci/firmware_postlink.py`: zones, code placement, W^X, no
    heap, MPAX budget, C71x guard alignment and MMU table budget, integrity
    record. `ci/firmware_elf.py --require-integrity` re-checks the stripped
    file. Tests: `ci.test_firmware_elf`, `ci.test_firmware_postlink`, and four
    new real-link refusals in `ci.test_linker_layouts`.
  - Host: `sdos_dsp_health`, `sdos_dsp_set_run_budget`; `dspctl health`,
    `supervise`, `--budget-us`, numeric algorithm ids, exit status 3; mock
    `faulted` and `hang` states; CLI and Linux-backend tests.
  - Fault injection: `-DSDOS_FAULT_INJECTION=ON`, six `*-fi` presets,
    nine cases; board suite `fault-injection`
    (`tests/target/sdos_board/fault_injection.py`) with offline tests;
    `course-smoke` now checks the health line.
  - Documents: `docs/safety.md` (mechanisms, interference analysis, partition,
    fault codes, rules, residual risks); protocol, dspctl, firmware, linker,
    commands, README; Task 027 records protocol 2.0.
- Next:
  1. Board session, after Task 013: `course-smoke` for all six firmware, then
     `fault-injection --out release/evidence/task-042/<timestamp>/`. Record the
     `mpax:` and `mmu:` trace lines. If a protection map stops a core from
     booting, the trace and health line name the address; adjust the map.
  2. With Task 012's survey: narrow C66x MPAX segment 0 and the C71x device
     windows to the peripherals the drivers use; configure C66x L2 page
     protection.
  3. Maintainer decisions to present, each with a recommendation: SoC
     firewalls in TI's boot configuration; per-DSP RTI watchdogs through the
     ESM; the MISRA subset (Task 024).
- Decisions: on 2026-10-19 the maintainer asked for this work (quoted under
  [Maintainer requirement](#maintainer-requirement)). Protocol 1.0 was never
  accepted (Task 027), so 2.0 replaces it; Task 027's recommendation now
  names 2.0.
- Verification, 2026-10-19: `make quality` passed: validators, lint,
  cppcheck, 158 Python tests (including 8 real-link layout cases), host tests
  in six configurations, all 20 firmware presets and the three TI reference
  builds with warnings as errors. Board checks not run.

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
