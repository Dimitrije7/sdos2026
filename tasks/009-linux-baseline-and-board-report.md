# 009 — Qualify the Linux baseline and implement board-report

## Status

in-progress

## Handoff

- Updated: 2026-10-18, claude.
- State: `board-report` is implemented and tested against a fixture; the
  board session that runs it is not scheduled yet. The maintainer powers the
  board only for batched sessions.
- Done: `tests/target/sdos_board/report.py` and the `board-report` command,
  with tests in `tests/target/test_sdos_board.py` and the fixture
  `tests/target/fixtures/board-report.raw.txt`.
- Next, in one board session:
  1. Run `board-report` twice and compare; archive both and `live.dtb`.
  2. Reboot over SSH, time the reconnection, and confirm the previous boot's
     journal ended with a clean shutdown.
  3. For Task 011: list remoteproc sysfs links, traces, and any service that
     claims the DSPs. For Task 012: decompile `live.dtb` with `dtc` in the
     container. For Task 007: capture the USB console read-only.
  4. Ethernet needs a cable to a network; without one the item stays failed
     with that reason. Time cannot synchronize over the USB link alone.
  5. When 011 and 012 pass, run Task 013's `dsp-ipc` in the same session
     (see its handoff), then Task 021's `course-smoke`, which runs the
     course firmware on every core and runtime through `dspctl`.
- Decisions: the maintainer prefers batched board sessions (2026-10-18).
- Verification, 2026-10-18: `python3 -m unittest tests.target.test_sdos_board`
  passed 38 tests.

## Depends on

008.

## Plan references

Phase 1 and the Linux row of Section 7.

## Objective

Verify the connected board's basic Linux health and turn the observations into
a repeatable read-only diagnostic command.

## Scope

- Test Ethernet, SSH, storage, time, clean shutdown/reboot, temperature, fan,
  and persistent logs.
- Capture board/silicon identity, U-Boot, kernel, DTB, packages, and firmware.
- Confirm required remoteproc, RPMsg, CMA/reserved-memory, debugfs, and trace
  kernel features.
- Implement and test `board-report` with machine-readable and human summaries.
- Redact secrets and volatile identifiers unsuitable for support bundles.

## Out of scope

DSP IPC proof, performance tuning, or kernel/device-tree modification.

## Maintainer checkpoint

`hardware-ready`: leave the qualified board connected; the agent owns all
commands and evidence capture.

## Acceptance criteria

- Every Phase 1 health item has a pass/fail result and diagnostic evidence.
- `board-report` is read-only, deterministic enough for comparison, and useful
  when a later lab fails.
- Missing required kernel features become explicit blockers.

## Verification and evidence

Run `board-report` twice, validate its schema/redaction, and archive the
baseline report plus relevant system logs.
