# 013 — Prove TI IPC on each DSP independently

## Status

pending

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
