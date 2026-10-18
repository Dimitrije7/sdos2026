# 021 — Create the course firmware templates

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: all 12 course firmware builds (3 cores × FreeRTOS/bare metal ×
  debug/release) link with warnings as errors and pass the carve-out check.
  Nothing has run on a DSP yet.
- Done:
  - Common platform in `firmware/common/platform/`: IPC service talking only
    to the A72, TI-layout resource table, 64 KiB trace ring, identity.
  - Adapters `firmware/c66x/` (MAR, cache, TSC) and `firmware/c71x/` (MMU,
    cache, TSC); the manifest's IPC pools are checked against TI's map at
    compile time.
  - Runtimes: `firmware/runtime/freertos/` (one task, blocking receive) and
    `firmware/runtime/baremetal/` (polling loop, interrupts off, mailbox poll
    mirroring TI's private table).
  - `cmake/SdosTiFirmware.cmake` links against the installed PDK in TI's order.
    A post-build step runs `ci/firmware_elf.py --require-trace`.
  - Documented in `docs/firmware.md`.
- Next:
  1. Board session: `python3 -m tests.target.sdos_board course-smoke --key
     ~/.ssh/sdos-bench_ed25519 --out release/evidence/task-021/<UTC stamp>`
     (tested against a simulated board shell in
     `tests/target/test_course_smoke.py`).
  2. Exception/crash record exposed to Linux.
  3. Student algorithm hook (Task 032 boundary) beyond the built-ins.
- Decisions: on 2026-10-18 the maintainer wrote "proceed with plan, but do everything except board tests, also postpone any decision making from my side because I will not be able to respond". This board-free work therefore started before its board-side dependencies were done; acceptance still needs them. Engineering choices made without a maintainer decision,
  all reversible: bare metal polls on both DSP families (TI's bare-metal OSAL
  cannot register C7x interrupts); bare-metal C71x links TI's FreeRTOS port
  for startup only; course firmware uses RPMsg endpoint 40 named `sdos-dsp`.
- Verification, 2026-10-19: `python3 ci/facade.py build --preset firmware
  --werror` in the container built all 12; every ELF reported `fits`.

## Depends on

020.

## Plan references

Phase 3 and the firmware matrix in Sections 6.1 and 6.4.

## Objective

Provide minimal, maintainable FreeRTOS and bare-metal starting points for both
C66x cores and the C71x core.

## Scope

- Define a shared platform/runtime interface and thin core-specific adapters.
- Implement startup plus a FreeRTOS task or bare-metal event loop.
- Include the validated resource table, RPMsg endpoint, linker script, trace,
  exception/crash record, and deterministic debug/release configuration.
- Isolate generated/vendor code from course-owned code and analysis results.
- Build all 12 core/profile/configuration combinations.

## Out of scope

Bulk shared buffers, final student algorithms, or changing the Gate 2 memory
contract without a new decision.

## Maintainer checkpoint

None unless the public student/runtime boundary changes the accepted course
policy.

## Acceptance criteria

- The complete C66x_0/C66x_1/C71x_0 × FreeRTOS/bare-metal × debug/release matrix
  builds through one command model.
- Each firmware exposes identity, version, echo, trace, and crash information.
- Common behavior has one source; target differences are explicit adapters.

## Verification and evidence

Run clean matrix builds and portable tests; archive artifacts, maps, manifests,
and build logs.
