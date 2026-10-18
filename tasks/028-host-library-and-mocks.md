# 028 — Implement the Linux host library and transport mocks

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: `libsdos_dsp` works against the mock backend and against a simulated
  kernel; shared buffers on the board wait for the Task 027 data-path decision.
- Done: public API `host/libsdos_dsp/include/sdos/dsp.h`; session logic with
  request correlation, timeouts, stale-reply dropping, version negotiation,
  ownership with quarantine and restart generations (`src/dsp.c`); Linux
  backend over remoteproc sysfs and rpmsg_char (`src/backend_linux.c`); mock
  backend running the DSP runtime with drop/corrupt/stale fault injection
  (`src/backend_mock.c`); ELF check before load (`src/elf_check.c`). Tests:
  `test_dsp_mock`, `test_dsp_linux` (fake sysfs with remoteproc numbering
  that differs from manifest order, a kernel-emulating thread, and a
  SOCK_SEQPACKET endpoint).
- Next: board validation of the Linux backend; buffers once Task 027 decides;
  sanitizer runs (Task 023); a shared-library and Debian package for the board.
- Decisions: on 2026-10-18 the maintainer wrote "proceed with plan, but do everything except board tests, also postpone any decision making from my side because I will not be able to respond". This board-free work therefore started before its board-side dependencies were done; acceptance still needs them.
- Verification, 2026-10-19: `make test PRESET=host` passed all six CTest tests
  in four configurations; `aarch64-gcc-*` built with warnings as errors.

## Depends on

027.

## Plan references

Phase 4 and the component/system tests in Section 6.6.

## Objective

Provide a stable Linux API for DSP discovery, lifecycle, control messages, and
buffers without exposing sysfs or RPMsg details to labs.

## Scope

- Implement stable DSP discovery and remoteproc lifecycle adapters.
- Wrap the selected rpmsg-char/TI transport behind a narrow interface.
- Implement protocol negotiation, request correlation, timeouts, diagnostics,
  cleanup, and restart-safe state handling.
- Add mock remoteproc/RPMsg transports for deterministic unit/component tests.
- Build and package native-host and AArch64 variants with a documented API.

## Out of scope

CLI UX, DSP cache operations, or treating mock success as hardware proof.

## Maintainer checkpoint

None if the implementation preserves the accepted Task 027 interface.

## Acceptance criteria

- Host unit/component tests cover lifecycle and protocol success/failure paths.
- The AArch64 library packages cleanly and exposes no raw enumeration contract.
- Timeouts and cleanup cannot leave silent stale ownership.

## Verification and evidence

Run GCC/Clang/debug/release/sanitizer tests, mock system tests, ABI/API checks,
and AArch64 package installation validation.
