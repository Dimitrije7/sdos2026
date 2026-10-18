# 029 — Implement the DSP runtime and shared-buffer data path

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: the common DSP runtime handles every 1.0 message and validates
  descriptors, overlap, checksums, and busy state; there is no shared region on
  the board yet.
- Done: `firmware/common/runtime/` with cache hooks around RUN, cycle
  measurement, and per-instance scratch; built for all 12 firmware targets and
  tested on the PC (`test_runtime`, and the fuzz test in `test_protocol`).
- Next: the shared region and guard bands after the Task 027 decision; board
  integrity matrices.
- Decisions: on 2026-10-18 the maintainer wrote "proceed with plan, but do everything except board tests, also postpone any decision making from my side because I will not be able to respond". This board-free work therefore started before its board-side dependencies were done; acceptance still needs them.
- Verification, 2026-10-19: `test_runtime` passed in four host configurations.

## Depends on

021 and 027.

## Plan references

Phase 4 and the shared-memory/cache risks in Section 8.

## Objective

Implement the accepted protocol and explicit shared-buffer ownership/cache
contract for every DSP and both runtime profiles.

## Scope

- Implement capability negotiation, dispatch, status, trace, and timeout-safe
  control handling in common DSP code.
- Implement descriptor validation, guard regions, sequence/checksum checks, and
  ownership transitions.
- Add target-specific cache writeback/invalidate operations and memory barriers
  behind reviewed adapters.
- Preserve FreeRTOS and bare-metal behavior parity across C66x and C71x.
- Add portable model tests and connected-board integrity tests.

## Out of scope

Algorithm examples beyond a small deterministic transfer kernel or unexplained
cache/coherency assumptions.

## Maintainer checkpoint

`hardware-ready` for final cache and large-buffer proof.

## Acceptance criteria

- All six core/runtime targets exchange bounded buffers without corruption.
- Cache ownership is explicit in code, protocol, documentation, and tests.
- Invalid descriptors, stale sequences, timeouts, and guard corruption fail
  safely with diagnostics.

## Verification and evidence

Run portable state/model tests plus real-board size/alignment/guard/checksum
matrices; archive per-target results.
