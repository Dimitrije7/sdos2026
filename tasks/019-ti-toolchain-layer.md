# 019 — Implement the licensed TI toolchain layer

## Status

pending

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
