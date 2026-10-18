# 002 — Define the reference hardware and lab BOM

## Status

done

## Depends on

None.

## Plan references

Phase 0 and the hardware-related risks in Section 8.

## Objective

Define a complete, supportable reference bench and lab-fleet bill of materials
for the BeagleBone AI-64 without making the maintainer perform inventory work.

## Scope

- Verify board power, cooling, storage, network, and 3.3 V UART requirements
  from primary sources.
- Specify supported models/capabilities for power supply, active cooling,
  microSD, card reader, UART, Ethernet, and recovery media.
- Define acceptance checks and a machine-readable inventory format.
- Recommend spare ratios and quantities separately from the technical minimum.
- Record what can be discovered automatically when hardware is connected.

## Out of scope

Purchasing, flashing media, or claiming unconnected equipment is present.

## Maintainer checkpoint

Only procurement quantity or cost/scope tradeoffs require a decision. The agent
provides a recommended BOM first.

## Acceptance criteria

- A versioned BOM and inventory schema cover every Phase 0 hardware item.
- Electrical compatibility and cooling requirements have cited evidence.
- Later board tasks can consume the inventory without free-form re-entry.

## Verification and evidence

Validate the BOM fields and links; distinguish required, recommended, and spare
equipment.

## Completion evidence

- [`docs/hardware-bom.md`](../docs/hardware-bom.md) records the supported bench,
  quantity formulas, candidate models, and qualification boundaries.
- [`manifest/hardware-bom.json`](../manifest/hardware-bom.json) and the two
  schemas under [`manifest/schemas/`](../manifest/schemas/) provide the
  versioned machine contract for later tasks.
- [`ci/validate_hardware.py`](../ci/validate_hardware.py) validates BOM and
  inventory structure, cross-references, stable identities, acceptance results,
  readiness, quantity plans, and source URLs.
- [`ci/test_validate_hardware.py`](../ci/test_validate_hardware.py) exercises a
  complete synthetic inventory and the missing-identity, unsafe-reader,
  failed-check, unknown-check, and missing-category failure paths.
- [`docs/research/task-002-hardware-bom-evidence.md`](../docs/research/task-002-hardware-bom-evidence.md)
  holds the primary-source research and clearly separates published facts,
  course recommendations, and observations that require connected hardware.
- No physical hardware was observed or claimed. The example inventory remains
  empty and assigns connected discovery to Task 007.
