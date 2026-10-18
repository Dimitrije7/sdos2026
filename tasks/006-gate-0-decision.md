# 006 — Assemble and decide Gate 0

## Status

done

## Completion evidence

- [`release/evidence/gate-0/gate-0-record.md`](../release/evidence/gate-0/gate-0-record.md)
  maps every Phase 0 deliverable to evidence, names conditions G0-1 to G0-5
  with owners, and freezes the baseline by SHA-256 at commit `802293b`.
- Decision, 2026-10-18, quoted in the record: "1. I accept" to the conditional
  go and the two-track plan, and "2. both options" to the kernel question,
  recorded in [Task 035](035-reproducible-image-builder.md).
- G0-4 is applied: license matrix revision 5 gates `LIC-001` on publication of
  any TI-derived artifact, owned by Task 019.
- Open conditions carried forward: G0-1 power and G0-3 the J3 debug cable,
  both owned by the maintainer.
- Verification: `python3 ci/validate_licenses.py` and
  `python3 ci/validate_versions.py` pass.

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
