# 016 — Establish the repository command facade and build skeleton

## Status

done

## Completion evidence

- Command surface: `Makefile` and [`ci/facade.py`](../ci/facade.py), with the
  contract, exit codes, and current versus future coverage in
  [`docs/commands.md`](../docs/commands.md). `package`, `image`, `deploy`, and
  `target-smoke` exit 4 and name their owning task; `deploy` and
  `target-smoke` without `BOARD` exit 2.
- Matrix: `CMakePresets.json` has 18 presets: host GCC and Clang in debug and
  release, aarch64 board programs, and C66x_0, C66x_1, and C71x_0 in FreeRTOS
  and bare metal, debug and release. `cmake/SdosOptions.cmake` rejects invalid
  combinations. Firmware presets stop with a message naming the missing TI
  compiler until Task 019.
- First product code: `algorithms/fir_q15`, a rounding, saturating Q15 FIR
  reference with unit tests.
- Verification, 2026-10-18: from a fresh clone of commit `92b38d3`, `make
  quality` passed in the Task 017 container: three validators, 87 Python
  tests, and four host presets with warnings as errors.
  `ci.test_facade` covers invalid presets, unknown commands, and every
  unavailable command. `build --preset board-host` produced aarch64 objects.

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
