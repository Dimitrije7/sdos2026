# 017 — Build the open-source containers and dev container

## Status

pending

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
