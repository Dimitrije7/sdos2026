# Course firmware

Every DSP runs the same course runtime, which answers [protocol 1.0](protocol.md)
requests from Linux. It is built for 3 cores × 2 runtimes × 2 build types,
from one command model:

```sh
make build PRESET=firmware                  # all 12
make build PRESET=c66x_0-freertos-release   # one
```

Each preset writes `build/<preset>/firmware/sdos-<core>-<runtime>.xe66`
(`.xe71` on the C71x). That file is stripped, and it is what boards and the
image use; three clean builds of the same source gave identical bytes. The
linked ELF with debug data is kept beside it as `<name>.debug.xe66`, with the
map file. Load one on the board with
[`dspctl`](dspctl.md):

```sh
dspctl stop c66x_0
dspctl load c66x_0 sdos-c66x_0-freertos.xe66
dspctl start c66x_0
dspctl echo c66x_0 hello
```

## Status

| Part | State |
|---|---|
| All 12 builds | Build with warnings as errors; each passes the carve-out and resource-table check |
| HELLO, ECHO, STATUS, RUN | Implemented in the common runtime and tested on the PC through the mock backend |
| On a board | Not run yet; the next board session runs it after TI's own echo firmware (Task 013) |
| Shared buffers | `region_size` is 0 until the Task 027 data-path decision; RUN answers `bad descriptor` |
| Crash record | Not implemented yet (Task 021) |

## Layout

| Path | Contents | Shared by |
|---|---|---|
| `firmware/common/protocol/` | Message encoding, validation, buffer ownership, CRC-32C | DSPs and Linux |
| `firmware/common/runtime/` | Request dispatch and the built-in algorithms (`copy`, `gain_q15`, and `fir_q15` from `algorithms/`) | DSPs and the Linux mock |
| `firmware/common/platform/` | IPC service, resource table, trace buffer, identity | All six DSP targets |
| `firmware/c66x/` | C66x cache attributes, cache maintenance, cycle counter | Both C66x cores |
| `firmware/c71x/` | C71x MMU map, cache maintenance, cycle counter | The C71x core |
| `firmware/runtime/freertos/` | `main()` with one FreeRTOS task | FreeRTOS targets |
| `firmware/runtime/baremetal/` | `main()` with a polling loop, and the mailbox poll | Bare-metal targets |

Common behavior has one source. The core adapters contain only what differs
between the C66x and the C71x.

## What comes from TI

Nothing TI ships is copied into the repository. The build reads the following
from the pinned PSDK that `make bootstrap TI=accept` installs:

- headers;
- prebuilt PDK libraries: IPC, OSAL, CSL, Sciclient, and the FreeRTOS kernel;
- TI's per-core linker command files.

[`cmake/SdosTiFirmware.cmake`](../cmake/SdosTiFirmware.cmake) links in the
same order as TI's own build of the `ipc_echo_test_freertos` example; the
order was captured from TI's make with `-n`.

The IPC bring-up follows TI's BSD-3-Clause examples, with one deliberate
change. The course firmware talks only to Linux on the A72 and never waits for
the R5F cores, which may run no firmware on a BeagleBone AI-64.

## Memory

Each core's carve-outs come from
[`manifest/dsp-cores.json`](../manifest/dsp-cores.json):

| Core | IPC pool (vrings) | Firmware carve-out |
|---|---|---|
| C66x_0 | `0xa7000000`, 1 MiB | `0xa6100000`, 15 MiB |
| C66x_1 | `0xa6000000`, 1 MiB | `0xa7100000`, 15 MiB |
| C71x_0 | `0xa8000000`, 1 MiB | `0xa8100000`, 15 MiB |

TI swaps the two C66x IPC pools on purpose. The firmware fails to compile if
TI's memory map disagrees with the manifest. After linking, every build runs
`ci/firmware_elf.py`, which refuses any loadable segment outside the core's
carve-out. The same check runs before `dspctl load` and before the Task 013
board procedure uploads anything. It has already caught one real defect: the
C71x linker file has no rule for `.tracebuf`, so the linker placed the trace
buffer in local L2 SRAM. The C71x build now keeps it in `.bss`.

## Runtimes

| | FreeRTOS | Bare metal |
|---|---|---|
| `main()` | Starts one task, then the scheduler | Runs the service loop directly |
| Message arrival | Mailbox interrupt; the task blocks in `RPMessage_recv` | The loop polls the mailbox with interrupts off |
| TI libraries | FreeRTOS OSAL and kernel, `ipc` | C66x: bare-metal OSAL and `ipc_baremetal`. C71x: FreeRTOS OSAL and port for startup only, and `ipc_baremetal` |

Bare metal polls on both DSP families. TI's bare-metal OSAL cannot register
C7x interrupts in PDK 11.02 (its C7x `HwiP` code is a TODO), and polling keeps
the C66x and the C71x behaving the same. The poll walks TI's private mailbox
table, whose layout
[`mailbox_poll.c`](../firmware/runtime/baremetal/mailbox_poll.c) mirrors from
PDK 11.02.01.08; recheck it whenever the PDK pin changes. The C71x boot code
needs interrupt and exception startup that only TI's FreeRTOS port provides,
so bare-metal C71x links those objects and never starts the scheduler.

## Trace

The resource table points Linux at a 64 KiB text ring, which
`dspctl trace <dsp>` prints. Each line starts with the uptime. The C66x uses
the standard trace entry. The C71x uses TI's 64-bit trace entry, as TI's own
C71x firmware does. A kernel without TI's support for that entry ignores it,
and `dspctl trace c71x_0` then has nothing to read. The next board session
shows which case applies.

## Known risks, checked in the next board session

- TI's prebuilt echo firmware waits for every remote core, including R5Fs that
  may have no firmware on this board. If it stalls, the course firmware does
  not have that dependency.
- The kernel's RPMsg character driver must accept the endpoint the library
  creates with `RPMSG_CREATE_EPT_IOCTL`.
- Bare-metal timing and polling cost have not been measured.
