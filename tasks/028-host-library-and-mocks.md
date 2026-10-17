# 028 — Implement the Linux host library and transport mocks

## Status

pending

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
