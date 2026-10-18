# 005 — Prove GitHub-hosted runner feasibility

## Status

ready

## Depends on

003 and 004.

## Plan references

Phase 0 and Sections 6.4, 6.5, 6.9, and 6.12.

## Objective

Prove that licensed TI builds and the Linux-image workflow can fit the
GitHub-hosted-only execution model before that architecture is frozen.

## Scope

- Measure download size, installed size, memory, runtime, and cache behavior for
  both TI toolchains and the image builder.
- Prove non-interactive authorized installation without exposing credentials to
  PRs or redistributing forbidden payloads.
- Exercise a minimal compiler build and image-builder operation on a standard
  GitHub-hosted runner.
- Recommend PR versus main/nightly job placement and cache policy.

## Out of scope

Self-hosted runners, HIL, the full firmware matrix, or the final image.

## Maintainer checkpoint

If the proof fails, present architecture-compliant alternatives. Adding a
self-hosted runner is outside scope and requires a new architecture decision.

## Acceptance criteria

- Actual GitHub-hosted logs demonstrate both compiler families and the chosen
  image approach within documented limits.
- Secret and licensing boundaries are tested for pull requests from forks.
- A measured CI budget and fallback-free recommendation are recorded.

## Verification and evidence

Archive workflow URLs/log summaries, timings, storage peaks, and tool versions;
ensure no sensitive value appears in logs or artifacts.
