# 040 — Freeze and publish the semester release

## Status

pending

## Depends on

039.

## Plan references

Phase 8, Gate 8, and the definition of done in Section 11.

## Objective

Create the immutable, supportable semester baseline and its maintenance policy
after every software and connected-board gate passes.

## Scope

- Freeze image, SDK, protocol, student API, examples, and lab handouts by
  version/hash.
- Publish quick start, instructor/architecture guides, troubleshooting tree,
  release notes, checksums, SBOM, provenance, evidence index, and recovery media.
- Establish development versus semester-release branches/tags and critical-fix
  policy.
- Verify CODEOWNERS, required checks, dependency updates, action pins, retention,
  and archive restoration.
- Schedule the next post-course version evaluation without moving the frozen
  baseline.

## Out of scope

Feature additions after freeze, silent dependency updates, or releasing with an
unaccepted gate.

## Maintainer checkpoint

`maintainer-decision`: approve the exact release candidate and critical-fix
policy from the complete evidence index. The agent performs publication and
archive verification after approval.

## Acceptance criteria

- Every Section 11 definition-of-done item links to passing evidence.
- Published sources and artifacts reproduce/identify each other by hash.
- A restore rehearsal proves the archived repository, containers/tool inputs,
  image, documents, and recovery media are usable.
- Gate 8 is signed off and the semester baseline is immutable.

## Verification and evidence

Run the release workflow, verify downloads/checksums/attestations and archive
restoration, then store the final Gate 8 record and evidence index.
