# SDOS 2026 reference hardware and lab BOM

Status: recommended Phase 0 baseline  
Task: [002 — Define the reference hardware and lab BOM](../tasks/002-reference-hardware-and-lab-bom.md)  
Machine source: [`manifest/hardware-bom.json`](../manifest/hardware-bom.json)  
Evidence: [`docs/research/task-002-hardware-bom-evidence.md`](research/task-002-hardware-bom-evidence.md)

This document defines the supported reference bench and the inventory contract.
It does not claim that any hardware is present. Actual assets enter the inventory
only after an agent observes their identity and acceptance results.

## Reference bench

Each simultaneously occupied station needs:

| Category | Technical minimum | Recommended baseline |
|---|---|---|
| Board | One BeagleBone AI-64 with supplied heatsink | Asset-tagged unit with board EEPROM and TDA4VM silicon revisions recorded separately |
| Power | Dedicated regulated 5 V/4 A-or-greater supply, center-positive | Qualified barrel supply; one per station |
| Cooling | Secure forced airflow across the supplied heatsink | Qualified 5 V four-wire PWM/tachometer fan assembly; one per station |
| Working storage | Genuine, traceable microSD of at least 32 GB and Class 10/U1 | 64 GB high-endurance microSDXC, A1 or better, with at least 20% free space after the final image |
| Reader | One Linux-compatible removable-media reader per lab | One accepted single-slot USB 3 reader per four stations, minimum two |
| UART | One 3.3 V USB-TTL set per lab | One uniquely identified adapter and keyed three-wire harness per four stations, minimum two |
| Ethernet | One reachable 100BASE-T-or-better link per station | Cat5e-or-better lead and documented Gigabit switch port per station |
| Recovery | One hash-verified cold-bootable recovery microSD per lab | One normally offline golden card per four stations, minimum two, plus a separately retained canonical image and hash |

The BOM names concrete candidates where product-specific documentation supports
their capability:

- BeagleBoard.org BeagleBone AI-64;
- Adafruit 1466 regulated 5 V/4 A supply;
- Noctua NF-A4x10 5V PWM fan;
- Kingston `SDCE/64GB` high-endurance microSD;
- Kingston MobileLite Plus `MLPM` reader; and
- FTDI `TTL-232R-3V3` serial cable plus a separately qualified board harness.

An example is accepted for fleet use only after every check under that BOM item
passes. This distinction matters for the power connector fit, fan assembly,
reader identity, and UART harness, which cannot be qualified from catalogue data
alone. Ethernet uses a capability contract because cable and switch brands add no
useful compatibility guarantee after the physical-link and network checks pass.

## Quantities

Let `N` be the maximum number of simultaneously occupied stations. Quantities are
calculated from the versioned BOM rather than copied into another purchasing list.

- Boards, supplies, cooling assemblies, and Ethernet station links: `N` working
  units, plus `max(2, ceil(0.10 * N))` spares.
- Working microSD cards: `N` working units, plus
  `max(2, ceil(0.20 * N))` spares.
- Readers, UART sets, and recovery cards: technical minimum one per lab;
  recommended `max(2, ceil(N / 4))`, plus one spare.

For example, the validator calculates this 20-station plan:

| Item | Technical | Recommended | Spares |
|---|---:|---:|---:|
| Boards | 20 | 20 | 2 |
| Power supplies | 20 | 20 | 2 |
| Cooling assemblies | 20 | 20 | 2 |
| Working microSD cards | 20 | 20 | 4 |
| Readers | 1 | 5 | 1 |
| UART sets | 1 | 5 | 1 |
| Ethernet station links | 20 | 20 | 2 |
| Recovery cards | 1 | 5 | 1 |

The ratios are a service-level recommendation. Cohort size, existing qualified
inventory, purchasing rules, prices, and lab concurrency remain site facts. A
procurement decision may change recommended quantities, but it must not reduce an
electrical requirement or the technical minimum.

## Qualification boundaries

The primary-source review found four issues that require sample or site evidence:

1. BeagleBoard material states both 5 V/3 A and 5 V/>3 A. The baseline therefore
   uses a dedicated 5 V/4 A-or-greater supply and requires a loaded stability test.
2. The cable guide names a 1.25 mm PicoBlade UART connector, while the official
   revision-B1 schematic names a 1.5 mm JST ZR part. Harnesses must be physically
   qualified against each observed board revision. Only GND, crossed TX/RX, and
   3.3 V signaling are accepted; VCC, RTS, and CTS remain disconnected.
3. The reviewed reader documentation does not promise a unique Linux-visible
   serial. A candidate reader must expose a non-placeholder unique serial and
   stable `/dev/disk/by-id` identity before automated flashing can use it.
4. The board publishes a four-pin fan interface, but no complete supported fan
   assembly, mounting method, header-current limit, or final Linux policy. The
   fan and harness must pass electrical, mechanical, startup, control, and thermal
   checks before fleet purchase.

These conditions are acceptance work, not evidence that the hardware is absent or
incompatible. The complete claim-to-source record is in the research note linked
above.

## Inventory contract

[`manifest/schemas/hardware-inventory.schema.json`](../manifest/schemas/hardware-inventory.schema.json)
defines station and asset records for later connected-board tasks. A present asset
records its BOM item, category, assigned station where applicable, manufacturer,
model, condition, observation time, stable identification method and value,
observed capabilities, and named acceptance-check results. Structured `unresolved`
entries carry the owner and exact unblock condition for missing observations.

[`manifest/hardware-inventory.example.json`](../manifest/hardware-inventory.example.json)
is deliberately empty. It states that no hardware was observed during Task 002 and
does not invent serials, revisions, quantities, or test results.

The validator checks both schemas and their instances, requires all eight BOM
categories exactly once, rejects duplicate IDs and mismatched inventory
cross-references, and calculates quantities. Readiness mode additionally requires:

- exactly the planned number of ready stations;
- no unresolved inventory facts;
- every counted asset to have a recorded stable identity;
- every BOM acceptance check for that asset to be `true`; and
- every station-level item to be assigned to each ready station.

Run the portable validation and quantity calculation with:

```sh
python3 ci/validate_hardware.py --stations 20
```

Network link checking is explicit because it depends on external sites:

```sh
python3 ci/validate_hardware.py --check-links
```

Later tasks populate an observed inventory through read-only discovery first.
Writing a card still requires the exact block path, stable identity, model,
capacity, mount state, and explicit approval for that destructive operation.
