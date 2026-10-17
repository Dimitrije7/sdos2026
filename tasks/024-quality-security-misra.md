# 024 — Implement quality, security, and MISRA-oriented gates

## Status

pending

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
