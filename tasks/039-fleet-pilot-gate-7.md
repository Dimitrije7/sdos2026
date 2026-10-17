# 039 — Qualify the fleet, run the pilot, and decide Gate 7

## Status

pending

## Depends on

034, 037, and 038.

## Plan references

Phase 7 and Gate 7.

## Objective

Demonstrate release-candidate reliability across representative boards and the
actual lab fleet while keeping execution agent-operated.

## Scope

- Qualify at least three boards early and every available lab board before
  freeze using Task 038.
- Run 30 cold boots where agent-controlled power exists, 100 DSP lifecycle
  cycles, overnight three-DSP soak, network/storage/power-loss scenarios,
  corrupt firmware, damaged-rootfs recovery, and thermal/performance envelopes.
- Run the complete pilot lab with an independent participant.
- Turn reproducible failures into automated health/pre-lab checks where safe.
- Quantify setup success and recovery time against Gate 7.

## Out of scope

Asking the maintainer to execute repetitive cycles or silently substituting warm
reboots for required cold boots.

## Maintainer checkpoint

`hardware-ready`: connect each board/media when requested. If physical power
cycling cannot be agent-controlled, receive a recommendation and decide how to
resolve or explicitly accept that release risk. Approve the Gate 7 result.

## Acceptance criteria

- Every connected fleet board has a traceable qualification record.
- At least 95% of pilot setups require no platform-author intervention.
- Performance/thermal ranges and recovery under 10 minutes are evidenced.
- Gate 7 has no unexplained failure or unowned release risk.

## Verification and evidence

Archive per-board reports, fleet summary, cycle/soak/fault logs, pilot findings,
performance envelopes, recovery timings, and the Gate 7 decision.
