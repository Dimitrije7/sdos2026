# 012 — Audit device tree and DSP memory reservations

## Status

done

## Handoff

- Updated: 2026-10-19, claude.
- State: done.
- Done:
  - `live.dtb` captured and archived; [`ci/dt_memory.py`](../ci/dt_memory.py)
    extracts its memory facts to JSON (`dt-memory.json`) and checks them
    against [`manifest/memory-map.json`](../manifest/memory-map.json), the
    owner, reason, and source of every reservation, and against
    `manifest/dsp-cores.json`: carve-outs, pools, local memories, MSMC, and
    shared DDR. It runs in `make quality`.
    [`ci/test_dt_memory.py`](../ci/test_dt_memory.py) covers the parser and
    deliberate overlap, alignment, range, ownership, and mismatch failures.
  - Findings, in [`docs/memory-map.md`](../docs/memory-map.md): 0xAA000000
    is used at run time by the device-manager firmware on MCU R5F 0, though
    no device-tree node names it. MSMC beyond ATF's 128 KiB has no owner. C71x
    L2 is 512 KiB in the device tree; the contract keeps TI's 448 KiB and marks
    it not loadable.
- Next: none. A partition of MSMC per core is a design choice for Task 042.
- Decisions: none.
- Verification, 2026-10-19: `python3 ci/dt_memory.py` passes on the
  committed extraction; `ci.test_dt_memory` passes 14 tests.

## Depends on

004 and 009.

## Plan references

Phase 2, Section 3 design rules, and the memory-map risks in Section 8.

## Objective

Prove that Linux, firmware, resource tables, and the selected TI SDK agree on
all DSP-visible memory before loading course firmware.

## Scope

- Capture/decompile the effective device tree and enumerate reserved memory,
  CMA, OP-TEE, DMA pools, and other remote-core allocations.
- Reconcile it with the pinned PDK memory map and expected linker regions.
- Define one machine-readable authoritative memory-map representation.
- Add overlap, alignment, range, and size validation suitable for CI.
- Document intentional gaps and ownership/cache attributes.

## Out of scope

Applying a device-tree/kernel patch before the audit demonstrates it is needed.

## Maintainer checkpoint

`hardware-ready` for effective device-tree capture. A material repartitioning
of memory is a later architecture decision.

## Acceptance criteria

- Every reserved region has an owner and source reference.
- Automated validation finds no unintended overlap or out-of-range address.
- Linux, linker, and resource-table consumers can be checked from one contract.

## Verification and evidence

Run validators on the captured map plus deliberate overlap/alignment failures;
archive the map, diagrams, and audit report.
