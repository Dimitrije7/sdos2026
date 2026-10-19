# SDOS 2026 — BeagleBone AI-64 DSP Course Platform

A reproducible teaching platform for the Systems for Digital Signal Processing
course. Linux on the board's A72 cores controls student firmware on two C66x
DSPs and one C71x DSP. Everything builds locally in one pinned container, and
CI runs the same commands.

## Start here

```sh
make bootstrap TI=accept     # pinned container; shows TI's terms for the DSP compilers
make build PRESET=all        # PC, board, and all 20 DSP firmware builds
make test                    # unit tests on the PC
make quality                 # everything a pull request runs
```

Without a board, the Linux side runs against a simulated DSP:

```sh
build/host-gcc-debug/host/dspctl --backend mock list
build/host-gcc-debug/host/dspctl --backend mock echo c66x_0 hello
```

- [`docs/commands.md`](docs/commands.md): every command, preset, and exit status.
- [`docs/firmware.md`](docs/firmware.md): the DSP firmware.
- [`docs/dspctl.md`](docs/dspctl.md): controlling DSPs from Linux.
- [`docs/protocol.md`](docs/protocol.md): the Linux–DSP protocol.
- [`docs/linker.md`](docs/linker.md): DSP memory layout and the linker files.
- [`docs/safety.md`](docs/safety.md): memory protection, self-supervision, and fault handling on the DSPs.
- [`docs/board-access.md`](docs/board-access.md): flashing and checking a board.

## What is where

| Path | Contents |
|---|---|
| `firmware/common/` | Protocol, runtime, and safety code shared by every DSP and by Linux; IPC service, resource table, trace, monitor |
| `firmware/c66x/`, `firmware/c71x/` | Core-specific adapters: caches, memory protection (MPAX, MMU), exceptions, cycle counter |
| `firmware/linker/` | Editable TI linker files: per-core main files, module fragments, examples |
| `firmware/runtime/` | FreeRTOS and bare-metal entry points |
| `host/libsdos_dsp/` | Linux library: lifecycle, messages, buffers; Linux and mock backends |
| `host/dspctl/` | Command-line tool over the library |
| `algorithms/` | Portable DSP algorithms with PC tests (Q15 FIR so far) |
| `toolchain/`, `.devcontainer/`, `compose.yaml` | The pinned container and editor integration |
| `cmake/` | Presets support, toolchains, TI firmware linking, generated core table |
| `ci/` | The command facade, validators, firmware post-link and ELF checks, TI reference builds |
| `tests/target/` | Agent-run board tools: flash, health report, DSP IPC, course smoke, fault injection |
| `manifest/` | Pinned versions and hashes, license matrix, DSP identities and memory |
| `release/evidence/` | Proof for completed tasks |
| `tasks/` | The dependency-ordered work plan and each task's state |

## Status

What runs today, and what does not yet, is tracked per task in
[`tasks/README.md`](tasks/README.md). The roadmap is
[`docs/bringup-plan.md`](docs/bringup-plan.md).

AI-assisted contributors must read [`AGENTS.md`](AGENTS.md). The repository
uses one canonical, provenance-tracked skill set under `.opencode/skills/`,
shared with supported agent harnesses through repository symlinks.
