# 009 — Qualify the Linux baseline and implement board-report

## Status

done

## Handoff

- Updated: 2026-10-19, claude.
- State: done. Every Phase 1 item has a pass or fail result with evidence.
- Done, on the board on 2026-10-19 (BeagleBoard.org Debian 13, kernel
  6.18.55-arm64-k3-r59, U-Boot 2026.10):
  - `board-report` ran twice, then again after a reboot; the reports differ
    only in time, free memory, and temperature ([`release/evidence/task-009/20261019T094327Z/`](../release/evidence/task-009/20261019T094327Z/)).
  - `systemctl reboot` over SSH: back after 53 s, and the previous boot's
    journal ends with a clean shutdown (`reboot.txt`).
  - Passed: SSH, remoteproc for all three DSPs, kernel features, CMA (144
    MiB), 20 reserved-memory nodes, storage, debugfs and tracefs,
    temperature, persistent logs, no failed units.
  - Failed, for lack of a network cable, not a board defect: Ethernet (no
    link) and time (not synchronized; no RTC, so the clock restarts at the
    image date).
- Next: Task 010. Connecting Ethernet makes both failed items testable.
- Decisions: none.
- Verification, 2026-10-19: the three reports above; `ci.test_dt_memory`
  and the board-suite unit tests pass in `make quality`.

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
