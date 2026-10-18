# Task 002 reference-bench hardware evidence

Evidence checked: 2026-10-18

This note supports
[`tasks/002-reference-hardware-and-lab-bom.md`](../../tasks/002-reference-hardware-and-lab-bom.md).
It separates published hardware facts from course recommendations and from facts
that require a sample or site inventory. All external claims below cite the
manufacturer, standards body, upstream project, or board/SoC owner directly.

## Recommended reference bench

Use one complete working set per simultaneously occupied bench:

| Item | Technical minimum | Recommended course capability | Supported example or disposition |
|---|---|---|---|
| Board | BeagleBone AI-64 with its supplied heatsink | Asset-tagged board whose EEPROM identity and SoC revision have been captured | BeagleBoard BeagleBone AI-64. It carries a TDA4VM, 4 GB LPDDR4, 16 GB eMMC, microSD, two 3.3 V debug UARTs, and Gigabit Ethernet ([BeagleBoard specification](https://docs.beagleboard.org/boards/beaglebone/ai-64/01-introduction.html)). |
| Power | Regulated 5 V input able to sustain the board load; never feed a higher barrel-jack voltage | Dedicated regulated 5 V, 4 A or greater, center-positive barrel supply so the board, fan, and normal USB load do not depend on a host USB port | Adafruit product 1466 is a documented 5 V/4 A, 5.5 mm OD/2.1 mm ID, center-positive candidate ([manufacturer page](https://www.adafruit.com/product/1466)). Its physical fit on a fleet sample is still an acceptance check because the board documentation does not publish the barrel dimensions. |
| Power cable | Cable and connector rated for the selected 5 V current | Prefer the dedicated barrel supply. If USB-C is used, use a C-to-C supply/cable path that actually advertises 5 V/3 A; do not rely on USB-A-to-C for board power | BeagleBoard recommends C-to-C for tethering and says USB-A-to-C requires the barrel supply ([quick-start guide](https://docs.beagleboard.org/boards/beaglebone/ai-64/02-quick-start.html)). |
| Active cooling | Forced airflow over the supplied heatsink for all course workloads and benchmarks | 5 V, four-wire fan with tachometer and PWM, secured over or across the heatsink; record RPM or an explicit fan-health result and temperature | Noctua NF-A4x10 5V PWM is a documented 40 x 40 x 10 mm, 5 V, four-wire PWM/tach fan drawing at most 0.07 A; its OmniJoin adapter permits a custom harness ([manufacturer specification](https://www.noctua.at/en/products/nf-a4x10-5v-pwm/specifications)). Mechanical mounting and the board-side harness must be qualified before fleet purchase. |
| Working microSD | 32 GB or larger genuine microSDHC/microSDXC | 64 GB or larger, UHS-I, Class 10/U1, A1, high-endurance product from a traceable source | Kingston High-Endurance `SDCE/64GB` is a suitable example: 64 GB, UHS-I U1/Class 10/A1 and rated for 3,000 program/erase cycles ([manufacturer datasheet](https://www.kingston.com/datasheets/sdce_en.pdf)). |
| USB microSD reader | Linux-compatible reader that accepts microSDHC and microSDXC and exposes the inserted card as a removable block device | Single-slot USB 3 reader that passes the stable-identity acceptance test below | Kingston MobileLite Plus `MLPM` supports microSDHC/microSDXC, UHS-I/UHS-II, USB 3.2 Gen 1, and Linux ([manufacturer page](https://www.kingston.com/en/memory-card-readers/mobilelite-plus-microsd-reader)). The manufacturer does not promise a unique storage serial, so this is a transport-compatible example, not an approved destructive-write target until sampled. |
| USB-UART | USB-to-TTL adapter with 3.3 V UART signals, GND/TX/RX, 115200 8N1 operation, and no VCC connection to the board | Genuine adapter with a factory-unique USB serial number plus a keyed three-wire board harness | FTDI `TTL-232R-3V3` is the reference example: 3.3 V UART I/O, 3 Mbaud capability, a six-pin 0.1-inch termination, and a factory-programmed unique USB serial ([product page](https://ftdichip.com/products/ttl-232r-3v3/), [cable datasheet](https://ftdichip.com/wp-content/uploads/2023/07/DS_TTL-232R_CABLES.pdf)). It requires a separate, qualified board-header adapter. |
| Ethernet | RJ45 patch cable and a network port capable of at least 100 Mbit/s, with a route from the agent workstation to the board | Cat5e-or-better patch lead and a managed or documented Gigabit switch port; DHCP reservation or recorded address; SSH reachability | The board exposes 10/100/1000 Ethernet and an RJ45 link/speed indicator ([BeagleBoard specification](https://docs.beagleboard.org/boards/beaglebone/ai-64/01-introduction.html)). Cable/switch brand is not electrically significant after the link test, so no single model needs to be mandated. |
| Recovery media | One independently verified microSD image that boots with the board's boot button and can recover the selected course baseline | Dedicated, labeled, normally offline high-endurance card with recorded image SHA-256, write date, validation date, and compatible board/image version | Use the same supported card families as working media. Do not treat USB or UART boot as the course recovery path: BeagleBoard documents SD boot as the eMMC override/update route and says software for the other modes is not provided ([boot-mode documentation](https://docs.beagleboard.org/boards/beaglebone/ai-64/03-design-and-specifications.html)). |

The technical minimum is the lowest configuration the project should accept.
The recommendation is the uniform configuration to purchase and support. Product
examples demonstrate documented capabilities; they do not replace incoming
inspection or the acceptance checks in this note.

## Source-backed electrical and interface facts

### Power

BeagleBoard gives two slightly different statements. Its feature table and power
accessory table say 5 V at 3 A, while the detailed design and quick-start text say
5 V at **more than** 3 A and call that supply mandatory. The board accepts power
through USB-C, a barrel jack, or the cape power pins. Peripherals can increase the
required current, and USB power may prevent the Type-A ports from supplying their
full documented current ([board design](https://docs.beagleboard.org/boards/beaglebone/ai-64/03-design-and-specifications.html),
[BeagleBoard power-supply table](https://docs.beagleboard.org/accessories/power-supplies.html)).

The course should resolve that wording conservatively: 5 V/3 A is the bare-board
compatibility floor; a dedicated 5 V supply rated at 4 A or more is the reference
bench requirement. A higher current rating is available capacity, not forced
current. Voltage and polarity remain fixed at 5 V and center-positive. A supply
should fail incoming inspection if its label omits 5 V output, its plug fit is
loose, or the board resets or reports undervoltage under the qualification load.

USB-C-to-C is the only documented single-cable tethering candidate. BeagleBoard
warns that a PC often cannot source enough power, and states that USB-A-to-C
operation requires the external barrel supply ([quick-start guide](https://docs.beagleboard.org/boards/beaglebone/ai-64/02-quick-start.html)).
Therefore a data cable supplied for tethering must not be counted as the bench
power supply unless the exact host, supply, and cable pass the loaded test.

### Cooling and fan interface

The board is delivered with a heatsink and exposes a PWM-controllable four-pin fan
connector ([board component specification](https://docs.beagleboard.org/boards/beaglebone/ai-64/01-introduction.html)).
The official revision-B1 schematic identifies the fan header as a 1.25 mm,
four-pin header with pin 1 GND, pin 2 5 V, pin 3 tachometer, and pin 4 PWM
([BeagleBoard revision-B1 schematic](https://github.com/beagleboard/beaglebone-ai-64/blob/main/hw/BeagleBone_AI-64_Rev_B1_SCH_220602.pdf)).

Neither the reference manual nor schematic specifies a complete supported fan
assembly, heatsink mounting geometry for a fan, header current limit, or Linux fan
policy. Active cooling is therefore a **course qualification requirement** driven
by sustained DSP benchmarking and repeatability, not a claim that BeagleBoard
requires a fan for every use. Approve a fan assembly only after verifying connector
fit, polarity, startup at 5 V, tachometer observation, PWM response, secure mounting,
clearance, and the Gate 1/5 temperature envelope. Until the board-side harness is
qualified, the Noctua example may be powered with its documented USB adapter while
still providing forced airflow; that arrangement does not prove board-header PWM or
tachometer operation.

### Storage and endurance

BeagleBoard documents the microSD slot as a boot source that can override eMMC and
be used to update it, and says the connector supports larger-capacity cards
([board design](https://docs.beagleboard.org/boards/beaglebone/ai-64/03-design-and-specifications.html)).
The SD Association defines SDHC as more than 2 GB through 32 GB and SDXC as more
than 32 GB through 2 TB ([capacity standard overview](https://www.sdcard.org/developers/sd-standard-overview/capacity-sd-sdhc-sdxc-sduc/)).
This supports the plan's 32 GB minimum and a 64 GB SDXC recommendation.

A1 is relevant to a Linux root filesystem because it guarantees, under the SD
Association's specified conditions, at least 1,500 random-read IOPS, 500
random-write IOPS, and 10 MB/s sustained sequential write
([SD Association application-performance paper](https://www.sdcard.org/cms/wp-content/uploads/2020/11/Application_Peformance_Class_White_Paper.pdf)).
Class 10/U1 likewise denotes a 10 MB/s minimum sequential-write class
([SD Association speed classes](https://www.sdcard.org/developers/sd-standard-overview/speed-class/)).
Those marks measure performance, not write endurance. The recommended card must
also have a manufacturer endurance specification; a generic consumer card is not
equivalent merely because it is U1 or A1.

The exact minimum usable capacity cannot be derived from board hardware. Before
freezing a card SKU, the built course image must fit with room for two image update
staging areas, package caches, build output required on target, logs, crash data,
and at least 20% free space after a complete pilot. If a 32 GB card does not meet
that measured condition, raise the technical minimum rather than pruning recovery
or evidence data.

### Safe card-reader identification

Linux block names such as `/dev/sdX` depend on enumeration and are not stable.
`lsblk` reads sysfs and the udev database, advises scripts to name their output
columns explicitly, and recommends `udevadm settle` before inspecting newly added
devices ([upstream `lsblk` manual source](https://github.com/util-linux/util-linux/blob/master/lsblk-cmd/lsblk.8.adoc)).
Systemd's persistent-storage rule creates `/dev/disk/by-id` only when the device
supplies an `ID_SERIAL`; it separately creates a topology-dependent
`/dev/disk/by-path` link when a path is available
([upstream systemd rule](https://github.com/systemd/systemd/blob/main/rules.d/60-persistent-storage.rules.in)).

A supported reader sample must therefore pass all of these before it can be used
by automated flashing:

1. With no removable reader attached, capture the complete block inventory.
2. Attach one reader and one card, settle udev, and identify exactly one new whole
   removable disk with `TRAN=usb`, expected capacity, no mounts, and the expected
   USB vendor/product.
3. Require a nonempty, reader-specific USB serial and a unique `/dev/disk/by-id`
   link. Reject duplicate or placeholder serials across every sampled reader.
4. Record `/dev/disk/by-path` as a secondary physical-port check. Do not substitute
   it for the unique ID if benches can be recabled.
5. Perform a non-destructive read test, then a scratch-card write/read/hash test.
6. Store the accepted vendor, product, serial, by-id path, by-path path, capacity,
   and observed removable flag in inventory.

Kingston's `MLPM` documentation establishes card, USB, and Linux compatibility but
does not state that the product exports a unique USB or SCSI serial. Consequently,
neither it nor any other reader should be marked supported from a catalogue page
alone. If no affordable reader passes step 3, the project must design a documented
two-person or physical-port identification fallback before Task 007; it must not
silently weaken the exact-target rule.

### UART electrical and pin requirements

The board exposes two three-pin 3.3 V TTL debug ports: `WKUP_UART0` and `UART0`.
Only TX and RX are supported; hardware handshake is not. The official schematic
labels each header pin 1 GND, pin 2 RXD, and pin 3 TXD. Use `UART0` for the normal
MPU/Linux console unless the selected image documents otherwise. BeagleBoard's
default serial settings are 115200 baud, eight data bits, no parity, one stop bit,
and no handshake ([board design](https://docs.beagleboard.org/boards/beaglebone/ai-64/03-design-and-specifications.html),
[BeagleBoard cable guide](https://docs.beagleboard.org/accessories/cables.html)).

Connect adapter TX to board RX, adapter RX to board TX, and GND to GND. Leave the
adapter's VCC/power wire and RTS/CTS wires disconnected. The recommended FTDI cable
does expose 5 V on VCC even though its UART I/O signals are 3.3 V; the 5 V wire is
not a board-console signal ([FTDI product specification](https://ftdichip.com/products/ttl-232r-3v3/)).
A 5 V TTL adapter or true RS-232 adapter fails acceptance.

There is a primary-source connector conflict that must be resolved on a physical
board before buying harnesses. BeagleBoard's current cable guide calls the AI-64
debug connector a three-pin, 1.25 mm Molex PicoBlade. The official revision-B1
schematic instead names `B3B-ZR(LF)(SN)`, which JST documents as a 1.5 mm-pitch
part ([BeagleBoard cable guide](https://docs.beagleboard.org/accessories/cables.html),
[official schematic](https://github.com/beagleboard/beaglebone-ai-64/blob/main/hw/BeagleBone_AI-64_Rev_B1_SCH_220602.pdf),
[JST ZR datasheet](https://www.jst.com/wp-content/uploads/2025/06/eZR.pdf)).
Do not infer that visually similar 1.25 mm and 1.5 mm plugs are interchangeable.
Record the connector that fits each observed board revision and qualify a keyed,
strain-relieved three-wire adapter. This conflict does not change the verified
GND/RX/TX electrical pin order.

### Ethernet

The board has a Gigabit RJ45 interface; BeagleBoard says a standard 100 Mbit/s
Ethernet cable is sufficient for basic connectivity
([specification](https://docs.beagleboard.org/boards/beaglebone/ai-64/01-introduction.html),
[quick-start guide](https://docs.beagleboard.org/boards/beaglebone/ai-64/02-quick-start.html)).
The course should still provision Cat5e-or-better leads and Gigabit switch ports so
the physical link does not cap transfers or invalidate performance diagnosis.
Acceptance requires link carrier, an observed negotiated rate, a recorded MAC and
IP address, workstation-to-board reachability, SSH, and a reconnect test. DHCP,
VLAN, outbound Internet, proxy, and fixed-address policy are site facts rather than
properties of the board or cable.

### Recovery path

The default boot source is eMMC. Holding the boot button while removing and
reapplying power with a bootable microSD inserted selects SD boot; a warm reset does
not resample that choice. The same SD path can update eMMC
([BeagleBoard boot documentation](https://docs.beagleboard.org/boards/beaglebone/ai-64/03-design-and-specifications.html)).
This makes a known-good microSD the supported recovery medium.

Each recovery card needs a human-readable asset ID and a machine record containing
card manufacturer/model/capacity, image URL and SHA-256, image/build identity,
write timestamp, writer identity, verification hash/result, last boot-validation
date, compatible board revisions, and state (`sealed`, `checked-out`, `failed`, or
`retired`). Store the canonical compressed image and hash separately from the card.
A recovery card is not accepted merely because it was flashed: it must cold-boot a
reference board from SD and expose the expected image identity without modifying
eMMC.

## Quantity and spare policy

Let `N` be the maximum number of simultaneously occupied student benches, not the
course enrollment. Let `Q = max(2, ceil(0.10 * N))` and
`M = max(2, ceil(0.20 * N))`. Round upward and count already-qualified inventory
before purchasing.

| Category | Technical minimum for operation | Recommended deployment quantity | Spare stock |
|---|---:|---:|---:|
| BeagleBone AI-64 boards | `N` | `N` assigned, qualified boards | `Q` qualified boards |
| Dedicated 5 V supplies | `N` | `N` assigned supplies | `Q` supplies |
| Active-cooling assemblies | `N` | `N` installed and qualified | `Q` fan/harness assemblies |
| Working microSD cards | `N` | `N` assigned cards | `M` blank, qualified cards |
| Ethernet patch leads | `N` | `N` assigned leads | `Q` leads |
| Switch ports | `N` reachable ports | `N + Q` available ports when switch capacity permits | Capacity, not loose hardware, is the spare |
| USB card readers | `1` per lab/provisioning station | `ceil(N / 4)` for concurrent recovery/provisioning, minimum 2 | 1 accepted reader |
| USB-UART sets | `1` per lab | `ceil(N / 4)` for concurrent diagnosis, minimum 2 | 1 complete adapter plus board harness |
| Golden recovery cards | `1` per lab | `ceil(N / 4)` ready-to-boot cards, minimum 2, plus one canonical offline image/hash | 1 additional sealed card |

Ten percent is appropriate for relatively durable, higher-cost boards, supplies,
fans, and cables; twenty percent is recommended for frequently rewritten removable
media. The minimum of two avoids a single spare becoming a fleet-wide bottleneck.
Readers, UART sets, and recovery cards are shared technician equipment at a 1:4
ratio; change that ratio only from measured lab turnaround and concurrency. These
are service-level recommendations, not electrical requirements.

## Automatic discovery and inventory fields

The board contains a 4 Kbit EEPROM whose published contents include board name,
serial number, and revision ([BeagleBoard design documentation](https://docs.beagleboard.org/boards/beaglebone/ai-64/03-design-and-specifications.html)).
Linux also defines SoC attributes under `/sys/devices/socX`, including `machine`,
`family`, `serial_number`, `soc_id`, and `revision`
([Linux sysfs SoC ABI](https://www.kernel.org/doc/html/latest/admin-guide/abi-testing-files.html#abi-file-testing-sysfs-devices-soc)).
TI documents TDA4VM/J721E silicon revisions 1.0, 1.1, and 2.0, so a schematic's
printed revision must not be substituted for the live SoC value
([TI TDA4VM datasheet](https://www.ti.com/lit/ds/symlink/tda4vm.pdf)).

A connected-board collector can therefore attempt to populate:

- board EEPROM name, serial, and revision;
- device-tree model and compatible strings;
- SoC `machine`, `family`, `soc_id`, `revision`, and `serial_number` when exported;
- Ethernet permanent/current MAC addresses and negotiated link information;
- eMMC and microSD model, capacity, CID/serial fields exposed by the kernel;
- USB-UART vendor/product/serial and stable device link;
- running bootloader, kernel, DTB, OS image, and firmware package identities; and
- a site-assigned immutable asset ID that joins discovery output to the inventory.

Board EEPROM and SoC fields are distinct: board revision identifies the PCB/assembly,
while SoC revision identifies the TDA4VM silicon. Missing sysfs attributes must be
recorded as unavailable, not replaced by assumed values. The later inventory schema
should store both raw observed values and normalized values so new board/kernel
revisions do not require free-form re-entry.

## Facts that require the actual cohort or site

Primary documentation cannot establish any of the following:

- `N`, room count, lab scheduling overlap, enrollment, group size, or turnaround
  time, which determine quantities and whether the recommended 1:4 shared ratio is
  adequate;
- how many boards, supplies, fans, cards, readers, UART adapters, recovery cards,
  cables, and switch ports already exist and pass acceptance;
- observed board PCB revisions, EEPROM serials, TDA4VM silicon revisions, damaged
  connectors, missing heatsinks, or counterfeit/remapped media;
- which barrel plug fits the fleet and which debug connector family is fitted to
  each board revision;
- whether a candidate card reader exposes a unique serial and stable Linux by-id
  path, or whether every FTDI cable is genuine and uniquely serialized;
- ambient temperature, bench/enclosure airflow, fan mounting, noise constraints,
  and the temperature/frequency envelope reached by sustained three-DSP workloads;
- mains plug/socket type, electrical-safety procurement rules, approved vendors,
  prices, lead times, taxes, and purchasing agreements;
- switch model, free ports, DHCP/DNS/VLAN/firewall/proxy policy, SSH reachability,
  Internet availability, or whether board MAC reservations are permitted; and
- the final image's expanded size, target-side free-space budget, measured card
  wear/failure rate, recovery-time objective, and simultaneous recovery demand.

These are inventory observations or maintainer procurement choices. They should be
collected by the agent when equipment is connected; the maintainer should only need
to provide physical access and decide a documented cost/scope tradeoff.

## Factual conflicts and cautions for Task 002

1. The plan says **5 V / at least 3 A**. Current BeagleBoard prose says both 3 A
   and more than 3 A. A 5 V/4 A-or-greater dedicated reference supply resolves the
   discrepancy without changing the board voltage.
2. The BeagleBoard cable guide says the debug connector is 1.25 mm PicoBlade; the
   official revision-B1 schematic says JST `B3B-ZR`, a 1.5 mm part. Harness
   procurement is blocked on a physical fit check by board revision.
3. No reviewed card-reader manufacturer page promises a unique Linux-visible
   storage serial. Reader model approval is blocked on sample `udevadm`/by-id
   evidence, although the cited Kingston model meets transport and Linux-support
   requirements.
4. The board publishes a fan pinout but not a supported fan assembly, mounting
   method, header-current limit, or Linux policy. The fan model is a qualified
   candidate, and fleet support depends on the temperature, tachometer, PWM,
   connector, and mounting acceptance tests.
5. Hardware documentation cannot prove any item is present at the site. Do not mark
   inventory quantities, revisions, or acceptance states from this research note.
