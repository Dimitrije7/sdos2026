# 004 — Create the candidate version manifest

## Status

done

## Depends on

001 and 003.

## Plan references

Section 4, Phase 0, and Section 6.11.

## Objective

Make every candidate platform input explicit, immutable, and machine-readable.

## Scope

- Define the manifest schema and validation command.
- Pin the Beagle image, kernel/DTB source, PSDK/PDK, TI compilers, firmware,
  container bases, build tools, and key Actions.
- Record canonical URLs, versions, SHA-256 values, licenses, and compatibility
  notes without storing restricted binaries.
- Distinguish candidate, qualified, semester-frozen, and superseded states.

## Out of scope

Declaring the candidate hardware-qualified or building final containers.

## Maintainer checkpoint

None unless a pin conflicts with an accepted architecture or license decision.

## Acceptance criteria

- Schema validation rejects missing hashes, mutable `latest` inputs, and unknown
  lifecycle states.
- Every Section 4 component is represented.
- A documented command verifies all locally available inputs.

## Verification and evidence

Run positive and deliberately invalid manifest fixtures through validation and
record the results.

## Completion evidence

- [`manifest/versions.json`](../manifest/versions.json) revision 1 pins 50
  candidate inputs from primary sources. It maps every Section 4 component to
  them and lists 3 deferred items with owners and reasons.
- [`manifest/schemas/versions.schema.json`](../manifest/schemas/versions.schema.json)
  requires a kind-specific integrity anchor and one of four lifecycle states.
- [`ci/validate_versions.py`](../ci/validate_versions.py) rejects missing
  hashes, moving references, and broken license-matrix links, and verifies a
  local cache with `--verify-cache DIR`.
- [`ci/test_validate_versions.py`](../ci/test_validate_versions.py) runs the
  positive fixture, the three invalid fixtures in
  [`ci/fixtures/versions/`](../ci/fixtures/versions/), and 18 further
  failure-path cases.
- [`docs/version-manifest.md`](../docs/version-manifest.md) documents the
  contract, selection rule, commands, and open items.
- License matrix revision 3 resolves the link checker to lychee and adds the
  Node.js, jsonschema, and Docker-maintained Actions components.
- Verification, 2026-10-18:
  - `python3 ci/validate_versions.py` validated the manifest, and
    `--check-links` reached every evidence URL.
  - `python3 -m unittest discover -s ci -p 'test_*.py'` passed.
  - `--verify-cache` matched four real downloads: the image block map, the Syft
    inventory, the markdownlint-cli2 package, and the jsonschema wheel. It
    failed as expected on a tampered copy.
  - Ubuntu snapshot InRelease signatures verified against the Ubuntu archive
    keyring.
- Not done here: TI archives were not downloaded, pending Task 005 and the
  maintainer's confirmation of terms. No pin is hardware-qualified.
