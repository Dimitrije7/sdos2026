# 027 — Specify and decide the host/DSP protocol and data path

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: protocol 2.0 is specified, implemented, and tested on the PC. It
  replaced the unreleased 1.0 on 2026-10-19 for Task 042: an end-to-end
  message CRC, safety status codes, RUN budgets, and the safety counters. The
  maintainer decision is pending, and the board data path is open until Task
  012's memory audit.
- Done: `docs/protocol.md` (wire format, status codes, compatibility,
  ownership table, cache duties, restart and stale-message rules, options);
  `firmware/common/protocol/` with golden, rejection, ownership-table, CRC,
  and 200,000-case fuzz tests (`test_protocol`).
- Next:
  1. Present the recommendation in `docs/protocol.md` ("Decision needed") to
     the maintainer: accept 2.0 over upstream rpmsg_char.
  2. After Task 012, recommend the shared-region mechanism (likely option A,
     a dedicated carve-out with a small driver) and record the decision.
- Decisions: on 2026-10-18 the maintainer wrote "proceed with plan, but do everything except board tests, also postpone any decision making from my side because I will not be able to respond". This board-free work therefore started before its board-side dependencies were done; acceptance still needs them. The recommendation is written; nothing is accepted.
- Verification, 2026-10-19: `make test PRESET=host` passed the protocol and
  runtime tests on gcc and clang, debug and release.

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
