# dspctl: control the DSPs from Linux

`dspctl` lists, loads, starts, stops, and talks to the three DSPs of the
BeagleBone AI-64. It uses [`libsdos_dsp`](../host/libsdos_dsp/include/sdos/dsp.h),
the same library a lab application links, and speaks
[protocol 1.0](protocol.md).

A DSP is always named by its logical name, never by a `remoteprocN` number:

| Name | Core | Device address |
|---|---|---|
| `c66x_0` | C66x DSP 0 | `4d80800000.dsp` |
| `c66x_1` | C66x DSP 1 | `4d81800000.dsp` |
| `c71x_0` | C71x DSP | `64800000.dsp` |

## Prerequisites

- **On the PC:** `make build PRESET=host` builds `dspctl` for the PC. It is at
  `build/host-gcc-debug/host/dspctl`. Add `--backend mock` to run without a
  board.
- **For the board:** `make build PRESET=board-host` cross-builds it for the
  board, at `build/aarch64-gcc-release/host/dspctl`.
- **On the board:** loading, starting, and stopping need root, because they
  write remoteproc sysfs files. Reading the trace also needs debugfs.

## Commands

| Command | What it does |
|---|---|
| `dspctl list` | Every DSP, its state, and the firmware remoteproc will load |
| `dspctl status <dsp>` | State; when running, the firmware identity and counters |
| `dspctl load <dsp> <file>` | Install firmware on a stopped DSP after checking it fits the core |
| `dspctl start <dsp>` | Start and wait until the firmware answers HELLO |
| `dspctl stop <dsp>` | Stop the DSP |
| `dspctl recover <dsp>` | Stop it if running or crashed, then start it |
| `dspctl trace <dsp>` | Print the firmware trace buffer |
| `dspctl echo <dsp> [text]` | Send text and print the reply |
| `dspctl run <dsp> <algorithm> <in> <out> [gain]` | Run `copy` or `gain_q15` over a file |

Options go before the command: `--backend linux|mock` (default `linux`),
`--timeout <ms>` (default 2000), `--json`, and `--root <dir>`, which tests use
to point the tool at a fake sysfs tree. With `--json`, `list` prints
`{"dsps": [{"name", "device", "remoteproc", "state", "firmware"}, ...]}`, and
`status` prints the same fields for one DSP, plus `identity` and `stats` when
it is running.

`load` copies the file to `/lib/firmware/sdos/<dsp>.xe66` (`.xe71` for the
C71x) and selects it. Before that, it refuses:

- a file built for the other DSP family;
- any loadable segment outside that core's memory carve-out;
- a file without an RPMsg resource table.

The limits come from
[`manifest/dsp-cores.json`](../manifest/dsp-cores.json).

## Expected output

On the PC with the mock backend:

```text
$ dspctl --backend mock list
DSP      DEVICE           REMOTEPROC   STATE      FIRMWARE
c66x_0   4d80800000.dsp   mock         running    mock/c66x_0-course-runtime
c66x_1   4d81800000.dsp   mock         running    mock/c66x_1-course-runtime
c71x_0   64800000.dsp     mock         running    mock/c71x_0-course-runtime

$ dspctl --backend mock status c66x_0
C66x_0 (4d80800000.dsp) running
  firmware   mock c66x_0
  runtime    mock 0.1.0
  region     1048576 bytes, 128-byte alignment
  requests   2, errors 0, runs 0, last error ok

$ dspctl --backend mock echo c71x_0 hello from linux
hello from linux

$ dspctl --backend mock run c66x_1 gain_q15 in.bin out.bin 8192
gain_q15: 4096 bytes in, 4096 bytes out, 0 DSP cycles, checksums verified
```

The gain is a Q15 value: 8192 is 0.25 and 16384 is 0.5. Samples are 16-bit
little-endian integers; the result is rounded half up and saturated.

## Exit status

| Status | Meaning |
|---|---|
| 0 | Success |
| 1 | The operation failed; the message says why |
| 2 | Usage error, an unknown DSP, or an unknown algorithm |

## Failure modes and recovery

| Message | Cause | Recovery |
|---|---|---|
| `is not present on this system` | No remoteproc device has this address: not a BeagleBone AI-64, or the kernel lacks K3 DSP remoteproc | Check `dmesg \| grep dsp` |
| `load needs c66x_0 offline, but it is running` | Firmware can only change while the DSP is stopped | `dspctl stop c66x_0`, then load |
| `lies outside the c66x_0 carve-out` | The firmware was linked for another core or memory map | Build it with the preset for this core |
| `has no rpmsg_ctrl device` | The firmware runs but set up no RPMsg | Check `dspctl trace`; the firmware needs the course resource table |
| `did not answer ... within 2000 ms` | The firmware is busy, hung, or crashed | `dspctl trace <dsp>`, then `dspctl recover <dsp>` |
| `crashed while starting` | The firmware faulted during start | `dspctl trace <dsp>`; `dspctl recover <dsp>` after fixing it |
| `checksum mismatch` | Data changed between the DSP and the host, usually a cache step | Report it with the trace; the buffers are not trusted |
| `bad buffer state` | A buffer is still quarantined after a timeout | `dspctl recover <dsp>` frees it; acquire it again |
| `shared buffers on the board wait for the Task 027 data-path decision` | `run` on the board before the data path exists | Use `--backend mock` on the PC for now |

## Status

The protocol, library, and mock run on the PC, and the Linux backend is tested
against a simulated kernel. The board run comes in the next board session,
with the firmware from Task 021. Shared buffers on the board follow the Task
027 decision.
