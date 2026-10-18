# 007 — Build safe connected-board access

## Status

in-progress

## Handoff

- Updated: 2026-10-18, claude.
- State: Read-only discovery and SSH identity are proven on the connected
  board. A serial log is still missing: the USB console needs account access,
  and the UART adapter cannot reach the J3 debug header yet.
- Done:
  - Code, tests, and [`docs/board-access.md`](../docs/board-access.md).
  - The first live run found two defects, now fixed and tested: the board's
    gadget drive was listed as a reader, and the shared gadget serial
    `1234BBBK5678` was treated as unique. Built-in SD slots are supported, and
    approval records list the partitions a write would erase.
  - Evidence: [`release/evidence/task-007/`](../release/evidence/task-007/)
    holds the redacted discovery, the SSH host-key check, and the refused write
    target, because the card's BOOT partition was auto-mounted.
  - UART attempts on 2026-10-18 with the maintainer's CH340 USB-to-ESP-01
    adapter (`usb-1a86_USB_Serial-if00-port0`, no unique serial) wired to
    P9.11/P9.13 received 1 to 3 zero bytes per power cycle and no text, in
    both wire orders. The stock image routes UART0 to the J3 debug header, not
    to P9. The adapter's Dupont wires do not fit J3.
  - After the maintainer reconnected USB-C to the PC, `ssh-identity` on
    `192.168.7.2` returned the three host-key fingerprints recorded earlier, so
    it is the same board.
- Next:
  1. After a reboot activates `dialout`, or a `setfacl` grant on `/dev/ttyACM0`,
     run `capture --uart usb-BeagleBoard.org_BeagleBone_1234BBBK5678-if04
     --seconds 20` read-only and archive a redacted log.
  2. Complete the task: record the console, card, and board host keys as
     inventory observations. The J3 boot-loader log belongs to Task 008 under
     Gate 0 condition G0-3.
- Decisions, 2026-10-18: the maintainer said they have one USB-A and one USB-C
  port, no Ethernet cable from PC to board, and can insert the SD card into the
  PC directly. Later they wrote "I will connect UART to USB A port via
  converter" and, after the UART attempts, "connected via usb c continue".
- Verification, 2026-10-18: `python3 -m unittest tests.target.test_sdos_board`
  passed 26 tests. Live discovery and `ssh-identity` exited 0;
  `check-write-target` refused the mounted card as designed. The P9 captures
  are kept outside the repository; they contain only zero bytes.

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
