# 006 — Assemble and decide Gate 0

## Status

in-progress

## Handoff

- Updated: 2026-10-18, claude.
- State: The Gate 0 record is prepared with a conditional-go recommendation
  and waits for the maintainer's decision.
- Done: [`release/evidence/gate-0/gate-0-record.md`](../release/evidence/gate-0/gate-0-record.md)
  holds the evidence index, the observed bench, conditions G0-1 to G0-5,
  alternatives, and baseline hashes at commit `802293b`.
- Next:
  1. Record the maintainer's answer verbatim in the gate record and here.
  2. If accepted: move `LIC-001`'s gate as in G0-4, set Task 008 to `ready`,
     and complete this task.
  3. If any baseline file changes before the decision, recompute its hash in
     the record.
- Decisions: pending. Asked 2026-10-18 whether to accept conditional go with
  conditions G0-1 to G0-5.
- Verification: the baseline hashes were computed at commit `802293b`.

## Depends on

001, 002, 003, 004, and 005.

## Plan references

Phase 0 and Gate 0.

## Objective

Convert Phase 0 evidence into a go/no-go recommendation for physical bring-up.

## Scope

- Audit every Phase 0 deliverable and unresolved risk.
- Reconcile policy, BOM, licenses, manifest pins, and runner feasibility.
- Recommend `go`, `conditional-go`, or `no-go`, with exact conditions.
- Record the accepted Gate 0 baseline and freeze its inputs by hash.

## Out of scope

Board flashing, Linux qualification, or silently waiving an unmet condition.

## Maintainer checkpoint

`maintainer-decision`: approve the Gate 0 result or select a documented
alternative. The agent supplies the recommendation and evidence index.

## Acceptance criteria

- Every Gate 0 condition has direct evidence and an owner.
- The accepted candidate stack is reproducible from the manifest.
- Physical bring-up cannot proceed on an ambiguous license or hardware target.

## Verification and evidence

Store the signed-off gate record and evidence links under `release/evidence/`
or the adopted decision-record location.
