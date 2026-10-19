# Freedom from interference on the DSPs

The course firmware applies the practices that automotive software uses to
keep one software element from corrupting another. The method follows
ISO 26262-6 Annex D, which sorts interference into three classes: memory,
timing and execution, and exchange of information. Each DSP partitions its
memory in hardware, supervises its own stacks, image, and time, protects every
message end to end, and moves into a defined safe state when a check fails.
Linux can see that state even when the firmware no longer answers.

[Task 042](../tasks/042-freedom-from-interference.md) introduced this
on 2026-10-19.

## What this is, and is not

**This is not a safety-certified system and makes no ASIL claim.** The
BeagleBone AI-64 is not a safety-qualified board. Linux on the A72 counts as
QM, and TI's FreeRTOS port, IPC library, and PDK are not safety-qualified
products. The project has not qualified its compilers. What the project does
claim is narrower: every mechanism below is implemented, checked in every
build, and proven on the board by deliberate fault injection. Where the
hardware or the fixed Linux configuration prevents a mechanism, this page
says so and gives the reason.

## Mechanisms

| ID | Mechanism | Where | Reaction |
|---|---|---|---|
| SM-1 | **C71x MMU partition.** Only the core's own memory and the device windows are mapped. Code is read-only, data is never executable, and Linux's and the other cores' memory faults. | [`c71x.c`](../firmware/c71x/src/c71x.c) | Exception, then halt with a fault record |
| SM-2 | **C66x MPAX partition.** DDR outside the carve-out and vring pool is denied. The code zone is read-only and executable, and the constant zone is read-only. Nothing in DDR is executable except the code zone. | [`c66x.c`](../firmware/c66x/src/c66x.c) | The access is blocked; the monitor reads the fault registers and enters the safe state |
| SM-3 | **Protection self-test.** At start, the C66x reads the other C66x's memory and requires the MPAX to block and record it. | `c66x.c` | Safe state with fault `self-test` |
| SM-4 | **Read-only page tables.** After start, the C71x maps its own MMU tables read-only. | `c71x.c` | Exception on any write |
| SM-5 | **Stack guards.** Each guarded stack is filled with 0xA5. On the C71x its lowest page becomes read-only after start; on both families the monitor requires its lowest 256 bytes to keep the fill. Peak use is reported. | [`monitor.c`](../firmware/common/platform/src/monitor.c) | C71x: exception and halt, fault `stack`. C66x: safe state, fault `stack` |
| SM-6 | **Image integrity.** After linking, [`firmware_postlink.py`](../ci/firmware_postlink.py) writes a CRC-32C record of every read-only loaded segment into the ELF. The firmware checks the whole image at start, then 16 KiB per monitor period. This covers code, constants, and load/run copies such as the FIR in L2. | `monitor.c`, [`safety.c`](../firmware/common/safety/src/safety.c) | Safe state, fault `integrity` |
| SM-7 | **No C heap.** Builds link with `-heap 0`, and the post-link step refuses `malloc()` and its relatives. FreeRTOS's own heap may change only during start-up. | linker files, `monitor.c` | Build failure; at run time, safe state with fault `heap` |
| SM-8 | **Write xor execute.** No loaded segment is both writable and executable. Code lies only in the code zone or local L2, and nothing writable lies in a read-only zone. | `firmware_postlink.py`, [`firmware_elf.py`](../ci/firmware_elf.py) | Build failure |
| SM-9 | **RUN time budget.** Every RUN carries a budget in microseconds, at most the 2 s watchdog limit, and the runtime measures it in cycles. | [`runtime.c`](../firmware/common/runtime/src/runtime.c) | `deadline missed`; the late output is discarded |
| SM-10 | **Watchdog.** Under FreeRTOS, a monitor task above every other task faults a RUN that runs past 2 s. | `runtime.c`, FreeRTOS `main.c` | Safe state, fault `watchdog` |
| SM-11 | **Heartbeat and health line.** The monitor advances a heartbeat every 20 ms and rewrites a fixed health line at the start of the trace buffer. Linux reads it through remoteproc, without IPC. | `monitor.c`, [`trace.c`](../firmware/common/platform/src/trace.c) | Read by `dspctl health` and `supervise` |
| SM-12 | **Supervisor.** `dspctl supervise` restarts a core whose heartbeat stops or that reports a fault, after saving its trace. | [`dspctl`](dspctl.md) | Restart of that core alone |
| SM-13 | **End-to-end message protection.** A CRC-32C covers every message header and payload. Request ids are matched, lengths and descriptors are checked, and sample data has its own CRC. | [`protocol.c`](../firmware/common/protocol/src/protocol.c) | `message corrupted`, counted |
| SM-14 | **Sender check.** The firmware serves only Linux on the A72. | [`ipc_service.c`](../firmware/common/platform/src/ipc_service.c) | Message dropped, counted |
| SM-15 | **Start-up checks.** The integrity pass, the protection self-test, and the layout check all pass before RUN is accepted. Until then RUN answers `busy`. | `main.c`, `monitor.c` | Safe state on failure |

