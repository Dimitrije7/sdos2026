# 007 — Build safe connected-board access

## Status

pending

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
