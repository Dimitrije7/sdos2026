# 031 — Qualify the data path and decide Gate 4

## Status

pending

## Depends on

026, 028, 029, and 030.

## Plan references

Phase 4, Gate 4, and the IPC/shared-data/performance rows of Section 7.

## Objective

Prove sustained, recoverable, corruption-free control and bulk transfer on
each DSP and all three concurrently.

## Scope

- Test sizes from one cache line through multi-megabyte blocks, alignments,
  guards, checksums, sequence wrap, backpressure, and timeouts.
- Run bidirectional transfers on every core/runtime profile and concurrently.
- Inject malformed requests, stalled firmware, controlled crashes, and host
  interruption; verify bounded recovery and restart.
- Measure control latency, throughput, A72/DSP load, DDR effects, and
  temperature with repeatable metadata.
- Establish regression baselines and tolerances without hiding variance.

## Out of scope

Final algorithm performance claims or treating one successful run as a soak.

## Maintainer checkpoint

`hardware-ready`; the agent executes the full matrix and presents the Gate 4
result.

## Acceptance criteria

- Integrity and recovery pass for every core/profile individually and all three
  together.
- No timeout/crash path leaks ownership or requires undocumented board repair.
- Performance baselines include environmental and frequency context.
- Gate 4 has an indexed pass/fail record.

## Verification and evidence

Archive machine-readable stress/fault/benchmark results, correlated logs, raw
samples, summarized baselines, and the Gate 4 decision.
