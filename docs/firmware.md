# Course firmware

Every DSP runs the same course runtime, which answers [protocol 2.0](protocol.md)
requests from Linux. It is built for 3 cores × 2 runtimes × 2 build types,
from one command model. Two more presets build example layouts, and six
`*-fi` presets build fault-injection test firmware
([`safety.md`](safety.md#fault-injection)):

```sh
make build PRESET=firmware                  # all 20
make build PRESET=c66x_0-freertos-release   # one
```

The firmware partitions its memory in hardware and supervises itself; see
[`safety.md`](safety.md).

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
| All 20 builds | Build with warnings as errors; each passes the protection rules, the integrity record, and the carve-out and resource-table check |
| HELLO, ECHO, STATUS, RUN | Implemented in the common runtime and tested on the PC through the mock backend |
| On a board | Not run yet; the next board session runs it after TI's own echo firmware (Task 013) |
| Shared buffers | `region_size` is 0 until the Task 027 data-path decision; RUN answers `bad descriptor` |
| Fault record and health line | Implemented ([`safety.md`](safety.md)); board proof in Task 042 |
| Memory protection | C66x MPAX and C71x MMU partitions implemented; board proof in Task 042 |

## Layout

| Path | Contents | Shared by |
|---|---|---|
| `firmware/common/protocol/` | Message encoding, validation, buffer ownership, CRC-32C | DSPs and Linux |
| `firmware/common/runtime/` | Request dispatch and the built-in algorithms (`copy`, `gain_q15`, and `fir_q15` from `algorithms/`) | DSPs and the Linux mock |
| `firmware/common/safety/` | Stack margins, the integrity record and its check, MMU and MPAX planning | DSPs and the PC tests |
| `firmware/common/platform/` | IPC service, resource table, trace buffer and health line, identity, monitor, fault injection | All six DSP targets |
| `firmware/c66x/` | C66x cache attributes, MPAX partition and self-test, cache maintenance, cycle counter | Both C66x cores |
| `firmware/c71x/` | C71x least-privilege MMU map, guard pages, exception hook, cache maintenance, cycle counter | The C71x core |
| `firmware/runtime/freertos/` | `main()` with the request task and the monitor task | FreeRTOS targets |
| `firmware/runtime/baremetal/` | `main()` with a polling loop, and the mailbox poll | Bare-metal targets |

Common behavior has one source. The core adapters contain only what differs
between the C66x and the C71x.

## What comes from TI

Nothing TI ships is copied into the repository. The build reads the following
from the pinned PSDK that `make bootstrap TI=accept` installs:

- headers;
- prebuilt PDK libraries: IPC, OSAL, CSL, Sciclient, and the FreeRTOS kernel.

The linker command files are the course's own ([`linker.md`](linker.md)).

[`cmake/SdosTiFirmware.cmake`](../cmake/SdosTiFirmware.cmake) links in the
same order as TI's own build of the `ipc_echo_test_freertos` example; the
order was captured from TI's make with `-n`.

The IPC bring-up follows TI's BSD-3-Clause examples, with one deliberate
change. The course firmware talks only to Linux on the A72 and never waits for
the R5F cores, which may run no firmware on a BeagleBone AI-64.

## Memory

Each core's regions, their owners, and their limits are in
[`manifest/dsp-cores.json`](../manifest/dsp-cores.json). Placement is in
editable TI linker files under [`firmware/linker/`](../firmware/linker/):
[`linker.md`](linker.md) explains the files, the memory each DSP can use, the
mandatory rules and their reasons, load/run copies, overlays, and the reports.

TI swaps the two C66x IPC pools on purpose. The firmware fails to compile if
TI's memory map disagrees with the manifest. After linking, every build checks
the result against the manifest. The same check runs before `dspctl load` and
before the board procedures upload anything.

## Runtimes

| | FreeRTOS | Bare metal |
|---|---|---|
| `main()` | Starts the request task, which starts the monitor task | Runs the service loop directly |
| Monitor | A task above every course and IPC task, every 20 ms | Called from the loop between requests |
| Message arrival | The request task polls the mailbox and sleeps one tick between polls | The loop polls the mailbox; on the C71x it idles until a 1 ms wake tick between polls |
| Watchdog | The monitor task halts a RUN past 2 s | C71x: the wake tick halts it. C66x: none; the heartbeat stops |
| TI libraries | FreeRTOS OSAL and kernel, `ipc_baremetal` | C66x: bare-metal OSAL and `ipc_baremetal`. C71x: FreeRTOS OSAL and port for startup only, and `ipc_baremetal` |

Both runtimes link TI's bare-metal IPC library and poll the mailbox. TI's RTOS
IPC library routes a mailbox interrupt through the device manager, which this
board's boot configuration refuses to the C66x, and TI's bare-metal OSAL cannot
register C7x interrupts at all (its C7x `HwiP` code is a TODO in PDK 11.02).
The poll walks TI's private mailbox table, whose layout
[`mailbox_poll.c`](../firmware/common/platform/src/mailbox_poll.c) mirrors from
PDK 11.02.01.08; recheck it whenever the PDK pin changes. The C71x boot code
needs interrupt and exception startup that only TI's FreeRTOS port provides,
so bare-metal C71x links those objects and never starts the scheduler.

### Start and stop on this board

The board session of 2026-10-19 (Task 021) found that a core's surroundings
outlive it, and the firmware now handles each case:

- **The C71x cannot be loaded into its own L2.** It has no local reset, so
  Linux loads the ELF while the module is held in reset, and a write into L2
  then hangs the SoC. The manifest marks C71x L2 not loadable; firmware fills
  it at start-up (`table(BINIT)`, NOLOAD).
- **A busy C71x cannot be powered off.** Linux stops a C71x by switching it
  off, which completes only while the CPU idles. Bare metal therefore idles
  between polls on a 1 ms wake tick, and every halt idles, outside exception
  context. The C66x is stopped by local reset, busy or not.
- **The C66x subsystem stays powered.** Its caches survive a stop, so the
  reset entry [`entry.c`](../firmware/c66x/src/entry.c) discards them before
  TI's C runtime runs. Its interrupt router keeps the previous run's tick
  route, which the device manager then refuses to set again or release; the
  firmware replaces TI's `OS_init` on the C66x, requests the route, and
  accepts a refusal only when the route is verifiably in place.
- **The DM timers stay powered** and keep counting with their interrupt
  pending, so every start resets its own timer, and a graceful shutdown
  resets it too.
- **Sciclient** runs polled: in interrupt mode every C66x request after the
  first timed out.
- **A C71x task stack** holds wide vector registers: the monitor task needs
  64 KiB, not the 16 KiB first given.

## Trace

The resource table points Linux at a 64 KiB trace buffer, which
`dspctl trace <dsp>` prints. Its first 192 bytes are the health line
([`safety.md`](safety.md#health-line)); a text ring follows, in which each
line starts with the uptime. The C66x uses
the standard trace entry on both families. TI's C71x IPC type ends with a
64-bit trace entry, which the course kernel ignores ("unsupported resource
65538"), so the C71x table declares the standard one; `firmware_elf.py`
refuses a firmware whose only trace is the 64-bit entry.

## Known limits

- The C71x stack-overflow case is contained but not reported, and needs a
  board reboot ([safety.md](safety.md#residual-risks)).
- Polling adds up to one tick (1 ms) of latency per request under FreeRTOS;
  bare-metal polling cost has not been measured.
