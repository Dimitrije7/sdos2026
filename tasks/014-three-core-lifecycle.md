# 014 — Prove concurrent three-DSP lifecycle and recovery

## Status

pending

## Depends on

013.

## Plan references

Phase 2 and the concurrent-use, remoteproc, and IPC rows of Section 7.

## Objective

Run both C66x cores and the C71x core together and prove lifecycle isolation,
communication, crash visibility, and recovery.

## Scope

- Start all three known TI IPC firmwares and run concurrent echo traffic.
- Exercise different start/stop orders and repeated lifecycle cycles.
- Inject a supported firmware failure or controlled fault per core.
- Verify one-core recovery does not corrupt or silently stop the others.
- Capture traces, endpoint enumeration, memory faults, and kernel diagnostics.

## Out of scope

Course shared-buffer protocol, long-term performance tuning, or HIL automation.

## Maintainer checkpoint

`hardware-ready`; all execution remains agent-operated from the connected PC.

## Acceptance criteria

- Three DSPs operate concurrently with stable logical identities and RPMsg.
- Stop/restart and controlled failure behavior are observable and recoverable.
- Repetition reveals no unexplained carve-out, IOMMU, cache, or kernel error.

## Verification and evidence

Archive the lifecycle matrix, fault results, iteration counts, and correlated
host/firmware logs.
