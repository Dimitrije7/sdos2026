# 024 — Implement quality, security, and MISRA-oriented gates

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: YAML and Markdown linting and cppcheck are enforced; the
  vulnerability policy, MISRA-oriented checks, and REUSE are left.
- Done: `.yamllint.yaml` and `.markdownlint-cli2.yaml` (each rule change has
  its reason in the file; vendored skills are excluded), both added to
  `make quality`; the 11 real findings in `docs/` were fixed.
  - Cppcheck 2.22.0 built from the manifest's commit in a Dockerfile stage
    (`toolchain/Dockerfile`, stage `cppcheck`), run by `make quality` over
    the portable C (`ci/facade.py` `CPPCHECK`). Its first run found an
    unchecked open in a test, two dead struct fields, and argument-order
    clarity issues; all fixed, and it reports nothing now. Code that
    includes TI headers is left to the TI compiler with warnings as errors.
- Next:
  1. Vulnerability policy for Trivy and Dependabot findings carried from Tasks
     017 and 018 (braces, smol-toml, katex in markdownlint's dependencies;
     libssl3 after the snapshot).
  2. REUSE (sdist).
  3. Remove MISRA from the policy and the scope (decision below): P-005 in
     `docs/course-policy.md`, the MISRA rows elsewhere in the docs, and this
     task's scope and title. Keep the rule against claiming MISRA compliance.
- Decisions: on 2026-10-18 the maintainer wrote "proceed with plan, but do everything except board tests, also postpone any decision making from my side because I will not be able to respond". On 2026-10-19 the maintainer decided to drop MISRA: no MISRA subset and no MISRA-oriented checks; Cppcheck and the compiler warnings stay.
- Verification, 2026-10-19: `yamllint --strict` and `markdownlint-cli2` passed
  in the container (0 issues in 60 Markdown files).

## Depends on

018, 021, and 023.

## Plan references

Sections 6.4, 6.7, 6.8, and 6.11.

## Objective

Turn the accepted coding and supply-chain policy into reproducible local and PR
checks with truthful compliance language.

## Scope

- Configure formatting, warnings-as-errors, clang-tidy, Clang Static Analyzer,
  Cppcheck, the named MISRA subset, and complexity thresholds.
- Add shell, CMake, YAML, Markdown, Dockerfile, and Actions linting.
- Add gitleaks, dependency/container scanning, REUSE/SPDX checks, action pin
  enforcement, `actionlint`, and `zizmor`.
- Separate vendor/generated findings and implement reviewed deviations with
  owner, rationale, scope, and expiry.
- Make `make quality` reproduce all applicable PR quality gates.

## Out of scope

Claiming formal MISRA compliance, embedding proprietary MISRA rule text, or
broad baseline suppressions.

## Maintainer checkpoint

`maintainer-decision` only if implementation evidence requires changing the
MISRA subset or accepted deviation policy from Task 001.

## Acceptance criteria

- Each configured gate detects at least one deliberate violation.
- Local and GitHub results agree and identify actionable source locations.
- Tool versions, checked rules, exclusions, and deviations are documented.

## Verification and evidence

Run `make quality`, negative fixtures, secret scans, and workflow/container
security checks; archive machine-readable reports.
