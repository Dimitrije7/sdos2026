# 020 — Build the TI reference firmware matrix

## Status

pending

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