## Interference analysis

### Memory

| Fault | Mechanism | Status |
|---|---|---|
| DSP code writes another core's or Linux's memory | SM-1, SM-2 | C71x: prevented. C66x: prevented in DDR. MSMC and the other cores' local memories are reachable on the C66x through MPAX segment 0, see [residual risks](#residual-risks) |
| DSP code corrupts its own code or constants | SM-1, SM-2, SM-6 | Prevented in DDR. Local L2 is checked by SM-6 only |
| Executing data | SM-1, SM-2, SM-8 | Prevented in DDR on both families. On the C66x, local L2 stays executable |
| Stack overflow | SM-5 | C71x: trapped at the guard page. C66x: detected afterwards by the fill check |
| Heap exhaustion or fragmentation | SM-7 | No C heap; the kernel heap is frozen after start |
| Corruption of the protection itself | SM-4, SM-6, SM-3 | The C71x page tables are read-only and the C66x MPAX is self-tested. Code changes are found by the CRC check |
| Another master (Linux, DMA, the other cores) writes this core's memory | SM-6 detects changes to code and constants | Not prevented: that needs SoC firewalls, see [residual risks](#residual-risks) |

### Timing and execution

| Fault | Mechanism | Status |
|---|---|---|
| An algorithm runs too long | SM-9 | Detected at completion; the output is discarded |
| An algorithm never returns | SM-10, SM-11, SM-12 | FreeRTOS: watchdog fault within 2 s. Bare metal: the heartbeat stops and the supervisor restarts the core |
| The firmware stops serving | SM-11, SM-12 | Restart by the supervisor |
| Starvation of the monitor | The FreeRTOS monitor has the second-highest priority, below only the kernel timer task | Bare metal runs the monitor between requests only |

### Exchange of information

