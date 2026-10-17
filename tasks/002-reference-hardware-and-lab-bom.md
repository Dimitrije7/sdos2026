# 002 — Define the reference hardware and lab BOM

## Status

ready

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
