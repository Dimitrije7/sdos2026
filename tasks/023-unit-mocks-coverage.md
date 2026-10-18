# 023 — Establish unit tests, mocks, sanitizers, and coverage

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: CTest runs C unit, component, fuzz, and CLI tests on the PC in five
  configurations, one of them under sanitizers; coverage and JUnit reports
  are left.
- Done:
  - Preset `host-gcc-sanitize` (AddressSanitizer, UndefinedBehaviorSanitizer,
    leak detection, no recovery) in the `host` group, so `make quality` and CI
    run it.
  - Tests: `test_protocol` (golden, rejection, ownership table, CRC, 200,000
    fuzz cases), `test_runtime`, `test_dsp_mock` (fault injection),
    `test_dsp_linux` (simulated kernel), `dspctl_mock_cli`; shared helpers in
    `tests/unit/include/`.
  - GCC instead of Clang for sanitizers: Clang 15's AddressSanitizer crashed
    13 of 40 runs of an empty program in the container on this host kernel,
    and GCC 12's crashed in 0 of 40 (`cmake/SdosOptions.cmake` records why).
  - Preset `host-gcc-coverage` and `ci/coverage.py` (gcov JSON, no new
    tools): `make test` and `make quality` print line and branch coverage
    per file and write `coverage.json`. First result, 2026-10-19: 83.2% of
    lines and 67.4% of branches overall; protocol 94.3%/83.3%, runtime
    95.8%/85.7%, Linux backend 64.1%/49.4% (its ioctl paths need a kernel).
- Next:
  1. Maintainer decision: a coverage threshold. Recommendation: fail below
     90% lines for `firmware/common`, and below 80% for `host/libsdos_dsp`,
     measured by `ci/coverage.py`; report-only elsewhere.
  2. JUnit output for CI; deliberate-defect fixtures that prove each gate
     fails.
- Decisions: on 2026-10-18 the maintainer wrote "proceed with plan, but do everything except board tests, also postpone any decision making from my side because I will not be able to respond". This board-free work therefore started before its board-side dependencies were done; acceptance still needs them.
- Verification, 2026-10-19: `make test PRESET=host-gcc-sanitize` passed all
  six tests, ten times in a row.

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
