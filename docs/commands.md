# Build and test commands

Status: Task 016 command facade with the Task 017 container; host presets
verified, firmware presets wait for the TI layer in Task 019  
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
Docker's cache and take seconds.

## Commands

| Command | What it does | Available |
|---|---|---|
| `make bootstrap` | Builds the pinned container and lists the tool versions inside it | Yes |
| `make configure PRESET=<p>` | Runs CMake for one preset or group | Yes |
| `make build PRESET=<p>` | Configures and builds | Yes; firmware presets need Task 019 |
| `make test PRESET=<p>` | Builds and runs unit tests on the PC | Host presets only; on-target tests arrive in Task 033 |
| `make quality` | Manifest validators, Python tests, and every host preset with warnings as errors | Yes; grows with Tasks 018 and 024 |
| `make clean` | Deletes `build/` | Yes |
| `make package` | Packages firmware and host artifacts | Task 022 |
| `make image` | Builds the course Linux image | Task 035 |
| `make deploy BOARD=<name>` | Installs a build on a board | Task 030 |
| `make target-smoke BOARD=<name>` | Runs smoke tests on a board | Task 038 |
| `make help` | Lists commands and presets | Yes |

`PRESET` defaults to `host-gcc-debug`. It also accepts a group or a
comma-separated list.

## Build matrix

| Group | Presets | Product |
|---|---|---|
| `host` | `host-{gcc,clang}-{debug,release}` | Libraries and unit tests for the PC |
| `board-host` | `aarch64-gcc-{debug,release}` | Linux programs for the board's Cortex-A72 cores |
| `firmware` | `{c66x_0,c66x_1,c71x_0}-{freertos,baremetal}-{debug,release}` | DSP builds with the TI C6000 or C7000 compiler |
| `all` | All 18 | |

Each preset builds into `build/<preset>/` and writes `compile_commands.json`
there for editors.

The firmware presets currently compile the portable algorithms with the TI
compiler. Firmware images with a resource table, linker command file, and
FreeRTOS or bare-metal runtime are added by Tasks 020 and 021. Until Task 019
installs the TI compilers, configuring a firmware preset stops with a message
naming the missing compiler.

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
