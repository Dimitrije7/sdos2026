# 019 — Implement the licensed TI toolchain layer

## Status

in-progress

## Handoff

- Updated: 2026-10-18, claude.
- State: Both TI compilers install in the `ti-toolchain` layer and build all 12
  firmware presets. The PDK and DSP library subset and the path-by-path
  inventory for `LIC-001` remain.
- Done:
  - `toolchain/Dockerfile` stage `ti-toolchain`: downloads each installer from
    TI, checks its manifest SHA-256 under `set -e`, installs unattended, and
    records the hash in `/opt/ti/<family>.installer.sha256`. `dev` and `ci`
    use it as their base only after acceptance.
  - Acceptance: `make bootstrap TI=accept` records the user's acceptance of the
    exact pinned installers in `~/.config/sdos/ti-terms-accepted.json`, after
    the facade prints TI's terms pages. Trusted CI sets `SDOS_TI_TERMS=accept`.
    The TI image tag carries `-ti-` and is never pushed.
  - `cmake/SdosOptions.cmake` rejects a compiler whose version differs from
    the pin and writes `sdos-build-info.json` in each build tree.
  - `make quality` builds all firmware presets with warnings as errors when TI
    is installed.
- Next:
  1. Add the PSDK RTOS subset (PDK, C66x DSPLIB, C7x libraries, FreeRTOS)
     through the stream extraction proven by Task 005, as a separate layer.
  2. Inventory the extracted paths and their licenses to close `LIC-001`, and
     propose a matrix revision if a BSD-only subset may be cached.
  3. Complete the task.
- Decisions: the maintainer accepted the TI terms for project automation in
  Task 005. The agent's local build used `SDOS_TI_TERMS=accept` under that
  acceptance and wrote no personal acceptance record.
- Verification, 2026-10-18: in `sdos-dev:dev-ti-*`, `make quality` passed with
  94 Python tests, 4 host presets, and 12 firmware presets. A wrong C6000
  installer hash failed at `sha256sum --check`; `-DSDOS_TI_EXPECTED_VERSION=8.3.8`
  failed configuration with the version message; `trivy fs --scanners secret`
  found nothing; the image history holds no credentials.

## Depends on

005, 006, and 017.

## Plan references

Phase 3 and the TI license handling in Section 6.5.

## Objective

Add reproducible C6000/C7000/PDK tooling without placing restricted payloads or
credentials in Git, public registries, or PR logs.

## Scope

- Implement checksummed acquisition or controlled injection for accepted TI
  artifacts under the approved license model.
- Build the `ti-toolchain` container layer or documented local equivalent.
- Record exact compiler/PDK versions and reject mismatches.
- Add smoke builds for each compiler family and offline-cache behavior where
  authorized.
- Scrub build history, logs, environment, and artifacts for credentials.

## Out of scope

Redistribution not authorized by Task 003 or manual SDK edits inside a running
container.

## Maintainer checkpoint

None after the Task 003 license decision; credential approval may be required,
but the agent performs setup and validation.

## Acceptance criteria

- A clean authorized environment installs both toolchains deterministically.
- Versions/hashes are verified before use and visible in build manifests.
- Fork/PR jobs cannot access restricted credentials or cached payloads.

## Verification and evidence

Run clean and cached smoke builds, negative hash/version tests, secret scanning,
and compare installed manifests with Task 004.
