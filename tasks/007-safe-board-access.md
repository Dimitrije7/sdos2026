# 007 — Build safe connected-board access

## Status

in-progress

## Handoff

- Updated: 2026-10-18, claude.
- State: Read-only discovery ran against the connected board. Serial capture on
  hardware remains, and it needs the workstation account in `dialout`.
- Done:
  - Code, tests, and [`docs/board-access.md`](../docs/board-access.md).
  - Bench as connected by the maintainer: board USB-C to the PC's USB-A port,
    with the board booted from its factory eMMC image dated 2022; microSD card
    in the PC's built-in SD slot, holding an older image; no USB-UART adapter
    and no Ethernet.
  - The live run found two defects, now fixed and tested. The board's gadget
    drive was listed as a reader. The shared gadget serial `1234BBBK5678` was
    treated as unique. Built-in SD slots are now supported, and approval
    records list the partitions a write would erase.
  - Evidence: [`release/evidence/task-007/`](../release/evidence/task-007/)
    holds the redacted discovery, the SSH host-key check, and the refused write
    target, because the card's BOOT partition was auto-mounted.
- Next:
  1. After the maintainer adds the account to `dialout` and logs in again, run
     `capture --uart usb-BeagleBoard.org_BeagleBone_1234BBBK5678-if04 --seconds
     20` read-only and archive a redacted log.
  2. Complete the task: record the console, card, and board host keys as
     inventory observations.
- Decisions, 2026-10-18: the maintainer said they have one USB-A and one USB-C
  port, no Ethernet cable from PC to board, and can insert the SD card into the
  PC directly. They then reported "board is connected via USB C to USB A of
  this PC". The SD card already holds an older Linux image.
- Verification, 2026-10-18: `python3 -m unittest tests.target.test_sdos_board`
  passed 26 tests. Live discovery and `ssh-identity` exited 0;
  `check-write-target` refused the mounted card as designed.
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
