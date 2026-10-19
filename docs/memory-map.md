# Board memory map

Every memory reservation on the course board, who owns it, and how the
repository keeps it true. The authoritative data is in two manifests:

- [`manifest/memory-map.json`](../manifest/memory-map.json): every
  reserved-memory node and on-chip SRAM of the live device tree, with its owner,
  reason, and source.
- [`manifest/dsp-cores.json`](../manifest/dsp-cores.json): each DSP's regions
  (carve-out, vring pool, local SRAM, MSMC, shared DDR) and device windows. The
  linker definitions, the C core table, and the ELF validators are generated
  from or checked against it ([linker.md](linker.md)).

[`ci/dt_memory.py`](../ci/dt_memory.py) ties them to the board. `extract`
turns a captured `live.dtb` into JSON; `check` (run by `make quality` against
the committed extraction) refuses an unowned or moved reservation, overlap, a
region outside DDR or not page-aligned, a DSP whose carve-out or pool is not its
device-tree `memory-region`, a local memory larger than the hardware, and MSMC
use that touches the firmware carve-out.

## DDR, 0x9E800000 to 0xABBFFFFF

Captured on 2026-10-19 from BeagleBoard.org Debian 13 (kernel
6.18.55-arm64-k3-r59), [Task 012 evidence](../release/evidence/task-012/).

```text
0x9E800000  OP-TEE                                24 MiB
0xA0000000  MCU R5F 0   pool 1 MiB | firmware 15 MiB   (device manager, running)
0xA1000000  MCU R5F 1   pool 1 MiB | firmware 15 MiB
0xA2000000  MAIN R5F 0  pool 1 MiB | firmware 15 MiB
0xA3000000  MAIN R5F 1  pool 1 MiB | firmware 15 MiB
0xA4000000  MAIN R5F 2  pool 1 MiB | firmware 15 MiB
0xA5000000  MAIN R5F 3  pool 1 MiB | firmware 15 MiB
0xA6000000  C66x_1 pool 1 MiB | C66x_0 firmware 15 MiB   (TI swaps the
0xA7000000  C66x_0 pool 1 MiB | C66x_1 firmware 15 MiB    two C66x pools)
0xA8000000  C71x_0 pool 1 MiB | C71x_0 firmware 15 MiB
0xA9000000  Linux RAM                             16 MiB
0xAA000000  inter-RTOS IPC, used by the device manager   28 MiB
```

Each pool is the first `memory-region` of its core (vrings and RPMsg buffers,
allocated by Linux); the firmware carve-out is the second, and the only DDR
remoteproc loads into.

## Findings

- The DSP identities are fixed by device-tree unit address: `4d80800000.dsp`
  (C66x_0), `4d81800000.dsp` (C66x_1), `64800000.dsp` (C71x_0). `remoteprocN`
  numbering held across reboots but is not relied on.
- **0xAA000000 is not free.** No device-tree node references it, but the
  device-manager firmware on MCU R5F 0 (TI's IPC echo test with Sciserver)
  uses it for RTOS-to-RTOS vrings. TI's DSP echo firmware messaged that core
  through it until the device manager's IPC heap ran out and TI-SCI device
  management stopped answering for the whole SoC; only a reboot recovered it
  ([Task 013 evidence](../release/evidence/task-013/)). Course firmware
  never uses it, and every DSP's protection denies it ([safety.md](safety.md)).
- **MSMC** (0x70000000, 8 MiB minus 64 KiB): ARM Trusted Firmware holds the
  first 128 KiB; Linux's `mmio-sram` driver binds the rest with no consumer.
  No core owns 0x70020000 to 0x707EFFFF, and every DSP's protection denies it
  until a partition gives each core its own slice.
- **C71x L2 cannot be loaded.** The device tree gives 512 KiB; TI's C7x
  linker files use 448 KiB, which the contract keeps. The J721E C71x has no
  local reset, so Linux loads the ELF while the module is held in reset; an
  A72 write into its L2 then gets no response and hangs the SoC (seen on the
  board, Task 021). The contract marks it not loadable, so the validator
  refuses any C71x segment there, zero-filled ones included; firmware fills L2
  at start-up instead.
- C66x L2 (288 KiB), L1P, and L1D (32 KiB each) match the device tree.
