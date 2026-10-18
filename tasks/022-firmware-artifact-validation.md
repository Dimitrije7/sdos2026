# 022 — Validate firmware artifacts and memory budgets

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: the ELF and resource-table validator exists and gates every firmware
  build; budgets, symbols, map checks, and JSON/JUnit reports are left.
- Done: `ci/firmware_elf.py` (machine, segment placement against
  `manifest/dsp-cores.json`, RPMsg vdev, trace, TI 64-bit trace entry) with
  synthetic negative fixtures in `ci/test_firmware_elf.py`; the same check in C
  guards `dspctl load` (`host/libsdos_dsp/src/elf_check.c`).
- Next: identity symbol and linker-map checks, memory and stack budgets per
  target, machine-readable reports, `make package`.
- Decisions: on 2026-10-18 the maintainer wrote "proceed with plan, but do everything except board tests, also postpone any decision making from my side because I will not be able to respond". This board-free work therefore started before its board-side dependencies were done; acceptance still needs them.
- Verification, 2026-10-19: `python3 -m unittest ci.test_firmware_elf` passed.

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
