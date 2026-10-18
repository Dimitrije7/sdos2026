# 005 — Prove GitHub-hosted runner feasibility

## Status

done

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

## Completion evidence

- [`docs/research/task-005-feasibility.md`](../docs/research/task-005-feasibility.md)
  records the pre-run facts, the measurements from two GitHub-hosted runs, and
  the CI budget and placement recommendation.
- Run summaries are archived in
  [`release/evidence/task-005/`](../release/evidence/task-005/) for runs
  [39133768293](https://github.com/Dimitrije7/sdos2026/actions/runs/39133768293)
  and [39134691847](https://github.com/Dimitrije7/sdos2026/actions/runs/39134691847).
  They contain no TI payload, license text, or secret.
- Both TI compiler families downloaded anonymously, installed unattended, and
  compiled a Q15 FIR kernel. The SDK subset streamed and verified, and the
  arm64 image-delta install succeeded. The workflow token was refused write
  access, and no OIDC token was offered.
- License matrix revision 4 closes `LIC-002`. TI CI use is allowed on trusted
  lanes, fork pull requests skip TI steps, and TI caching is prohibited.
- Decisions, 2026-10-18:
  1. Asked whether project automation, including fork pull-request jobs, may
     download and install the pinned TI compilers and SDK under the shipped
     terms, with the maintainer as accepting party. Answer: "Actually I do not
     care about forks so do whatever is easiest for you." Applied as: the
     maintainer accepts the shipped no-cost terms for project automation, and
     every workflow event runs the same jobs with no fork-specific handling.
  2. Asked whether the agent may push branch `feasibility/task-005` and run the
     workflow. Answer: "You may push".
- Verification, 2026-10-18: `python3 ci/validate_licenses.py`,
  `python3 ci/validate_versions.py`, and `python3 -m unittest discover -s ci -p
  'test_*.py'` passed after the matrix and manifest revisions.
