# Build and test commands

Status: host, board-host, and firmware presets and the TI SDK layer verified
on 2026-10-18  
Code: [`Makefile`](../Makefile), [`ci/facade.py`](../ci/facade.py),
[`ci/container.py`](../ci/container.py),
[`toolchain/Dockerfile`](../toolchain/Dockerfile),
[`CMakePresets.json`](../CMakePresets.json)

Every build, test, and check runs through `make`. Continuous integration calls
the same commands, so a green local `make quality` predicts a green pull
request.

## Before the first build

You need Git, GNU Make, Python 3.10 or newer, and Docker. Docker Engine on Linux
works in rootful and rootless mode. Then, from the repository root:

```sh
make bootstrap
```

`bootstrap` builds the development container from pinned inputs only: the
Ubuntu 22.04 base image by digest and every package at an exact version from
the Ubuntu snapshot archive of 2026-10-04. It checks each package against the
SHA-256 in [`manifest/versions.json`](../manifest/versions.json). The image tag
is derived from those pins, so a manifest change makes the other commands ask
for a new `make bootstrap`.

The first bootstrap downloads several hundred megabytes. Later runs reuse
Docker's cache and take seconds. After a successful build, bootstrap untags
`sdos-dev:dev-*` images built from earlier pins, so only the current open and
TI images stay on disk.

### TI DSP compilers

The firmware presets need TI's C6000 8.3.7 and C7000 5.0.0.LTS compilers. They
are free of charge but proprietary, and the project may not redistribute them,
so each machine downloads them from TI. `make bootstrap` prints the TI pages
that present the terms. After reading them:

```sh
make bootstrap TI=accept
```

This records your acceptance of exactly the pinned TI inputs in
`~/.config/sdos/ti-terms-accepted.json`. It then:

- downloads TI's Processor SDK RTOS archive, 3.7 GB, once into
  `~/.cache/sdos/inputs/`, resuming after interruptions, and checks its SHA-256;
- downloads both compiler installers from TI, checks their SHA-256, and
  installs them unattended;
- extracts the SDK parts that firmware needs, about 2.1 GB: the PDK with
  FreeRTOS and drivers, the C66x DSPLIB, the C7x DSPLIB and FFTLIB, MMALIB,
  and the SDK build scripts. The archive is checked again before extraction.

Expect about 20 GB of free disk and, on the first run, as long as the SDK
download takes. `SDOS_INPUT_CACHE` moves the download cache. Later bootstraps reuse the record. The
layer stays on your machine: never push an `sdos-dev:*-ti-*` image to a
registry. A pin change makes the record stale, so you accept the new
installers again.

Trusted CI jobs set `SDOS_TI_TERMS=accept`, under the maintainer's acceptance
for project automation recorded in Task 005. Jobs for fork pull requests do
not.

## Commands

| Command | What it does | Available |
|---|---|---|
| `make bootstrap` | Builds the pinned container and lists the tool versions inside it | Yes |
| `make configure PRESET=<p>` | Runs CMake for one preset or group | Yes |
| `make build PRESET=<p>` | Configures and builds | Yes; firmware presets need `TI=accept` at bootstrap |
| `make test PRESET=<p>` | Builds and runs unit tests on the PC | Host presets only; on-target tests arrive in Task 033 |
| `make quality` | Manifest and workflow validators, hadolint, actionlint, zizmor, ShellCheck, Python tests, every host preset with tests, and, when TI is installed, every firmware preset and the three release TI references, all with warnings as errors | Yes; grows with Task 024 |
| `make clean` | Deletes `build/` | Yes |
| `make package` | Packages firmware and host artifacts | Task 022 |
| `make image` | Builds `dspctl` and the firmware, then the course SD image from the pinned BeagleBoard image; see [`image.md`](image.md) | Yes; the from-source kernel variant is still Task 035 |
| `make deploy BOARD=<name>` | Installs a build on a board | Task 030 |
| `make target-smoke BOARD=<name>` | Runs smoke tests on a board | Task 038 |
| `make help` | Lists commands and presets | Yes |

`PRESET` defaults to `host-gcc-debug`. It also accepts a group or a
comma-separated list.

## Build matrix

