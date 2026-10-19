# 013 — Prove TI IPC on each DSP independently

## Status

blocked

## Handoff

- Updated: 2026-10-19, claude.
- State: blocked on a maintainer decision. TI's own echo firmware cannot pass
  on the C66x on this board; the course firmware proves the same IPC path on
  every core (Task 021).
- Done, on the board on 2026-10-19
  ([evidence](../release/evidence/task-013/20261019T095039Z/)):
  - All three TI blobs load, start, and announce `rpmsg_chrdev` and
    `ti.ipc4.ping-pong`.
  - C71x_0 echoed 11 of 11 in the first cycle. The C66x firmware answered
    nothing: TI's RTOS IPC library asks the device manager for a mailbox
    interrupt route, which this board's boot configuration refuses to the
    C66x (shown with the course firmware, Task 021).
  - TI's echo firmware also pings the device manager on MCU R5F 0 through
    0xAA000000. Repeated restarts exhausted the device manager's IPC heap
    and stopped TI-SCI until a reboot (`mcu_r5f0_0-dm-trace.txt.xz`). The
    suite does not run TI's echo firmware in loops for that reason.
- Next: decide whether the course firmware's IPC proof (Task 021: load,
  start, echo, stop, and restart on all three DSPs in both runtimes, twice)
  replaces this task's TI-firmware proof. Recommendation: yes. The course
  firmware uses the same TI IPC library without the interrupt routes the
  board refuses, and TI's echo firmware interferes with the device manager.
- Decisions: none yet.
- Verification, 2026-10-19: `dsp-ipc` for all three cores, evidence above.

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
