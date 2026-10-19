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
| SM-1 | **C71x MMU partition.** Only the core's own memory and the device windows its drivers use are mapped. Code is read-only, data is never executable, and Linux's and the other cores' memory, MSMC, and every other peripheral fault. After start the L2 code zone is read-only and the rest of L2 is not executable. | [`c71x.c`](../firmware/c71x/src/c71x.c) | Exception, then halt with a fault record |
| SM-2 | **C66x MPAX partition.** Only the core's own DDR, its vring pool, and the device windows its drivers use are writable; everything else, above and below 2 GiB, MSMC included, is denied. The code zone is read-only and executable, the constant zone read-only, and nothing else executable. On this SoC the XMC refuses and records writes outside the grants but lets reads through (board, 2026-10-19): it prevents corruption, not reading. | [`c66x.c`](../firmware/c66x/src/c66x.c) | The write is blocked; the monitor reads the fault registers and enters the safe state |
| SM-2a | **C66x L2 page protection.** Every 16 KiB page of usable L2 gets explicit attributes: the L2 code zone read-only and executable, the rest writable and never executable. No other bus master (the A72, the other DSPs, DMA) may access this core's L2 at all. | `c66x.c` | The access is blocked and recorded; the monitor enters the safe state, naming the address and the master |
| SM-3 | **Protection self-tests.** At start, the C66x writes into MSMC and into its own L2 code zone, each writing back the value it read; both must be refused and recorded. A read of the other C66x's memory is traced for the record. | `c66x.c` | A refusal that does not happen withholds the protection capability in HELLO; the board suites check it |
| SM-4 | **Read-only page tables.** After start, the C71x maps its own MMU tables read-only. | `c71x.c` | Exception on any write |
| SM-5 | **Stack guards.** Each guarded stack is filled with 0xA5. On the C71x its lowest page becomes read-only after start, and exceptions run on a 16 KiB stack of their own: the firmware installs its own vector table ([`vectors.asm`](../firmware/c71x/src/vectors.asm)) whose exception entries switch stacks before storing anything, so an overflow into the guard can be recorded. On both families the monitor requires the lowest 256 bytes to keep the fill. Peak use is reported. | [`monitor.c`](../firmware/common/platform/src/monitor.c), `c71x.c` | Safe state, fault `stack`; on the C71x the core halts and `dspctl recover` restarts it |
| SM-6 | **Image integrity.** After linking, [`firmware_postlink.py`](../ci/firmware_postlink.py) writes a CRC-32C record of every read-only loaded segment into the ELF. The firmware checks the whole image at start, then 16 KiB per monitor period. This covers code, constants, and load/run copies such as the FIR in L2. | `monitor.c`, [`safety.c`](../firmware/common/safety/src/safety.c) | Safe state, fault `integrity` |
| SM-7 | **No C heap.** Builds link with `-heap 0`, and the post-link step refuses `malloc()` and its relatives. FreeRTOS's own heap may change only during start-up. | linker files, `monitor.c` | Build failure; at run time, safe state with fault `heap` |
| SM-8 | **Write xor execute.** No loaded segment is both writable and executable. Code lies only in the code zone or local L2, and nothing writable lies in a read-only zone. | `firmware_postlink.py`, [`firmware_elf.py`](../ci/firmware_elf.py) | Build failure |
| SM-9 | **RUN time budget.** Every RUN carries a budget in microseconds, at most the 2 s watchdog limit, and the runtime measures it in cycles. | [`runtime.c`](../firmware/common/runtime/src/runtime.c) | `deadline missed`; the late output is discarded |
| SM-10 | **Watchdog.** A RUN past 2 s halts the core. Under FreeRTOS the monitor task, above every other task, checks it; on the bare-metal C71x the 1 ms wake tick does, from its interrupt. The bare-metal C66x has no interrupt; its heartbeat stops and SM-12 restarts it. | `runtime.c`, `monitor.c`, `c71x.c` | Halt with fault `watchdog`; Linux recovers the core |
| SM-11 | **Heartbeat and health line.** The monitor advances a heartbeat every 20 ms and rewrites a fixed health line at the start of the trace buffer. Linux reads it through remoteproc, without IPC. | `monitor.c`, [`trace.c`](../firmware/common/platform/src/trace.c) | Read by `dspctl health` and `supervise` |
| SM-12 | **Supervisor.** `dspctl supervise` restarts a core whose heartbeat stops or that reports a fault, after saving its trace. | [`dspctl`](dspctl.md) | Restart of that core alone |
| SM-13 | **End-to-end message protection.** A CRC-32C covers every message header and payload. Request ids are matched, lengths and descriptors are checked, and sample data has its own CRC. | [`protocol.c`](../firmware/common/protocol/src/protocol.c) | `message corrupted`, counted |
| SM-14 | **Sender check.** The firmware serves only Linux on the A72. | [`ipc_service.c`](../firmware/common/platform/src/ipc_service.c) | Message dropped, counted |
| SM-15 | **Start-up checks.** The integrity pass, the protection self-test, and the layout check all pass before RUN is accepted. Until then RUN answers `busy`. | `main.c`, `monitor.c` | Safe state on failure |
| SM-17 | **Locked, checked protection registers.** After configuration the C66x locks its MPAX and L2 attribute registers with the memory-protection lock (MPLK), proves the lock with two harmless writes, and the monitor compares every protection register with its snapshot each period. A refused write to a locked register is recorded like a refused memory write. The C71x page tables are read-only (SM-4). | `c66x.c` | A changed register: safe state, fault `protection`; a refused write: fault `access` at the register |
| SM-16 | **Clean restart.** A stopped core leaves nothing that disturbs its next start or another core: each start resets the core's DM timer, the C66x reset entry discards the caches its powered subsystem kept, the C66x tick route left in the interrupt router is verified and reused, and no IPC interrupt route is requested at all (both runtimes poll the mailbox). A fatal fault halts in the idle state, outside exception context, so Linux can always power the core off. | [`entry.c`](../firmware/c66x/src/entry.c), `main.c`, `c71x.c`, `ipc_service.c` | Restart by Linux at any time |

