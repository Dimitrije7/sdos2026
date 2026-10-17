# 026 — Prove clean reproducibility, deployment, and Gate 3

## Status

pending

## Depends on

015 and 019 through 025.

## Plan references

Phase 3 and Gate 3.

## Objective

Prove that an unprepared supported workstation and GitHub-hosted CI can produce
the same checked firmware and deploy it without SDK surgery.

## Scope

- Execute bootstrap through package from clean clones and empty project caches.
- Compare repeat firmware outputs or explain deterministic functional
  equivalence where tool metadata prevents byte identity.
- Deploy every course template to its matching DSP and rerun Gate 2 smoke.
- Exercise the full local-versus-CI quality and test contract.
- Confirm deliberate broken samples fail their claimed gates.

## Out of scope

Shared-buffer data path, finished student examples, or full Linux image.

## Maintainer checkpoint

`hardware-ready` for deployment and target smoke; the agent executes all steps.

## Acceptance criteria

- The complete 12-variant course firmware matrix builds cleanly from scratch.
- Local and CI manifests/check results agree.
- All deployed templates pass per-core and concurrent smoke without manual SDK
  edits.
- Gate 3 has an indexed pass/fail record.

## Verification and evidence

Archive clean-build timing/storage, hashes, CI links, deployment logs, target
smoke results, and the Gate 3 record.
