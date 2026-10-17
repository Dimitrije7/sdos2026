# 003 — Audit licensing and redistribution

## Status

ready

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
