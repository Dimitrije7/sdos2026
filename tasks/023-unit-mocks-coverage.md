# 023 — Establish unit tests, mocks, sanitizers, and coverage

## Status

pending

## Depends on

016 and 021.

## Plan references

Sections 6.6 and 6.7.

## Objective

Create the fast, board-free feedback layer for common firmware, algorithms,
protocol primitives, and future host tooling.

## Scope

- Configure CTest plus the selected C/C++ test frameworks.
- Add OS/platform/transport seams and mocks for FreeRTOS-independent code.
- Test bounds, integer overflow, malformed input, timeout, fixed-point,
  tolerance, saturation, and error paths.
- Enable GCC/Clang debug/release runs, ASan/UBSan where supported, JUnit, and
  line/branch coverage.
- Enforce accepted core-library coverage and changed-code reporting.

## Out of scope

Claiming mocked tests prove remoteproc, RPMsg, caches, timing, or real hardware.

## Maintainer checkpoint

None unless a coverage-policy change is proposed.

## Acceptance criteria

- Tests run through `make test` without a board and publish useful reports.
- Sanitizer and coverage jobs cover all portable core modules.
- Deliberate defects prove failure, sanitizer, and threshold gates work.

## Verification and evidence

Run the GCC/Clang/configuration matrix, inspect JUnit/coverage output, and record
negative-fixture results.
