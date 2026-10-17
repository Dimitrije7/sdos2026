# 022 — Validate firmware artifacts and memory budgets

## Status

pending

## Depends on

020 and 021.

## Plan references

Phase 3 and Sections 6.4 and 6.6.

## Objective

Reject malformed, oversized, mislinked, or contract-incompatible firmware
before it reaches a board.

## Scope

- Inspect ELF architecture, entry point, load segments, sections, symbols,
  resource table, firmware identity, and linker map.
- Validate every address and size against the authoritative memory map.
- Define flash/RAM/stack/trace budgets and regression thresholds per target.
- Emit machine-readable results and useful CI annotations.
- Add deliberately broken fixtures for each validator class.

## Out of scope

Proving runtime cache behavior or accepting a budget overrun by suppression.

## Maintainer checkpoint

Material budget changes require an architecture decision; normal threshold
tuning follows measured evidence.

## Acceptance criteria

- Every firmware matrix artifact passes the same deterministic validator.
- Wrong architecture, overlap, missing resource table/symbol, and budget
  overflow fixtures are rejected.
- Results identify the exact target and offending contract.

## Verification and evidence

Run positive artifacts and all negative fixtures in the container and CI;
archive representative JSON/JUnit reports.
