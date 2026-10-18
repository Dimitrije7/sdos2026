# 018 — Establish the minimal GitHub PR gateway

## Status

in-progress

## Handoff

- Updated: 2026-10-18, claude.
- State: The gateway workflow and its checks pass locally. Pushing `main`, a
  test pull request, and branch protection wait for the maintainer.
- Done:
  - `.github/workflows/ci.yml`: the section 6.3 triggers, `contents: read`,
    cancellation of superseded pull-request runs, timeouts, a `quality` job that
    runs `bootstrap`, `quality`, and the board-host build through the facade,
    and an always-reporting `ci-gate`. TI compilers are used only when the run
    is not a pull request from a fork.
  - `.github/CODEOWNERS` and `.github/dependabot.yml` for Actions.
  - `ci/validate_workflows.py` with `ci/test_workflows.py` and the broken
    fixture `ci/fixtures/workflows/unpinned.yml`; `make quality` now runs it
    with actionlint and zizmor.
- Next:
  1. With the maintainer's approval, push `main`, open a pull request with a
     harmless change and a second commit, and confirm both runs and
     `ci-gate`.
  2. Push a deliberately failing commit to that pull request and confirm
     `ci-gate` fails; close it.
  3. Apply the approved branch protection with `gh api`, export the settings
     to `release/evidence/task-018/`, and complete the task.
- Decisions: pending. Asked 2026-10-18 for approval to push and for the
  branch-protection policy.
- Verification, 2026-10-18: `make quality` passed in the TI image with 97
  Python tests, four validators, hadolint, actionlint, zizmor ("No findings"),
  ShellCheck, 4 host presets, and 12 firmware presets.

## Depends on

016 and 017.

## Plan references

Sections 6.2–6.4 and 6.9.

## Objective

Make every PR creation and update run an always-reporting, GitHub-hosted,
least-privilege gateway that delegates work to repository commands.

## Scope

- Add `pull_request` opened/synchronize/reopened/ready-for-review,
  `merge_group`, protected push, manual, and scheduled triggers as appropriate.
- Add an initial fast lane and stable aggregate `ci-gate` job.
- Pin Actions by full SHA, set explicit permissions, cancellation, timeouts, and
  safe fork behavior.
- Add CODEOWNERS/Dependabot and configure branch protection through an
  agent-executed, auditable GitHub API/CLI step when access exists.
- Add workflow lint/security checks and a deliberate failing fixture.

## Out of scope

Self-hosted runners, `pull_request_target` execution of PR code, or board tests.

## Maintainer checkpoint

The maintainer grants or approves repository administration access if required;
the agent performs and records configuration changes.

## Acceptance criteria

- Creating a PR and pushing a new commit both rerun the required pipeline.
- `ci-gate` always reports and blocks an intentionally broken change.
- Ordinary contributors cannot bypass the accepted protected-branch policy.

## Verification and evidence

Validate workflows locally, inspect permissions/pins, and archive GitHub run
links plus exported branch-protection settings.
