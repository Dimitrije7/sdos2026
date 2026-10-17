# 016 — Establish the repository command facade and build skeleton

## Status

pending

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
