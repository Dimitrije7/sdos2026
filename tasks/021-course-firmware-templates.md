# 021 — Create the course firmware templates

## Status

done

## Handoff

- Updated: 2026-10-19, claude.
- State: done.
- Done:
  - All 12 builds, plus the two example layouts and six fault-injection
    builds, link with warnings as errors.
  - Board, 2026-10-19: `course-smoke` passes on all three DSPs in both
    runtimes, twice in a row
    ([evidence](../release/evidence/task-021/20261019T153122Z/); the failures
    of the first run are kept in
    [`20261019T095402Z`](../release/evidence/task-021/20261019T095402Z/)).
    Each firmware reports identity and version (HELLO), echo, the trace, and a
    fault record (STATUS and the health line).
  - Defects the board exposed, all fixed and documented in
    [`docs/firmware.md`](../docs/firmware.md#start-and-stop-on-this-board):
    - a C71x ELF loading into C71x L2 hung the SoC;
    - the MAIN NAVSS secure proxy was missing from the device windows;
    - FreeRTOS on the C66x hung on every start after the first, because of
      the stale tick route and caches;
    - interrupt-mode Sciclient and the RTOS IPC library's refused interrupt
      routes, replaced by polling in both runtimes;
    - a C71x that never idles cannot be stopped: idle halts, the bare-metal
      wake tick, and a graceful shutdown acknowledgement;
    - the C71x monitor task stack was too small;
    - the C71x 64-bit trace entry was ignored by the kernel;
    - a HELLO sent before the endpoint existed was lost (`dspctl start` now
      retries).
- Next: none here. A student algorithm hook belongs to Task 032.
- Decisions: none.
- Verification, 2026-10-19: two board runs above; `make quality`.

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
