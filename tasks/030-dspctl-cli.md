# 030 — Implement the dspctl command-line interface

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: `dspctl` implements list, status, load, start, stop, recover, trace,
  echo, and run over `libsdos_dsp`; it is tested against the mock and has not
  run on the board.
- Done: `host/dspctl/dspctl.c`, CLI contract test `host/dspctl/tests/cli.cmake`
  (CTest `dspctl_mock_cli`), user guide `docs/dspctl.md`. The AArch64 binary
  builds in `aarch64-gcc-*`.
- Next: machine-readable output (`--json`), support bundle, board command
  matrix, and packaging for the image.
- Decisions: on 2026-10-18 the maintainer wrote "proceed with plan, but do everything except board tests, also postpone any decision making from my side because I will not be able to respond". This board-free work therefore started before its board-side dependencies were done; acceptance still needs them.
- Verification, 2026-10-19: `dspctl_mock_cli` passed.

## Depends on

028.

## Plan references

Phase 4 and the student workflow in Section 6.10.

## Objective

Give students and agents one safe interface for firmware lifecycle, diagnostics,
execution, and recovery.

## Scope

- Implement `list`, `load`, `start`, `stop`, `status`, `trace`, `run`, and
  `recover` over `libsdos_dsp`.
- Resolve logical DSP names and validate firmware compatibility before mutation.
- Provide human-readable and stable machine-readable output.
- Add clear timeout/error exits, bounded trace capture, and support-bundle hooks.
- Package and test native mock and AArch64 real-board builds.

## Out of scope

Direct student sysfs writes, an interactive GUI, or automatic firmware download
from untrusted locations.

## Maintainer checkpoint

`hardware-ready` for final real-device command validation.

## Acceptance criteria

- Every command is covered against mocks and validated on each real logical DSP.
- Unsafe lifecycle transitions and incompatible firmware are refused clearly.
- Output is sufficient for scripts, CI models, and support diagnostics.

## Verification and evidence

Run CLI contract tests, malformed/timeout cases, AArch64 package tests, and a
connected-board command matrix with archived output.
