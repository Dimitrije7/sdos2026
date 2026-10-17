# 035 — Implement the reproducible Linux image builder

## Status

pending

## Depends on

009, 015, and 017.

## Plan references

Phase 6 and Sections 6.4–6.6.

## Objective

Turn the pinned upstream Beagle image into a deterministic, inspectable course
image without maintaining a hand-edited master card.

## Scope

- Implement an `image-builder` container with the minimum required privileges.
- Verify the upstream image and apply versioned package/file/configuration
  transformations from source.
- Produce SD and eMMC-flasher layouts with manifests and deterministic metadata
  where upstream formats permit.
- Add static partition/filesystem/package/configuration tests and dry runs.
- Fit main/nightly GitHub-hosted runner storage, memory, and time budgets.

## Out of scope

Embedding secrets, mutable downloads, undocumented live edits, or publishing an
unqualified release.

## Maintainer checkpoint

None unless runner limits force an architecture change.

## Acceptance criteria

- `make image` builds from pinned inputs in a clean authorized environment.
- All customizations are traceable to repository source and testable offline.
- The job operates with documented least privilege and measured resource use.

## Verification and evidence

Run two clean builds, compare files/manifests, execute negative/static image
tests, and record resource/timing data.
