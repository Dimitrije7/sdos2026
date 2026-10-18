# 013 — Prove TI IPC on each DSP independently

## Status

in-progress

## Handoff

- Updated: 2026-10-18, claude.
- State: the board-free part is done: pinned firmware, a fit check, and the
  guarded `dsp-ipc` procedure with simulated-kernel tests. The board part
  runs in the next session, after Tasks 011 and 012 pass in that session.
- Done:
  - Firmware resolved and hashed: `manifest/versions.json` revision 4 pins
    TI's `ipc_echo_test` blobs for `c66xdsp_1`, `c66xdsp_2`, and `c7x_1` and
    `LICENSE.ti` at ti-linux-firmware commit `e853cc767f4b` (TI's GitHub
    mirror; git.ti.com is still unreachable). Release tag
    REL.PSDK.11.02.01.05, one patch level below the installed PDK
    11.02.01.08. The license row is in `manifest/license-matrix.json`
    revision 7.
  - `manifest/dsp-cores.json` maps each logical core to its device address,
    TI core name, kernel firmware name, and carve-outs.
  - `ci/firmware_elf.py` checks machine, segment placement, and the
    resource table. All three TI blobs fit their own core and are refused
    for any other; each has an RPMsg vdev with two 256-buffer vrings. The
    C66x blobs have a 512 KiB trace buffer; the C71x blob has none, so its
    trace file will say so.
  - `python3 -m tests.target.sdos_board dsp-ipc` with `REMOTE_AGENT`, tested
    in `tests/target/test_dsp_ipc.py` against a simulated remoteproc and
    rpmsg_char; documented in `docs/board-access.md`.
- Next, in the board session after 011 and 012:
  1. `dsp-ipc --key ~/.ssh/sdos-bench_ed25519 --out
     release/evidence/task-013/<UTC stamp>` for all three cores.
  2. If `rpmsg_ctrl` never appears, check `CONFIG_RPMSG_CTRL` in the board
     report and whether the kernel binds `rpmsg_chrdev` or only `rpmsg-raw`;
     record the finding before changing anything.
  3. Run it a second time to show the transitions are repeatable.
  4. Known risk: TI's echo firmware waits until every remote core in its list
     is ready, including R5F cores that may run no firmware on this board
     (`Ipc_isRemoteReady` loop in TI's ipc_testsetup.c). If no `rpmsg_ctrl`
     appears and the trace stops at that wait, record it, then run the course
     firmware (Task 021), which talks only to the A72: copy
     `build/aarch64-gcc-release/host/dspctl` and
     `build/<core>-freertos-release/firmware/*.xe6*` to the board and use
     `dspctl load|start|echo|status|stop`.
- Decisions: on 2026-10-18 the maintainer wrote "proceed with plan, but do
  everything except board tests, also postpone any decision making from my
  side". The board-free preparation therefore ran before 011 and 012 were
  done; acceptance still needs the board run.
- Verification, 2026-10-18: `python3 -m unittest tests.target.test_dsp_ipc
  ci.test_firmware_elf` passed 13 tests; `ci/firmware_elf.py` passed each
  pinned blob for its own core.

## Depends on

011 and 012.

## Plan references

Phase 2 and the remoteproc/IPC rows of Section 7.

## Objective

Boot TI's pinned, unmodified IPC firmware on each DSP separately and prove the
basic Linux/firmware contract before developing course firmware.

## Scope

- Resolve and hash the correct TI firmware for each logical DSP.
- Load, start, observe trace/RPMsg, run echo, stop, and restart one DSP at a
  time through guarded scripts.
- Capture kernel, remoteproc, RPMsg, and firmware traces for every transition.
- Characterize failures without altering kernel/DT/firmware inputs mid-run.

## Out of scope

Concurrent DSP use, course firmware, shared DDR, or performance claims.

## Maintainer checkpoint

`hardware-ready`: keep the board connected; the agent owns firmware deployment
and test execution.

## Acceptance criteria

- C66x_0, C66x_1, and C71x_0 each pass load/start/echo/stop/restart.
- Artifacts and platform versions match the manifest and memory audit.
- Every transition is repeatable from a versioned command.

## Verification and evidence

Archive a machine-readable per-core result and the supporting serial, kernel,
remoteproc, RPMsg, and firmware logs.
