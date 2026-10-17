# 012 — Audit device tree and DSP memory reservations

## Status

pending

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
