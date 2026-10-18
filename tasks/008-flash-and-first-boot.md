# 008 — Flash the candidate image and capture first boot

## Status

ready

## Depends on

006 and 007.

## Plan references

Phase 1 and the execution model.

## Objective

Produce a verified candidate microSD and an immutable first-boot evidence set.

## Scope

- Download and hash-check the manifest-pinned Beagle image.
- Discover the exact removable-media device and prove it is not a system disk.
- Flash, verify, and safely eject the card after exact-target approval.
- Capture the complete serial boot, bootloader/kernel versions, and first
  network/SSH identity.
- Preserve a known-good source image and recovery instructions by hash.

## Out of scope

eMMC overwrite, DSP firmware replacement, or unattended destructive access.

## Maintainer checkpoint

`hardware-ready`: connect the board/UART/network and card media. Then approve
the one destructive write only after the agent reports exact device identity,
model, size, and mount state.

## Acceptance criteria

- Read-back verification matches the intended image.
- Serial evidence covers power-on through a usable login/network state.
- Every captured version maps to the candidate manifest.

## Verification and evidence

Archive hashes, the target-identification record, flashing result, serial log,
and first-boot summary under a timestamped evidence directory.