| Group | Presets | Product |
|---|---|---|
| `host` | `host-{gcc,clang}-{debug,release}`, `host-gcc-sanitize` | Libraries and unit tests for the PC; the last one with AddressSanitizer and UndefinedBehaviorSanitizer |
| `board-host` | `aarch64-gcc-{debug,release}` | Linux programs for the board's Cortex-A72 cores |
| `firmware` | `{c66x_0,c66x_1,c71x_0}-{freertos,baremetal}-{debug,release}` | Course firmware for each DSP, built with the TI C6000 or C7000 compiler |
| `ti-reference` | `ti-reference-{c66x_0,c66x_1,c71x_0}-{debug,release}` | TI's IPC echo firmware rebuilt from the pinned PDK |
| `all` | All 25 | |

Each preset builds into `build/<preset>/` and writes `compile_commands.json`
there for editors.

A firmware preset produces `build/<preset>/firmware/sdos-<core>-<runtime>.xe66`
(`.xe71` for the C71x) and its map file. Every build checks the result with
[`ci/firmware_elf.py`](../ci/firmware_elf.py) and fails when a segment leaves
the core's carve-out or the resource table is missing; see
[`firmware.md`](firmware.md). Without the TI layer, configuring a firmware
preset stops with a message naming the missing compiler. A compiler whose
version differs from the manifest is rejected.

A `ti-reference` preset runs TI's own PDK make for the `ipc_echo_test_freertos`
example and writes `build/<preset>/build-manifest.json` with input and output
hashes, the fit check, and, for release builds, a comparison with TI's
prebuilt blob. `make quality` builds the three release references; the debug
ones recompile the whole PDK, because TI ships release libraries only.

Each build tree holds `sdos-build-info.json`, recording the product, core,
runtime, compiler and version, the TI installer and SDK archive SHA-256, and
the image tag.

## Exit status

The facade exits with:

| Status | Meaning |
|---|---|
| 0 | Success |
| 1 | A build, test, or check failed, or the container is not built |
| 2 | Usage error: unknown command or preset, or a missing `BOARD` |
| 3 | Refused for safety |
| 4 | The command or target exists in the contract but is not available yet |

`make` itself always exits with 2 when a step fails; it prints the facade's own
status as `Error <n>`. CI calls `python3 ci/facade.py <command>` directly to get
the status unchanged.

## Editors and shells

- **VS Code:** open the repository and choose "Reopen in Container". The
  `.devcontainer` definition runs `make bootstrap` and opens `sdos-dev:current`
  with clangd and CMake Tools.
- **Shell:** `docker compose run --rm dev` opens a shell in the same image.
  `docker compose --profile board run --rm board` adds read access to USB
  serial devices. Flashing never runs in the container.

## What the container holds

| Kind | Tools | Pinned by |
|---|---|---|
| Ubuntu packages | GCC 12, Clang 15 with clang-tidy, clang-format, and clangd, the aarch64 cross GCC 12, CMake 3.22, Ninja, Python 3.10, ShellCheck, lcov, dtc | Snapshot 20261004T000000Z; each `.deb` SHA-256 in the manifest |
| Python tools | jsonschema, yamllint, lizard, cmakelang, zizmor | [`toolchain/python-tools.lock`](../toolchain/python-tools.lock), hashes for every wheel |
| Release binaries | hadolint, actionlint, Syft, Trivy, Node.js 22 | SHA-256 in the manifest |
| Node.js tools | markdownlint-cli2 | [`toolchain/node/package-lock.json`](../toolchain/node/package-lock.json) |

The Dockerfile targets are `oss-base`, `ci`, `dev`, and `docs`; all currently
hold the same tools. The TI compilers are added by Task 019 in a separate layer
that is never published.

## Without Docker

`SDOS_NATIVE=1 make <command>` uses tools installed on the host instead of the
container. `make bootstrap` then only lists which tools are missing. Native
builds are not reproducible and are not supported for coursework.

## Where code goes

| Directory | Content |
|---|---|
| `algorithms/` | Portable DSP algorithms with unit tests; each builds for every preset |
| `cmake/` | Build options, warning policy, and toolchain files |
| `toolchain/` | Container definitions |
| `ci/` | The facade, validators, and their tests |

## Verify the facade

```sh
python3 -m unittest -v ci.test_facade
```

The tests cover preset selection, every unavailable command, refusal exit
codes, and the container commands, without Docker.
