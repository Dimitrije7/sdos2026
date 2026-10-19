# 010 — Establish Linux stability and decide Gate 1

## Status

ready

## Handoff

- Updated: 2026-10-19, claude.
- State: ready; Task 009 is done.
- Next: needs a network cable for Ethernet and time synchronization.

## Depends on

009.

## Plan references

Phase 1, Gate 1, and the stability/recovery rows of Section 7.

## Objective

Demonstrate that the candidate Linux baseline is stable enough for DSP
bring-up, with agent-operated repetition and evidence.

## Scope

- Automate reboot/startup health checks and a two-hour baseline soak.
- Exercise storage, network, logs, time, thermal monitoring, and clean shutdown.
- Perform the planned 20 cold boots when controllable power is already
  available; otherwise measure reboot cycles and prepare an evidence-based
  decision on the remaining cold-boot risk.
- Correlate every failure with serial and system logs.

## Out of scope

Building a HIL rig, asking the maintainer to repeat power cycles, or DSP soak.

## Maintainer checkpoint

`hardware-ready`. If true cold cycling cannot be agent-controlled, present the
options and a recommendation for a Gate 1 decision rather than assigning the
cycles to the maintainer.

## Acceptance criteria

- The two-hour soak and all agent-controllable cycles pass without unexplained
  kernel, storage, network, power, or thermal faults.
- The cold-boot requirement is either evidenced or explicitly decided with its
  residual risk recorded.
- Gate 1 has a signed-off pass, conditional pass, or fail result.

## Verification and evidence

Archive per-cycle results, temperatures, kernel warnings, failure diagnostics,
and the Gate 1 record.
