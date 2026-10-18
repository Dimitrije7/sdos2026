# 005 — Prove GitHub-hosted runner feasibility

## Status

in-progress

## Handoff

- Updated: 2026-10-18, claude.
- State: Probes and workflow are written, linted, and committed locally. Not
  yet run. Running them waits on two maintainer decisions.
- Done:
  - Pre-run facts in
    [`docs/research/task-005-feasibility.md`](../docs/research/task-005-feasibility.md):
    anonymous TI downloads, runner limits, image sizes, and zero repository
    secrets with a read-only default token.
  - [`.github/workflows/feasibility.yml`](../.github/workflows/feasibility.yml),
    four probes under [`ci/feasibility/`](../ci/feasibility/), and
    [`ci/manifest_pin.py`](../ci/manifest_pin.py).
- Next:
  1. Record the maintainer's answers to the decisions below.
  2. If allowed: push branch `feasibility/task-005` without touching `main`,
     watch the run with `gh run watch`, and download the four JSON artifacts.
  3. Archive the run URL, job timings, and summaries under
     `release/evidence/task-005/`. Fill the Measurements and Recommendation
     sections of the research note, then complete the task.
  4. If an install fails, for example the 32-bit C6000 installer needing
     i386 libraries, fix the probe and rerun before concluding.
- Decisions, asked 2026-10-18 and pending:
  1. May project automation, including fork pull-request jobs, download and
     install the pinned TI compilers and SDK under the terms shipped in each
     package, with the maintainer as the accepting party?
  2. May the agent push branch `feasibility/task-005`, which publishes the
     local commits to the public repository, and run the workflow?
- Verification, 2026-10-18: ShellCheck v0.10.0, actionlint 1.7.12, and
  zizmor 1.30.1 in auditor mode reported no findings. Bash syntax checks
  passed for every probe.
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
