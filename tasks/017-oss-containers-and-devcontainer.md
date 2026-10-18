# 017 — Build the open-source containers and dev container

## Status

in-progress

## Handoff

- Updated: 2026-10-18, claude.
- State: The `oss-base`, `ci`, and `dev` targets build from manifest pins and
  run the command facade. Lint, scanning, SBOM, Compose profiles, and the
  `docs` target remain.
- Done:
  - [`toolchain/Dockerfile`](../toolchain/Dockerfile): base image by digest,
    Ubuntu snapshot 20261004T000000Z, each InRelease and requested `.deb`
    checked against the manifest SHA-256 under `set -e`, Python tools from
    [`toolchain/python-tools.lock`](../toolchain/python-tools.lock) with
    `--require-hashes`.
  - [`ci/container.py`](../ci/container.py) derives build arguments and a
    content-addressed tag from the manifest and the build context;
    `.devcontainer/devcontainer.json` opens `sdos-dev:current`.
  - Rootless and rootful Docker both leave build output owned by the user.
- Next:
  1. Add the `docs` target, Compose profiles with optional board access, and
     hadolint or an equivalent Dockerfile lint.
  2. Generate an SBOM and run a vulnerability scan with pinned open tools;
     record results.
  3. The facade still needs Python 3 and Make on the host; decide whether that
     meets "only Git, Docker, and an editor" or add a Docker-only entry point.
  4. `reuse` is an sdist with build-time dependencies; it moves to Task 024.
- Decisions: none needed.
- Verification, 2026-10-18: a cold build took 17 minutes, dominated by the
  snapshot download; a lock-file change rebuilt in seconds. A build with one
  wrong InRelease digit failed at `sha256sum --check`. `ci.test_container`
  passes.

## Depends on

004 and 016.

## Plan references

Phase 3 and Section 6.5.

## Objective

Provide a pinned, redistributable development environment before adding the
licensed TI layer.

## Scope

- Implement pinned BuildKit targets for `oss-base`, `dev`, `ci`, and `docs`.
- Install compilers, CMake/Ninja, test tools, linters, analysis, and docs tools
  at explicit versions.
- Add `.devcontainer` and Docker Compose profiles for normal development and
  optional connected-board device access.
- Run the command facade inside containers with non-root defaults and durable
  caches that contain no secrets.
- Record base-image digests and generate an initial SBOM.

## Out of scope

TI installers, privileged image building, or publishing images before scans.

## Maintainer checkpoint

None.

## Acceptance criteria

- A clean supported workstation needs only Git, Docker, and an editor.
- Local container commands and planned CI entry points are identical.
- Images are pinned, lint-clean, scanned, and free of credentials/restricted
  payloads.

## Verification and evidence

Perform clean builds, startup/compiler smoke tests, command-facade tests, lint,
vulnerability scanning, and SBOM generation.
