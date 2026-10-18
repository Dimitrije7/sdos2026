# 004 — Create the candidate version manifest

## Status

ready

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