## Interference analysis

### Memory

| Fault | Mechanism | Status |
|---|---|---|
| DSP code writes another core's or Linux's memory, MSMC, or a peripheral its drivers do not use | SM-1, SM-2 | Prevented on both families; the device windows are the exception, see [residual risks](#residual-risks) |
| DSP code reads another core's or Linux's memory | SM-1, SM-2 | Prevented on the C71x; not on the C66x, whose XMC does not refuse reads |
| DSP code corrupts its own code or constants | SM-1, SM-2, SM-2a, SM-6 | Prevented in DDR and in the L2 code zone. Constants placed in writable L2 are checked by SM-6 |
| Executing data | SM-1, SM-2, SM-2a, SM-8 | Prevented in DDR and L2 on both families |
| Another master writes a C66x's L2 | SM-2a | Prevented and recorded |
| Stack overflow | SM-5 | C71x: trapped at the guard page. C66x: detected afterwards by the fill check |
| Heap exhaustion or fragmentation | SM-7 | No C heap; the kernel heap is frozen after start |
| Corruption of the protection itself | SM-4, SM-6, SM-3 | The C71x page tables are read-only and the C66x MPAX is self-tested. Code changes are found by the CRC check |
| Another master (Linux, DMA, the other cores) writes this core's DDR or C71x L2 | SM-6 detects changes to code and constants | Not prevented: the SoC firewalls on these paths are open, see [residual risks](#residual-risks) |

### Timing and execution

| Fault | Mechanism | Status |
|---|---|---|
| An algorithm runs too long | SM-9 | Detected at completion; the output is discarded |
| An algorithm never returns | SM-10, SM-11, SM-12 | FreeRTOS and bare-metal C71x: watchdog halt within 2 s. Bare-metal C66x: the heartbeat stops and the supervisor restarts the core |
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
| L2 code zone, `L2CODE` | read, execute; no other master (L2 page protection) | read, execute after start |
| Rest of usable L2 | read, write; no other master | read, write |
| Device windows | read, write, never execute: the seven windows in [`dsp-cores.json`](../manifest/dsp-cores.json) | the same, device memory: seven windows |
| Everything else: Linux's and the other cores' memory, MSMC, other peripherals | denied | unmapped |

The device windows come from the PDK sources the firmware links, each with
its source in the manifest:

| Window | Used by | C66x | C71x |
|---|---|---|---|
| CorePac-local space below 256 MiB: L2, L1, interrupt controller, cache, XMC, RAT, interrupt router, DM timers | everything on the core | 0x00000000, 256 MiB | — |
| DM timers | FreeRTOS tick | in the local window | 0x02400000, 2 MiB |
| Interrupt router through the C66x RAT | Sciclient | 0x20AC0000 or 0x20AD0000, 4 KiB | — |
| MAIN NAVSS secure proxy configuration | Sciclient | 0x31140000, 4 KiB | 0x31000000, 2 MiB |
| NAVSS mailboxes | IPC | 0x31F80000, 64 KiB | 0x31E00000, 2 MiB |
| MAIN NAVSS secure proxy real-time, thread configuration, target data | Sciclient | 0x32400000, 0x32800000, 0x32C00000, 1 MiB each | the same bases, 2 MiB each |
| CLEC interrupt controller | port, IPC, Sciclient | — | 0x78000000, 128 MiB |

The C71x windows are rounded to whole 2 MiB MMU blocks, so they cost no page
table. Sciclient on a DSP uses the MAIN NAVSS secure proxy; the MCU NAVSS one
belongs to MCU R5F builds. The first board run granted the MCU one by mistake,
and `Sciclient_init` failed: a missing window shows up as a start-up failure,
never as access to something else.

## Fault codes

STATUS and the health line report the first fault; later ones are dropped
because they are usually consequences. A restart clears it.

| Code | Name | Raised when | Address | Detail |
|---|---|---|---|---|
| 1 | integrity | a read-only range differs from its CRC | start of the range | range index; bit 31 set: the record itself is missing or damaged |
| 2 | stack | a stack reached its guard | C66x: base of the stack; C71x: the faulting address in the guard page | C66x: index of the stack; C71x: IERR and IESR |
| 3 | protection | the MPAX or MMU could not be configured, or a C66x protection register differs from its snapshot | the register, for a changed one | 1: MPAX segment 1 does not cover the carve-out; 2: too many segments; 0x400 + segment, or 0x500 + L2 page, for a changed register. C71x: 0x100 the page tables, 0x200 a stack guard, 0x300 the L2 split could not be mapped; 0x700 the vector table is not 2 MiB-aligned; 0x701 the vector base register did not take it; other values count failed MMU mappings |
| 4 | access | the protection blocked an access | the faulting address | C66x: XMPFSR, or for an L2 page fault bit 16 set and L2MPFSR (bits 9 to 15 name the master); C71x: IERR and IESR |
| 5 | exception | the C71x raised another exception | the faulting instruction | IERR and IESR |
| 6 | watchdog | a RUN ran past 2 s (FreeRTOS) | | algorithm id |
| 7 | layout | the load/run copy check failed | | |
| 8 | self-test | a protection self-test was not blocked | the probe address | 1: the other C66x's DDR; 2: MSMC; bit 16: the L2 code zone write |
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
safely continue after one. It records the fault, leaves exception context
through TI's return hook, and idles: a C71x idling inside its exception
handler, or one that never idles, cannot be powered off by Linux. The C66x
reports MPAX violations without stopping, since the hardware blocked the
write, so it can stay in `faulted`.

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
- Define `sdos_zone_l2code_start/end` on 16 KiB boundaries, the C66x L2
  protection page, and `sdos_zone_l2_end` at the end of usable L2. Code that
  runs from L2, such as a load/run copy, goes in that zone.
- Place executable sections only in the code zones.
- Place nothing writable in a code or read-only data zone. TI's C71x port
  puts two writable pointers in `.const` sections, which
  [`c71x_runtime.cmd`](../firmware/linker/common/c71x_runtime.cmd) places with
  the data.
- Place `.sdos.integrity` in loaded memory.
- Do not link `malloc()`. Allocate statically.
- On the C71x, keep guarded stacks on 4 KiB pages. Keep the MMU map within
  TI's 16 page tables: each zone boundary that is not on a 2 MiB boundary
  costs one table.
- On the C66x, the device windows and the zones must fit 14 MPAX segments.
  The defaults use 13. Zones that are naturally aligned powers of two take one
  segment each.

## Fault injection

The `*-fi` presets build test firmware with extra algorithms, each of which
commits one fault. They are built in every quality run and never shipped in
the image. `python3 -m tests.target.sdos_board fault-injection --out DIR` runs
them on a board. For each case it reads the health line, classifies what the
firmware reported, and restarts the core.

| Case | C66x | C71x |
|---|---|---|
| write into the other C66x's memory | access | access, halted |
| read Linux's memory | not detected (reads pass the XMC) | access, halted |
| write into the code zone | access | access, halted |
| execute from the data zone | stopped heartbeat | access, halted |
| recurse into the stack guard | stack | stack, halted |
| never return | FreeRTOS: watchdog, halted; bare metal: stopped heartbeat | watchdog, halted |
| run 5 ms on a 1 ms budget | deadline missed | deadline missed |
| change read-only data the CRC record covers | integrity | integrity |
| allocate from the kernel heap after start | FreeRTOS: heap; bare metal: not applicable | the same |
| write into the L2 code zone | access | access, halted |
| read MSMC | not detected (reads pass the XMC) | access, halted |
| rewrite a protection register | access: the lock refuses and records it | access, halted: the page tables are read-only |

The table is the board result of 2026-10-19 for every core and runtime
([evidence](../release/evidence/task-042/)); every case recovered with
`dspctl recover`. The C71x stack case hung the core until the firmware got
its own exception stack; it has been recorded and recovered since
([evidence](../release/evidence/task-042/20261019T164820Z/)).

The test firmware also carries one diagnostic that injects nothing:
`dspctl run <dsp> 0xfa00000d - -` reads every region of the SoC firewalls on
the paths into the DSPs' memory through TI-SCI and traces the enabled ones
([residual risks](#residual-risks)).

## Residual risks

Each of these needs hardware, a decision, or both:

- **Other bus masters.** Linux, DMA engines, and the other cores can still
  write a DSP's DDR and the C71x's L2; the C66x L2 refuses them (SM-2a).
  SM-6 detects changes to code and constants. A read-only survey through
  TI-SCI on 2026-10-19 (fault-injection diagnostic `0xFA00000D`,
  [evidence](../release/evidence/task-042/20261019T155359Z/firewall-survey.txt))
  found every SoC firewall on those paths open:

  | Firewall | Path | State on this board |
  | --- | --- | --- |
  | 257, 265, 284 | A72, C71x, C71x DMA into MSMC and DDR (initiator side) | No region enabled: passes everything |
  | 4762, 4763 | Main-domain masters (C66x, R5F, DMA) through NAVSS into DDR | No region enabled |
  | 1280 | MCU domain (device manager) into main and DDR | One region, all addresses, every priv-id |
  | 264 | Anything into the C71x L2 | No region enabled |
  | 2304, 2305 | Anything into a C66x L2 or L1D | No region enabled |

  TI's system firmware leaves all of these with owner "none" (264, 2304,
  2305, 4762, 4763 are not in its boot table at all), and any host may claim
  an unowned firewall with `TISCI_MSG_CHANGE_FWL_OWNER` and program it, so
  the DSP firmware can close them at run time without a boot-chain change.
  Priv-ids (TI's J721E list): A72 1, C71x 21, C66x_0 220, C66x_1 221, MCU R5F
  96 and 97, main R5F 212 to 215. Linux loads the firmware, so the A72 must
  keep write access to the carve-outs; the C71x L2 needs no other master.
  Programming a firewall changes the whole SoC's bus, and a wrong region can
  cut Linux or the device manager off. The maintainer decided on 2026-10-19
  not to program the DDR firewalls; only the C71x L2 firewall, which no
  other master needs, is being tried.
- **Hardware watchdog.** The RTI watchdogs report through the ESM, which can
  reset the whole SoC. Routing one per DSP is a maintainer decision. The
  software watchdog and the Linux supervisor cover a stuck core.
- **C66x reads.** The C66x XMC does not refuse reads outside the MPAX grants,
  so a C66x can read Linux's and the other cores' memory. That is a
  confidentiality gap, not interference: writes, which corrupt, are refused.
- **C71x stack overflow past the guard.** The guard is one 4 KiB page. A
  function whose frame is larger than that (a big local array) can move the
  stack pointer past the guard and write below it without touching it; on
  the C71x that is other data in the carve-out. Keep large buffers static;
  peak use is reported. Faults inside the exception handler itself, which
  runs on its own unguarded 16 KiB stack, are not recorded.
- **Device windows.** They are derived from the PDK sources the firmware
  links and confirmed on the board by the course smoke test and every
  fault-injection run (2026-10-19). A peripheral a driver needs that the list
  misses shows up as a start-up failure, as the MCU secure proxy did on the
  first run; add a window to the manifest. Inside a window, the core can still write registers
  it does not use: the C66x local window spans the whole CorePac space below
  256 MiB, and the C71x keeps TI's 128 MiB CLEC window.
- **Protection registers.** Locked and checked on the C66x (SM-17); the lock
  key is in the firmware image, so the lock stops accidents, not code that
  sets out to unlock it.
- **Unguarded stacks.** These are FreeRTOS's idle and timer task stacks and
  the C71x interrupt stack, all inside TI's prebuilt kernel. FreeRTOS's own
  overflow check, method 2, covers the task stacks at each context switch.
- **Kernel heap.** TI's FreeRTOS kernel allocates during start-up. The heap
  cannot be removed without rebuilding that library; SM-7 freezes it instead.
- **Static analysis.** Cppcheck and the compilers' warnings, as errors, are
  the static analysis. The maintainer decided on 2026-10-19 against MISRA
  checking; the project makes no MISRA claim.

## Verification status

The following runs in every `make quality` and in CI:

- the unit tests for the protocol, runtime, and safety code;
- the mock and CLI tests;
- all firmware builds, with the post-link rules and the integrity check;
- the real-linker negative tests in
  [`test_linker_layouts.py`](../ci/test_linker_layouts.py).

On the board, 2026-10-19 (Tasks 021 and 042):

- the MPAX, L2, and MMU maps boot on every core and runtime, and the course
  smoke test passes twice in a row
  ([evidence](../release/evidence/task-021/));
- every fault-injection case behaves as the table above states, and every
  core recovers after its fault
  ([evidence](../release/evidence/task-042/)).
