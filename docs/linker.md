# DSP memory layout and linker files

You control where every part of the DSP firmware lives. Each firmware build
links with a native TI linker command file. You may edit its `MEMORY` and
`SECTIONS` directly:

- region origins and lengths, and fixed addresses;
- section order, grouping, and alignment;
- linker symbols;
- separate load and run addresses, and overlays.

There is no placement API or configuration language in between. The only limits
are the ones listed under [Mandatory rules](#mandatory-rules), each with a
hardware, loader, ownership, or interoperability reason, and the
[protection rules](#protection-rules) the run-time memory protection relies
on. The validators enforce them, whatever your layout looks like.

This replaces the earlier course policy that reserved linker layout to the
course runtime: policy P-002 was amended on 2026-10-19
([Task 041](../tasks/041-dsp-linker-control.md)).

## Files

| File | Role |
|---|---|
| [`firmware/linker/<core>/SDOS.cmd`](../firmware/linker/c66x_0/SDOS.cmd) | Main entry point per core: `MEMORY`, `SECTIONS`, stack and heap sizes |
| [`firmware/linker/common/c66x_runtime.cmd`](../firmware/linker/common/c66x_runtime.cmd), [`c71x_runtime.cmd`](../firmware/linker/common/c71x_runtime.cmd) | Sections each runtime adds, included inside `SECTIONS` |
| [`firmware/linker/modules/*_memory.cmd`](../firmware/linker/modules/) | One fragment per module, for example [`fir_memory.cmd`](../firmware/linker/modules/fir_memory.cmd) and [`buffers_memory.cmd`](../firmware/linker/modules/buffers_memory.cmd) |
| `build/<preset>/generated/linker/sdos_memory.cmd` | Generated from [`manifest/dsp-cores.json`](../manifest/dsp-cores.json): region origins, lengths, and usable sizes for the preset's core and runtime |
| [`firmware/linker/examples/`](../firmware/linker/examples/) | Complete alternative layouts |

Each default `SDOS.cmd` defines the carve-out split as `#define`d boundaries
(`CODE_ORIGIN`, `RODATA_ORIGIN`, `DATA_ORIGIN`) and builds both its `MEMORY`
regions and the protection zones from them, so moving a boundary moves the
protection with it. The regions are `DDR_META` (resource table, trace, and
integrity record), `DDR_CODE`, `DDR_RODATA` (constants and initialization
tables), `DDR_DATA`, `L2CODE` (code that runs from L2, such as the FIR copy),
and `L2SRAM`. On the C66x the default places read-only data at +1 MiB (2 MiB)
and code at +3 MiB (4 MiB), naturally aligned, so each takes one MPAX
segment.

`SDOS.cmd` serves both runtimes of its core. The build defines
`SDOS_RTOS_FREERTOS` or `SDOS_RTOS_BAREMETAL` for `#if` blocks. The fragments
are a convention: reorganize, merge, or replace them, or write one file of your
own. To use another file, set the `SDOS_LINKER_SCRIPT` CMake variable, as the
example presets do:

```sh
make build PRESET=c66x_0-baremetal-release-l2   # firmware/linker/examples/c66x_0_l2.cmd
make build PRESET=c71x_0-freertos-release-ddr   # firmware/linker/examples/c71x_0_ddr.cmd
```

## Addresses: the generated macros

Hardware facts are never typed into the linker files. `sdos_memory.cmd` defines
these macros for each region of the core:

| Macro | Meaning |
|---|---|
| `SDOS_<REGION>_ORIGIN` | First address of the region, in the DSP's own address view |
| `SDOS_<REGION>_LENGTH` | Size of the region in hardware |
| `SDOS_<REGION>_USABLE` | `LENGTH` minus what this build's runtime configures as cache |

A region ends at `ORIGIN + USABLE - 1`. Write `MEMORY` lengths as differences
of these macros, as the defaults do, so a layout stays correct when the
manifest changes. The linker map and the report print every region's first and
last address.

## Memory each DSP can use

| Region | C66x_0 / C66x_1 | C71x_0 | Load with remoteproc | Use |
|---|---|---|---|---|
| `ddr`, the firmware carve-out | `0xa6100000` / `0xa7100000`, 15 MiB | `0xa8100000`, 15 MiB | yes | anything |
| `l2sram`, local L2 SRAM | `0x00800000`, 288 KiB; FreeRTOS keeps the top 64 KiB as cache, bare metal none | `0x64800000`, 448 KiB | yes | anything |
| `l1dsram`, `l1psram` | `0x00f00000`, `0x00e00000`, 32 KiB each, all cache in both runtimes | — | — | after changing the cache configuration |
| `dma_pool` | `0xa7000000` / `0xa6000000`, 1 MiB | `0xa8000000`, 1 MiB | no | none |
| `msmc`, shared on-chip SRAM | `0x70020000`, 8000 KiB | same | no | after the Task 012 audit, at run time only |
| `shared_ddr` | `0xaa000000`, 28 MiB | same | no | after the Task 012 audit, at run time only |

**Why each limit exists:**

- **`dma_pool`:** Linux allocates the core's vrings and RPMsg buffers there
  when it starts the core.
- **Cache reserves:** the runtime turns that part of the SRAM into cache. On
  the C66x under FreeRTOS, the 64 KiB is compiled into TI's prebuilt kernel
  library (`portCONFIGURE_CACHE_L2_SIZE`); using all of L2 there requires
  rebuilding that library with a smaller setting.
- **`msmc` and `shared_ddr`:** remoteproc cannot load into them, because they
  are not in the DSP's device-tree memory regions. Other processors may also
  own parts of them. ARM Trusted Firmware and the DMSC firmware hold the first
  128 KiB and the last 64 KiB of MSMC, which are already excluded above. Until
  the live device tree is audited (Task 012), the validator refuses them.
  Afterwards, firmware may use them for run-time data, placed `NOLOAD` or
  copied in at start.

Linux's reservations themselves are fixed: changing them means changing the
device tree, which is outside this scope. Each region's full reason and its
source are in the manifest.

## Mandatory rules

| Rule | Reason |
|---|---|
| `.resource_table` in loadable memory | remoteproc finds the table by this section name and reads it before starting the core |
| `.tracebuf` in loadable memory | Linux reads the trace through remoteproc, which maps only loadable memory |
| Entry point 1 KiB-aligned (C66x), 2 MiB-aligned (C71x) | The K3 remoteproc driver writes the entry point into the boot vector register, which takes only aligned addresses |
| `.hwi_vect` or `.csl_vect` 1 KiB-aligned (C66x) | The interrupt vector table is installed through ISTP |
| `.neardata`, `.rodata`, `.bss` grouped (C66x) | Near data is addressed relative to the data page pointer within one window |
| `.vecs` 4 MiB- and `.secure_vecs` 2 MiB-aligned (C71x) | TI's validated layout for the C7x vector base registers; kept until proven unnecessary |
| `.stack` 128 KiB-aligned, task stacks 8 KiB-aligned (C71x) | TI's C7x interrupt handling and FreeRTOS port require it |
| C71x MMU tables `NOINIT` | They are written before C initialization runs |
| Segments within loadable regions, no two overlapping | remoteproc copies every loadable segment to its address |
| Nothing loaded into C71x L2, not even zero-filled data | The J721E C71x has no local reset: Linux loads it while the module is in reset, and a write into L2 then hangs the SoC (board, Task 021). L2 holds NOLOAD data and start-up copies only |
| C66x entry `sdos_entry` (`-e sdos_entry`), first in the code zone | The reset entry discards the caches the powered C66x subsystem kept from the previous run before TI's `_c_int00` runs |

The alignments leave holes. On the C71x, a code region must be large enough for
three aligned slots: the entry point and both vector tables. The linker reports
an alignment failure otherwise, as `c71x_0_ddr.cmd` notes.

## Protection rules

The C66x MPAX and the C71x MMU map memory by zones the linker files define
([`safety.md`](safety.md#memory-partition)). Lines marked PROTECTION in the
default files serve them:

| Rule | Reason |
|---|---|
| `sdos_zone_code_start/end` and `sdos_zone_rodata_start/end` defined, 4 KiB-aligned, in loadable memory | The MPAX and MMU map these ranges read-only |
| `sdos_zone_l2code_start/end` on the 16 KiB L2 protection page (4 KiB on the C71x), and `sdos_zone_l2_end` at the end of usable L2 | After start the L2 code zone is read-only and the rest of L2 is not executable |
| Executable sections loaded and run only in the code zones | Everything else is mapped without execute permission |
| Nothing writable in a code or read-only data zone | Writes there fault |
| `.sdos.integrity` in loaded memory | The firmware checks its image against this record |
| `-heap 0`, no `malloc()` | Static allocation only |
| C71x: guarded stacks (`.stack`, `.sdos.stack.*`) on 4 KiB pages; the MMU tables in a page-padded group | The lowest page of each stack, and the tables, become read-only after start |
| C71x: the map within TI's 16 page tables; C66x: the device windows and zones within 14 MPAX segments | TI's MMU code has no tables to spare; the MPAX has 16 segments, and segments 0 and 1 deny everything not granted |

[`ci/firmware_postlink.py`](../ci/firmware_postlink.py) checks every rule after
linking and names the one a layout breaks. The full list, with what each
protects, is in [`safety.md`](safety.md#rules-for-your-own-layout).

## Load and run addresses

Give a section `load = <region>, run = <region>, table(BINIT)` and the linker
builds a copy table. The TI runtime processes it in `_c_int00`, before `main()`,
on both families. The default
[`fir_memory.cmd`](../firmware/linker/modules/fir_memory.cmd) does this: the FIR
code is loaded into DDR and runs from L2 SRAM. `LOAD_START`, `RUN_START`, and
`RUN_SIZE` define symbols for the copy. At start, `sdos_layout_check()` runs the
FIR from its run address and writes the addresses and the result to the trace:

```text
[0.000] layout: fir code loaded at 0xa6218720, runs at 0x80c440, 384 bytes; check passed
```

For copies at other times, name a table with `table(my_table)` and call
`copy_in(&my_table)` from `<cpy_tbl.h>`.

## Overlays and NOINIT

`UNION` places sections at the same run address. The default
[`buffers_memory.cmd`](../firmware/linker/modules/buffers_memory.cmd) overlays
two scratch areas this way. Overlays share run addresses, never load images, so
the validator accepts them. Overlapping loadable segments are always an error.

`type = NOINIT` data is neither loaded nor zeroed. Its contents after a firmware
load, a core reset, or power loss are undefined; it does not persist.
`type = NOLOAD` sections are not loaded either; use it for buffers that code
initializes itself.

## Placing your own code and data

Put an object in a named section, then place the section:

```c
static const int16_t taps[64] __attribute__((section(".sdos.coeffs"))) = { /* ... */ };
```

```text
.sdos.coeffs : {} > L2SRAM, align = 0x80
```

Whole object files and library members can be placed by name too:
`*libsdos_fir_q15.a<*>(.text)` selects the FIR library's code.

## Checks and reports

Every link treats linker warnings as errors. Afterwards it runs these steps,
from [`cmake/SdosTiFirmware.cmake`](../cmake/SdosTiFirmware.cmake):

1. [`ci/firmware_postlink.py`](../ci/firmware_postlink.py) checks the
   [protection rules](#protection-rules) on the linked ELF and writes the
   integrity record into it.
2. `strip6x` or `strip7x` writes the loadable file.
3. [`ci/firmware_elf.py`](../ci/firmware_elf.py) checks it against the
   manifest for the build's runtime. The check covers:
   - region ownership and cache reserves;
   - overlap;
   - entry-point alignment;
   - the resource table, RPMsg vdev, and trace placement;
   - the integrity record against the file, and no segment both writable and
     executable.

   Each refusal names the region and its reason.
4. [`ci/linker_report.py`](../ci/linker_report.py) writes
   `<name>.layout.txt` and `.layout.json` beside the firmware. These list every
   region with its first and last address and its use, and every output
   section with its run and load address, size, and region.

`dspctl load` runs the same check on the board, without knowing the runtime.
It therefore allows the smaller of the runtimes' cache reserves.
[`ci/test_linker_layouts.py`](../ci/test_linker_layouts.py) relinks real
variants to prove that valid layouts build, and that these are refused:

- a section in the vring pool or in FreeRTOS's L2 cache;
- a misaligned entry point or zone;
- writable data in the read-only zone, or code outside the code zone;
- zones that need more MPAX segments than the firmware has.

## Not yet proven on a board

All of the above is checked at build time. That the default and example
layouts load, initialize, run, and restart on the DSPs is checked in the next
board session (Task 041), and the ownership of MSMC and shared DDR is audited
(Task 012).
