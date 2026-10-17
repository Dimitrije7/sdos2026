# 038 — Build the connected-board qualification framework

## Status

pending

## Depends on

031 and 037.

## Plan references

Phase 7 and the acceptance matrix in Section 7.

## Objective

Provide one agent-run command family and evidence schema for complete platform
qualification without introducing a persistent HIL service.

## Scope

- Orchestrate Linux health, image manifest, per-core/profile lifecycle, RPMsg,
  shared data, numerical, fault, recovery, performance, thermal, and soak tests.
- Consume exact CI artifacts by hash and refuse incompatible board/image state.
- Resume long runs safely and preserve partial/failure evidence.
- Produce per-board machine-readable results plus concise qualification reports.
- Add mocked orchestration tests so PRs validate the harness without hardware.

## Out of scope

GitHub-hosted physical-board jobs, self-hosted runners, or unattended power rigs.

## Maintainer checkpoint

`hardware-ready`; the agent runs qualification from the connected PC.

## Acceptance criteria

- One documented invocation covers every Section 7 hardware acceptance row.
- Artifacts, board/image identity, environment, commands, and results are
  traceable by hash.
- Interruption/failure cannot produce a false pass or lose diagnostics.

## Verification and evidence

Run orchestration mock tests, interrupted/resumed cases, and one complete board
qualification with schema-validated evidence.
