# 025 — Complete the software CI lanes and evidence framework

## Status

pending

## Depends on

018, 022, 023, and 024.

## Plan references

Sections 6.3, 6.4, 6.9, 6.11, and 6.13–6.14.

## Objective

Implement coherent PR, main, nightly, and release orchestration for every
currently available software artifact while preserving one local command path.

## Scope

- Parallelize the required PR fast-lane jobs behind stable `ci-gate`.
- Add clean main builds, nightly numerical/model stress, and release metadata
  scaffolding.
- Cache only immutable checksummed inputs and set bounded artifact retention.
- Publish compiler manifests, maps, tests, coverage, scans, hashes, and
  provenance inputs with an indexed evidence format.
- Keep explicit extension points for the later full image and target evidence.

## Out of scope

Physical-board jobs, premature image publication, or build logic duplicated in
workflow YAML.

## Maintainer checkpoint

None after repository policy is accepted.

## Acceptance criteria

- PR updates run the full available software matrix and stable aggregate gate.
- Main/nightly/release events select the documented workloads.
- Cancellation, timeouts, permissions, caching, and retention match policy.
- A deliberately broken case in every claimed lane fails as designed.

## Verification and evidence

Run workflow linters and representative GitHub executions for PR, main,
nightly/manual, and release dry-run events; archive run links and summaries.
