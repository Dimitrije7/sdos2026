# 019 — Implement the licensed TI toolchain layer

## Status

done

## Completion evidence

- `toolchain/Dockerfile` stages `ti-toolchain` and `ti-sdk` install both TI
  compilers and the SDK subset from TI's URLs, each checked against its
  manifest SHA-256 under `set -e`. They are built only after
  `make bootstrap TI=accept`, which records acceptance of the exact pinned
  inputs, or with `SDOS_TI_TERMS=accept` in trusted CI; images carry `-ti-` in
  their tag and are never pushed. The SDK archive is downloaded once into the
  user's cache with resume, and checked again inside the build.
- `cmake/SdosOptions.cmake` rejects a compiler version that differs from the
  pin and writes `sdos-build-info.json` with the installer and SDK hashes.
- Evidence: [`release/evidence/task-019/summary.md`](../release/evidence/task-019/summary.md)
  and [`psdk-inventory.json`](../release/evidence/task-019/psdk-inventory.json),
  which maps all 26,317 extracted files to rows of TI's manifests.
- Manifest updates: version manifest revision 3 records the real C7x DSPLIB,
  FFTLIB, and MMALIB folders and versions. License matrix revision 6 records
  the inventory under `LIC-001` and the 49 C66x DSPLIB files whose headers cite
  a TMS320-only license; `LIC-001` stays open for publication.
- Verification, 2026-10-18: `make quality` in the TI image passed with 98
  Python tests, hadolint, actionlint, zizmor, ShellCheck, 4 host presets, and
  12 firmware presets with warnings as errors. Wrong installer and SDK hashes
  stopped the build; a wrong expected compiler version failed configuration;
  the secret scan found nothing. GitHub run 39162211385 built all 12 firmware
  presets on a hosted runner with fresh downloads and no cache.
- Fork safety: no repository secret exists, fork pull requests run without
  `SDOS_TI_TERMS`, and no TI payload enters an Actions cache or artifact.
- Decisions: the maintainer accepted the TI terms for project automation in
  Task 005. The agent's local builds used `SDOS_TI_TERMS=accept` under that
  acceptance and wrote no personal acceptance record.

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
