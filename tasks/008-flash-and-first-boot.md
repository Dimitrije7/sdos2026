# 008 — Flash the candidate image and capture first boot

## Status

in-progress

## Handoff

- Updated: 2026-10-18, claude.
- State: Preparing the flash. The pinned image is downloading to
  `~/.cache/sdos/inputs/beagle-2026-10-09/`, outside the repository.
- Done:
  - The block map downloaded and matched its manifest SHA-256.
  - `flash` command added to `tests/target/sdos_board`, with tests: it repeats
    the write-target check, requires `--confirm`, checks the download hash,
    opens the card with `O_EXCL`, and verifies a full read-back against
    `expanded_sha256`.
  - Target observed: `mmc-SD32G_0x6f88078d`, `/dev/mmcblk0`, 29.1 GiB, in the
    PC's built-in slot, holding `p1` vfat BOOT 300 MB auto-mounted at
    `/run/media/dimitrije/BOOT`, `p2` ext3 1.2 GB, and `p3` 10 MB.
- Next:
  1. Verify the image SHA-256 after download.
  2. Ask the maintainer to approve the exact card, then have them unmount
     `p1` and run `flash` with `sudo`; archive the JSON record.
  3. Boot from the card with the board on USB-C and record the Linux side:
     kernel, U-Boot version from `/proc` and logs, SSH host keys, and
     `/etc/dogtag`. The J3 boot-loader log waits for condition G0-3.
- Decisions: Gate 0 accepted 2026-10-18 with "1. I accept". The maintainer
  reconnected the board over USB-C: "connected via usb c continue".
- Verification: `python3 -m unittest tests.target.test_sdos_board` passed 32
  tests on 2026-10-18.

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
