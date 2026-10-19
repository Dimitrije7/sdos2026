# 020 — Build the TI reference firmware matrix

## Status

blocked

## Handoff

- Updated: 2026-10-19, claude.
- State: blocked on the same decision as Task 013. The rebuilt references
  build, fit, and start, but fail the per-core echo on this board.
- Done:
  - Builds: all six `ti-reference` presets build through the facade (see
    [build evidence](../release/evidence/task-020/20261019T010000Z/)).
  - Board, 2026-10-19
    ([evidence](../release/evidence/task-020/20261019T154212Z/)), one cycle
    per core: all three load, start, and announce their channels. The device
    manager's trace shows TI's RTOS ping-pong with C7X_1 and C66X_1 completing
    10000 round trips. The echo to Linux failed on all three. On the C66x the
    cause is the same as for TI's prebuilt firmware (Task 013). On the C71x the
    result differs from the prebuilt blob, which echoed once; not pursued.
- Next: Task 013's decision.
- Decisions: none yet.
- Verification, 2026-10-19: `dsp-ipc --cycles 1 --firmware <rebuilt>` per
  core.

## Depends on

015 and 019.

## Plan references

Phase 3 and the firmware matrix in Section 6.1.

## Objective

Rebuild the known TI IPC firmware from source for every DSP using the frozen
Gate 2 stack.

## Scope

- Build pinned IPC examples for `c66xdsp_1`, `c66xdsp_2`, and `c7x_1`.
- Separate J721E SoC-common inputs from EVM-only assumptions.
- Produce deterministic debug/release build manifests, ELF files, maps, and
  hashes through the command facade.
- Compare rebuilt artifacts' memory/resource contracts with Gate 2 binaries.
- Deploy and smoke-test rebuilt firmware on each logical DSP.

## Out of scope

Course API design, optimized algorithms, or changing the frozen memory map.

## Maintainer checkpoint

`hardware-ready` for final deployment proof; build work remains board-free.

## Acceptance criteria

- All three reference targets build from a clean container without SDK edits.
- Rebuilt firmware passes the per-core connected-board IPC smoke test.
- Build manifests explain any non-bit-identical but functionally equivalent
  result.

## Verification and evidence

Archive compiler logs, ELF/map/hash manifests, reproducibility comparison, and
per-core smoke results.
