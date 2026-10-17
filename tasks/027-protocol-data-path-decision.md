# 027 — Specify and decide the host/DSP protocol and data path

## Status

pending

## Depends on

015 and 023.

## Plan references

Section 3, Phase 4, and the IPC/shared-data risks in Section 8.

## Objective

Freeze a small, versioned, testable boundary for RPMsg control and shared-DDR
sample transfer before host and firmware implementations diverge.

## Scope

- Define identities, capabilities, version negotiation, request IDs, status
  codes, timeouts, bounded messages, and compatibility rules.
- Define descriptor encoding, ranges, alignment, sequence/checksum fields,
  ownership states, and cache transition responsibilities.
- Define restart, stale-message, malformed-input, and version-mismatch behavior.
- Provide portable encode/decode/state-machine prototypes and tests.
- Compare upstream rpmsg-char and TI transport options behind one interface.

## Out of scope

Implementation-specific API sprawl, embedding sample payloads in RPMsg, or
assuming hardware coherence.

## Maintainer checkpoint

`maintainer-decision`: approve the recommended protocol/data-path contract and
its compatibility policy after reviewing alternatives and reversibility.

## Acceptance criteria

- The wire format is size/range bounded, endian-explicit, versioned, and
  representable in both host C++ and DSP C.
- Ownership/cache transitions form a complete validated state machine.
- Error/restart semantics and compatibility rules have executable tests.

## Verification and evidence

Run golden serialization, fuzz/property-style invalid-input, and state-machine
tests; record the accepted protocol decision and schema.
