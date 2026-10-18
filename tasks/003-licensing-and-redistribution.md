# 003 — Audit licensing and redistribution

## Status

done

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

## Completion evidence

- [`docs/licensing-policy.md`](../docs/licensing-policy.md) is the accepted
  distribution policy, with its decision record.
- [`manifest/license-matrix.json`](../manifest/license-matrix.json) revision 2
  records 20 artifacts with source, version rule, license, ten per-operation
  decisions, CI handling, and cost. Seven named blockers cover every
  unresolved permission.
- [`ci/validate_licenses.py`](../ci/validate_licenses.py) enforces the
  default-deny and no-cost invariants;
  [`ci/test_validate_licenses.py`](../ci/test_validate_licenses.py) covers
  their failure paths.
- [`docs/research/task-003-licensing-evidence.md`](../docs/research/task-003-licensing-evidence.md)
  holds the primary-source findings and the cross-check against every
  third-party input named in the bring-up plan.
- Decision, 2026-10-18: asked whether the maintainer accepts policy items 1–9
  with the two-lane CI consequence, and who owns TI license acceptance and
  MISRA access. The maintainer answered: "1. I accept it 2. We own no
  licenses, everything must be free and/or open source". Applied as policy
  item 10, matrix revision 2, the excluded MISRA row, and the closure of
  `LIC-004`.
- Verification, 2026-10-18: `python3 ci/validate_licenses.py` validated
  revision 2; `python3 -m unittest discover -s ci -p 'test_*.py'` passed. The
  link check reached 81 of 85 URLs; four git.ti.com URLs timed out from this
  workstation.
- Work by Codex (research, matrix, validator) and Claude (reconciliation,
  cross-check, decision application).
