# 008 — Flash the candidate image and capture first boot

## Status

in-progress

## Handoff

- Updated: 2026-10-18, claude.
- State: The card is flashed and verified, with root credentials added; it
  waits to be booted in the board.
- Done:
  - The pinned image and block map match their manifest SHA-256 values, and
    the decompressed image matches `expanded_sha256`.
  - The maintainer ran the guarded `flash` command. It wrote 10,737,418,240
    bytes in 593 s and the 128 s read-back matched `expanded_sha256`. Record:
    [`release/evidence/task-008/20261018T162635Z-flash.json`](../release/evidence/task-008/20261018T162635Z-flash.json),
    card serial redacted.
  - The image creates no login by default. After the verified write, one change
    was made: `root_password=root` and `root_authorized_key` with the bench key
    `~/.ssh/sdos-bench_ed25519` (fingerprint
    `SHA256:Oh4FgxbXZ3R6XDApOm7Elryi2sB1vMPBrVQIVzl6MWY`, private key only on
    the workstation) were appended to `sysconf.txt` on the BOOT partition.
    Original `sysconf.txt` SHA-256:
    `ce854cf8f45a2df756e2caf60feaa1057343fd4de11590e8806398da4fd68db7`.
- Next:
  1. The maintainer moves the card to the board and powers it holding BOOT.
  2. Confirm the SD boot over SSH as root at `192.168.7.2` with the bench key:
     kernel 6.18.55, `/etc/dogtag`, U-Boot version, host keys; capture the USB
     console after the maintainer's `setfacl` on `/dev/ttyACM0`.
  3. Archive the first-boot summary. The J3 boot-loader log waits for G0-3.
- Decisions, 2026-10-18: Gate 0 accepted ("1. I accept"). Flash approval: the
  maintainer ran the `flash` command for `mmc-SD32G_0x6f88078d` after the
  agent showed the target record. Credentials: "just make user named root and
  password root and make it permanent, to keep it simple". Root with password
  `root` is the bench setting only; the course image must not ship default
  credentials (Task 036).
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
