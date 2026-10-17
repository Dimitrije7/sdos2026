# 009 — Qualify the Linux baseline and implement board-report

## Status

pending

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