| Fault | Mechanism |
|---|---|
| Corruption | SM-13 (message CRC), the descriptor CRC over sample data, and the output CRC |
| Repetition, insertion, incorrect sequence | Request ids are matched by the host; stale responses are counted and dropped |
| Loss and delay | Host timeouts; a timed-out buffer is quarantined until a restart ([`protocol.md`](protocol.md#buffer-ownership-and-caches)) |
| Masquerade, incorrect addressing | SM-14; a fixed endpoint and service name |
| Blocking access to the channel | The DSP refuses a second RUN with `busy`; a restart closes the endpoint |

## Memory partition

Defaults, in each core's own address view; the linker files may move every
boundary, and the protection follows
([Rules for your own layout](#rules-for-your-own-layout)).

| Zone | C66x (MPAX) | C71x (MMU) |
|---|---|---|
| Code zone, `DDR_CODE` | read, execute | read, execute, cached |
| Read-only data, `DDR_RODATA` | read | read, cached |
| Rest of the carve-out: metadata, data, stacks | read, write | read, write, cached |
| Vring pool | read, write | read, write, uncached |
| Local L2 SRAM | not covered by MPAX: read, write, execute | read, write, execute, device type as TI maps it |
| Device windows, below 0x80000000 | read, write, no execute (segment 0) | read, write, no execute, device memory, the five windows in [`dsp-cores.json`](../manifest/dsp-cores.json) |
| Any other DDR: Linux, the other cores | denied | unmapped |

## Fault codes

STATUS and the health line report the first fault; later ones are dropped
because they are usually consequences. A restart clears it.

| Code | Name | Raised when | Address | Detail |
|---|---|---|---|---|
| 1 | integrity | a read-only range differs from its CRC | start of the range | range index; bit 31 set: the record itself is missing or damaged |
| 2 | stack | a stack reached its guard | base of the stack | index of the stack |
| 3 | protection | the MPAX or MMU could not be configured | | 1: MPAX segment 1 does not cover the carve-out; 2: too many segments; other values count failed MMU mappings |
| 4 | access | the protection blocked an access | the faulting address | C66x: XMPFSR; C71x: IERR and IESR |
| 5 | exception | the C71x raised another exception | the faulting instruction | IERR and IESR |
| 6 | watchdog | a RUN ran past 2 s (FreeRTOS) | | algorithm id |
| 7 | layout | the load/run copy check failed | | |
| 8 | self-test | the MPAX self-test did not see its blocked read | XMPFAR | XMPFSR |
| 9 | heap | the kernel heap changed after start | | bytes allocated |
| 10 | ipc | Sciclient or RPMsg could not start | | 1: Sciclient; 2: RPMsg |

## States

| State | Meaning | RUN | HELLO, STATUS |
|---|---|---|---|
| starting | start-up checks are running | `busy` | answered |
| ok | all checks passed | served | answered |
| faulted | the safe state after a fault | `firmware faulted` | answered |
| halted | a fatal fault stopped the core | no answer | no answer; the health line remains |

The C71x halts on every MMU fault and CPU exception, because the core cannot
safely continue after one. The C66x reports MPAX violations without stopping,
since the hardware blocked the access, so it can stay in `faulted`.

## Health line

The first 192 bytes of the trace buffer, rewritten every monitor period:

```text
SDOS-HEALTH 1 seq=0000002a hb=0000002a up=00003a98 state=1 fault=00 info=00000000 addr=0000000000000000 margin=00001e40 passes=00000011 late=00000000 end=0000002a
```

All fields are hexadecimal. `seq` and `end` are written first and last; a
reader that sees them differ read a line being rewritten and reads again.
`dspctl health` decodes it; [`protocol.h`](../firmware/common/protocol/include/sdos/protocol.h)
has the C parser both sides share.

## Rules for your own layout

The linker files stay yours to edit ([`linker.md`](linker.md)). The protection
needs these from any layout, and the post-link step refuses a build that
breaks one, naming it:

- Define `sdos_zone_code_start/end` and `sdos_zone_rodata_start/end` on 4 KiB
  boundaries inside loadable memory. The default files derive them from
  `#define`d region boundaries.
- Place executable sections only in the code zone or in local L2.
- Place nothing writable in the code or read-only data zone. TI's C71x port
  puts two writable pointers in `.const` sections, which
  [`c71x_runtime.cmd`](../firmware/linker/common/c71x_runtime.cmd) places with
  the data.
- Place `.sdos.integrity` in loaded memory.
- Do not link `malloc()`. Allocate statically.
- On the C71x, keep guarded stacks on 4 KiB pages. Keep the MMU map within
  TI's 16 page tables: each zone boundary that is not on a 2 MiB boundary
  costs one table.
- On the C66x, the zones must fit 14 MPAX segments. Boundaries on larger
  powers of two need fewer.

## Fault injection

The `*-fi` presets build test firmware with extra algorithms, each of which
commits one fault. They are built in every quality run and never shipped in
the image. `python3 -m tests.target.sdos_board fault-injection --out DIR` runs
them on a board. For each case it reads the health line, classifies what the
firmware reported, and restarts the core.

| Case | C66x | C71x |
|---|---|---|
| write into the other C66x's memory | access | access, halted |
| read Linux's memory | access | access, halted |
| write into the code zone | access | access, halted |
| execute from the data zone | access, exception, or stopped heartbeat | access or exception, halted |
| recurse into the stack guard | stack | stack, halted |
| never return | FreeRTOS: watchdog; bare metal: stopped heartbeat | the same |
| run 5 ms on a 1 ms budget | deadline missed | deadline missed |
| change read-only data the CRC record covers | integrity | integrity |
| allocate from the kernel heap after start | FreeRTOS: heap; bare metal: not applicable | the same |

## Residual risks

Each of these needs hardware, a decision, or both:

- **Other bus masters.** Linux, DMA engines, and the other cores can still
  write a DSP's memory. Blocking them needs the SoC firewalls, which TI's
  boot firmware configures. Changing them changes the boot chain, so it is a
  maintainer decision. SM-6 detects changes to code and constants.
- **Hardware watchdog.** The RTI watchdogs report through the ESM, which can
  reset the whole SoC. Routing one per DSP is a maintainer decision. The
  software watchdog and the Linux supervisor cover a stuck core.
- **C66x local memory and MPAX segment 0.** The MPAX guards only accesses that
  leave the core. L2 SRAM is not covered: its page-protection registers are
  not yet configured. Segment 0, which keeps everything below 0x80000000
  readable and writable, still reaches MSMC and other cores' local memories.
  Narrowing it needs the peripheral list that Task 012's board survey
  provides.
- **C71x device windows.** They keep TI's wide windows, now without execute
  permission. Narrowing them to the peripherals the drivers use needs the same
  survey.
- **Unguarded stacks.** These are FreeRTOS's idle and timer task stacks and
  the C71x interrupt stack, all inside TI's prebuilt kernel. FreeRTOS's own
  overflow check, method 2, covers the task stacks at each context switch.
- **Kernel heap.** TI's FreeRTOS kernel allocates during start-up. The heap
  cannot be removed without rebuilding that library; SM-7 freezes it instead.
- **Static analysis.** MISRA C:2012 checking awaits the subset decision in
  Task 024. A free MISRA-oriented check is not MISRA compliance.

## Verification status

The following runs in every `make quality` and in CI:

- the unit tests for the protocol, runtime, and safety code;
- the mock and CLI tests;
- all firmware builds, with the post-link rules and the integrity check;
- the real-linker negative tests in
  [`test_linker_layouts.py`](../ci/test_linker_layouts.py).

The following waits for the board session in Task 042:

- that the MPAX and MMU maps boot on the hardware;
- that every fault-injection case is detected;
- that every core recovers after its fault.

Evidence goes to `release/evidence/task-042/`.
