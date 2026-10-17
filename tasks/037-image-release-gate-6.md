# 037 — Add image release artifacts, recovery, and decide Gate 6

## Status

pending

## Depends on

025 and 036.

## Plan references

Phase 6, Gate 6, and Sections 6.11 and 7.

## Objective

Prove that a protected pipeline can rebuild, attest, install, and recover the
course image as a release artifact.

## Scope

- Integrate full image builds into main/nightly/release lanes and PR static
  validation into the fast lane.
- Generate checksums, SBOM, license manifest, provenance, release notes, and
  signed release metadata under the accepted signing policy.
- Flash a blank card, boot it, compare its manifest, and run the complete
  available hardware suite.
- Implement and time SD/eMMC recovery, update, and rollback procedures.
- Ensure release publication occurs only from protected refs after gates pass.

## Out of scope

Publishing restricted TI content or requiring a board for ordinary PR checks.

## Maintainer checkpoint

`hardware-ready` and exact-device approval for each destructive media write;
the maintainer decides Gate 6 from the agent's evidence package.

## Acceptance criteria

- Two protected clean builds are reproducible to the documented standard.
- A blank card boots with the expected manifest and passes hardware checks.
- Recovery returns a failed medium/board to health in under 10 minutes or the
  gate records a failure.
- Gate 6 has complete release metadata and an indexed decision.

## Verification and evidence

Archive CI links, hashes, SBOM/provenance/signing output, flash/read-back logs,
board qualification results, recovery timing, and the Gate 6 record.
