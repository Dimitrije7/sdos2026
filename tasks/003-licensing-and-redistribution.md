# 003 — Audit licensing and redistribution

## Status

in-progress

## Handoff

- Updated: 2026-10-18, claude.
- State: Every acceptance criterion except the maintainer decision has
  evidence. The decision packet was presented on 2026-10-18; the task waits
  for the answer.
- Done:
  - Codex: primary-source research in
    [`docs/research/task-003-licensing-evidence.md`](../docs/research/task-003-licensing-evidence.md),
    the proposed [`docs/licensing-policy.md`](../docs/licensing-policy.md),
    matrix revision 1 in [`manifest/license-matrix.json`](../manifest/license-matrix.json),
    its schema, and [`ci/validate_licenses.py`](../ci/validate_licenses.py)
    with tests. Its final patch pinned candidate SDK, compiler, and DSPLIB
    versions, recorded the Beagle image's 178 unlicensed inventory entries, and
    replaced a silent Lychee choice with an unresolved link-checker row.
  - Claude: reconciled the policy table with that patch; cross-checked the
    matrix against the plan and added the missing `ti.rpmsg-char-library` and
    `ti.code-composer-studio` rows plus a `debug-tool` schema category; fixed
    the broken actionlint license link; added policy items 8 and 9 for the
    first-release image model and copyleft source delivery.
- Next:
  1. Record the maintainer's answer below, quoted and dated.
  2. If accepted: set the policy status to accepted, set the matrix
     `decision_status` to `accepted`, record the named license owner in the
     policy and in `LIC-004`, and close `LIC-004` if MISRA access is confirmed.
  3. Replace this record with `## Completion evidence`, set this task to
     `done`, set Task 004 to `ready`, and commit.
  4. If the maintainer replaces part of the model, revise the policy, matrix,
     and validator tests first, then repeat step 3.
- Decisions: pending. Asked on 2026-10-18 whether the maintainer accepts
  policy items 1–9 with the two-lane CI consequence, and who owns TI license
  acceptance and MISRA access, including whether MISRA C:2012 plus
  Amendments 1–3 access exists.
- Verification, 2026-10-18: `python3 ci/validate_licenses.py` validated 20
  artifacts and 8 blockers; `python3 -m unittest discover -s ci -p 'test_*.py'`
  passed 13 tests. The link check reached 81 of 85 URLs. The four git.ti.com
  URLs timed out from this workstation and two Kitware URLs returned the
  expected anti-bot 403.

## Depends on

None.

## Plan references

Phase 0 and Sections 6.5, 6.8, 6.11, and 6.12.

## Objective

Produce the authoritative legal/technical input matrix for every artifact that
may enter containers, CI caches, course images, or releases.

## Scope

- Research current official terms for TI PSDK/PDK, C6000/C7000 CGT, firmware,
  DSPLIB/MMALIB, the Beagle image, base containers, and MISRA materials.
- Classify download, installation, CI use, caching, student access, and binary
  redistribution separately.
- Design license-acceptance and credential handling that works without secrets
  in fork/PR jobs.
- Recommend which layers can be public and which require controlled access.

## Out of scope

Legal representation, accepting terms for the maintainer, or committing
proprietary installers.

## Maintainer checkpoint

`maintainer-decision`: approve the recommended distribution model and identify
any institutional license that must be obtained.

## Acceptance criteria

- Every planned non-project artifact has a source, version rule, license,
  redistribution classification, and CI handling decision.
- Unsupported assumptions are blockers rather than guessed permissions.
- The result supports a public monorepo without exposing restricted payloads.

## Verification and evidence

Store cited findings and the machine-readable license matrix in the repository;
cross-check it against planned container and release contents.
