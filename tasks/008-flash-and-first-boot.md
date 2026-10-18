# 008 — Flash the candidate image and capture first boot

## Status

in-progress

## Handoff

- Updated: 2026-10-18, claude.
- State: The board booted the pinned image from the card and its first-boot
  evidence is archived. The only unmet criterion is a serial log from power-on:
  that needs the J3 debug cable (Gate 0 condition G0-3). Waiting for the
  maintainer to accept the kernel log as interim evidence or wait for the cable.
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
  - First boot, 2026-10-18, over SSH as root with the bench key at
    `192.168.7.2` (the board's USB network; the maintainer held BOOT at power-on):
    root filesystem `/dev/mmcblk1p3` on the card, grown to 24.9 GB; image
    "BeagleBoard.org Debian Trixie IOT Image 2026-10-09"; kernel
    `6.18.55-arm64-k3-r59` PREEMPT_RT from package
    `linux-image-6.18.55-arm64-k3-r59 1trixie`; U-Boot `2026.10-gb4086bd599be`
    from `bb-u-boot-beagleboneai64 2026.10.20261005.0-0~trixie+20261007`. All
    map to manifest inputs. Startup took 23.0 s; 3.7 GB RAM visible.
  - DSPs: C66x_0 (`4d80800000.dsp`, remoteproc0, reserved memory at
    `0xa7000000`), C66x_1 (`4d81800000.dsp`, remoteproc1, `0xa6000000`), and
    C71x_0 (`64800000.dsp`, remoteproc2, `0xa8000000`) probe and stay offline
    because `j7-c66_0-fw`, `j7-c66_1-fw`, and `j7-c71_0-fw` are absent. This
    confirms on hardware the Task 005 static finding.
  - The image's login banner advertises a default `debian` account with
    password `temppwd`; Task 036 must remove it from the course image.
  - The board answers with new SSH host keys, as expected after reflashing.
  - Evidence in [`release/evidence/task-008/`](../release/evidence/task-008/):
    `20261018T175900Z-firstboot.txt`, `-dmesg.txt` (kernel log from 0 s),
    `-journal.txt.xz`, and `-ssh-identity.json`.
- Next:
  1. Record the maintainer's choice on the serial criterion. If the kernel log
     is accepted for now, complete this task with the J3 boot-loader log
     carried to G0-3 and unblock Task 009.
  2. Otherwise, when the J3 cable arrives, capture a power-on log with
     `capture --until 'login:'` and complete the task.
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
