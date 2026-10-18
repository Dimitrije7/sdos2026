# 017 — Build the open-source containers and dev container

## Status

done

## Completion evidence

- [`toolchain/Dockerfile`](../toolchain/Dockerfile) defines `oss-base`, `ci`,
  `dev`, and `docs`. Every input comes from the manifest through
  [`ci/container.py`](../ci/container.py): the Ubuntu base by digest, packages
  from snapshot 20261004T000000Z with each InRelease and requested `.deb`
  checked against its SHA-256, hadolint, Syft, Trivy, and Node.js checked
  against their SHA-256, Python tools from
  [`toolchain/python-tools.lock`](../toolchain/python-tools.lock) with
  `--require-hashes`, and markdownlint-cli2 from
  [`toolchain/node/package-lock.json`](../toolchain/node/package-lock.json),
  resolved with `--before` the manifest cutoff. Every locked Python
  dependency was published before the cutoff.
- The image tag hashes the build context and arguments, so any pin change
  requires a new `make bootstrap`. `.devcontainer/devcontainer.json` and
  `compose.yaml` run the same image. The facade runs `bootstrap`, `configure`,
  `build`, `test`, and `quality` in it; CI will call the same commands.
- Workstation needs: Git, Docker, Make, and Python 3.10, which Tier 1 Ubuntu
  22.04 provides, plus an editor.
- Verification, 2026-10-18:
  - A cold build took 17 minutes, dominated by the snapshot download; later
    rebuilds reuse the apt cache mount.
  - A build with one wrong InRelease digit failed at `sha256sum --check`.
  - `hadolint toolchain/Dockerfile` is clean, with DL3008 suppressed because
    versions arrive through `APT_PACKAGES`; a fixture with an unpinned base and
    package fails hadolint.
  - `make quality` passed from a fresh clone and again with the lint steps: 90
    Python tests, three validators, hadolint, ShellCheck, and four host presets
    with warnings as errors.
  - `docker compose config` and `docker compose run --rm dev` work. The `board`
    profile was not exercised with a device attached.
  - SBOM and vulnerability scan:
    [`release/evidence/task-017/`](../release/evidence/task-017/). 1,016
    packages. No critical finding applies: all 7 are kernel CVEs in the
    header package `linux-libc-dev`. The OpenSSL high finding needs a snapshot
    move.
- Carried to later tasks: `reuse` is source-only and moves to Task 024, with the
  vulnerability policy and the yamllint and markdownlint configuration. Their
  first runs report style findings in `.github/workflows/feasibility.yml` and
  task files.
- Decisions: none needed.

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
