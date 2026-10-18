# 007 — Build safe connected-board access

## Status

in-progress

## Handoff

- Updated: 2026-10-18, claude.
- State: Implementation and mocked verification are complete and committed.
  The task waits at its hardware-ready checkpoint for the final read-only
  discovery proof.
- Done:
  - [`tests/target/sdos_board/`](../tests/target/sdos_board/): discovery,
    exact-identity guards that never write, SSH host-key identity,
    receive-only serial capture, and a CLI.
  - [`tests/target/test_sdos_board.py`](../tests/target/test_sdos_board.py)
    with fixtures for a healthy and a hazardous bench.
  - [`docs/board-access.md`](../docs/board-access.md).
- Next:
  1. Hardware-ready: ask the maintainer to connect the board with power, the
     USB-UART adapter on its debug header, Ethernet on the PC's network, and
     the microSD reader with a card.
  2. Run `discover --host <board>`, a short `capture`, `ssh-identity`, and
     `check-write-target`, all read-only. Store redacted outputs under
     `release/evidence/task-007/`.
  3. Record the observed UART and reader as inventory assets, then complete
     the task.
- Decisions: none.
- Verification, 2026-10-18:
  - `python3 -m unittest tests.target.test_sdos_board` passed 19 tests on
    Python 3.14 and on Python 3.10 in `python:3.10-slim`.
  - A live discovery on this workstation found no UART or reader and excluded
    the system NVMe disk.
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
