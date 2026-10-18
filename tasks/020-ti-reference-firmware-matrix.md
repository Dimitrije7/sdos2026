# 020 — Build the TI reference firmware matrix

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: TI's Linux echo firmware rebuilds from the pinned PDK for all three
  DSPs, through the facade; the per-core board smoke test is left.
- Done:
  - `ci/ti_reference.py` and presets `ti-reference-<core>-<debug|release>`
    (group `ti-reference`) run TI's PDK make for `ipc_echo_test_freertos`
    (examples/linux, `-DA72_LINUX_OS`). TI's boot-image step needs `mono` and
    is off; its objcopy check uses the container's AArch64 binutils.
  - Each build writes `build/<preset>/build-manifest.json`: input hashes, the
    exact command, ELF and map hashes, the fit check, and for release a
    comparison with TI's pinned prebuilt blob.
  - Result: all six fit their carve-outs. Release resource tables equal TI's
    prebuilt ones on all three cores; the C71x segment layout is identical,
    and four C66x data segments moved, consistent with PDK 11.02.01.08 against
    TI's 11.02.01.05 build.
  - `make quality` builds the three release references (debug ones rebuild
    the whole PDK from source).
- Next:
  1. Record the reproducibility check (two clean builds) in this file.
  2. In a board session after Task 013: load each rebuilt release ELF with
     `dsp-ipc` (extend it to take a local file) and compare with TI's blob.
- Decisions: on 2026-10-18 the maintainer wrote "proceed with plan, but do everything except board tests, also postpone any decision making from my side because I will not be able to respond". This board-free work therefore started before its board-side dependencies were done; acceptance still needs them.
- Verification, 2026-10-19: `make build PRESET=ti-reference` built all six;
  `python3 -m unittest ci.test_facade` passed.

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
