# 007 — Build safe connected-board access

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: discovery, SSH identity, and safe write targets are proven on the
  board. Only the serial log is missing.
- Done: see the evidence in [`release/evidence/task-007/`](../release/evidence/task-007/).
  On 2026-10-19 the board's USB console enumerated as
  `usb-BeagleBoard.org_BeagleBone-if02` (`/dev/ttyACM0`, group `dialout`).
  The maintainer's account is in `dialout`, but the running login session
  predates that, and neither `sg` nor `newgrp` is installed. The SSH host key
  matched the fingerprint recorded in Task 008.
- Next: after a fresh login, or `sudo setfacl -m u:dimitrije:rw /dev/ttyACM0`,
  run `capture --uart usb-BeagleBoard.org_BeagleBone-if02 --seconds 20`
  read-only and archive a redacted log. Then the task is complete.
- Decisions: see the earlier UART notes in this file's history (P9 wiring,
  J3 header).
- Verification, 2026-10-19: `ls -l /dev/serial/by-id/`; capture not run.

## Depends on

004.

## Plan references

Execution model, Phase 1, and the safety rules in `AGENTS.md`.

## Objective

Give agents a deterministic, non-destructive way to discover and control a
connected board, UART, network endpoint, and removable media.

## Scope

- Implement discovery for stable USB/UART identities, block-device identity,
  network reachability, and SSH host identity.
- Add serial capture with timestamps and loss-resistant logs.
- Define a board inventory record and connection diagnostics.
- Add guarded helpers that refuse ambiguous devices and default to read-only.
- Validate the workflow with mocked device listings before hardware arrives.

## Out of scope

Flashing, changing board state, assuming a device path, or adding automated HIL.

## Maintainer checkpoint

`hardware-ready` only for the final discovery proof: connect the board,
USB-UART, network, and card reader/media requested by the agent.

## Acceptance criteria

- Discovery produces stable identities without relying on enumeration order.
- Ambiguous/missing devices fail safely with actionable diagnostics.
- Serial and connection metadata are captured without requiring maintainer
  command entry.

## Verification and evidence

Run mocked failure cases and one read-only connected-device discovery; store a
redacted sample inventory and serial log.
