# 016 — Establish the repository command facade and build skeleton

## Status

in-progress

## Handoff

- Updated: 2026-10-18, claude.
- State: Implemented and verified in the Task 017 container; the clean-clone
  check remains.
- Done: `Makefile`, `ci/facade.py` with `ci/test_facade.py`,
  `CMakePresets.json` with 18 presets, `cmake/` options, warnings, and
  toolchain files, the first algorithm `algorithms/fir_q15`, and
  [`docs/commands.md`](../docs/commands.md).
- Next: run `make quality` from a fresh clone, then complete this task.
- Decisions: none needed.
- Verification, 2026-10-18: `python3 ci/facade.py quality` passed in image
  `sdos-dev:dev-b124c2c36c91`: three validators, 87 Python tests, and four host
  presets with warnings as errors. `build --preset board-host` produced aarch64
  objects; a firmware preset stopped with the missing-compiler message.

## Depends on

006.

## Plan references

Phase 3 and Sections 6.1, 6.5, and 6.13.

## Objective

Create the stable local command surface and build-system structure that CI and
students will share.

## Scope

- Add the intended repository directories without placeholder binaries.
- Implement `make bootstrap|configure|build|test|quality|package|image` and
  guarded board targets incrementally with clear unsupported diagnostics.
- Add CMake presets/toolchain seams for host, AArch64, C66x, and C71x variants.
- Encode debug/release and FreeRTOS/bare-metal dimensions without duplicated
  build logic.
- Document inputs, outputs, clean behavior, and exit-code contracts.

## Out of scope

Pretending unavailable TI targets work or embedding build logic in Actions YAML.

## Maintainer checkpoint

None unless the public command contract needs an architectural change.

## Acceptance criteria

- Implemented commands work from the repository root and fail unsupported
  targets explicitly.
- Configure/build metadata makes the full required matrix representable.
- CI can call the facade without private shell knowledge.

## Verification and evidence

Exercise implemented commands from a clean tree, include deliberate invalid
target tests, and document current versus future command coverage.
