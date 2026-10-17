# 018 — Establish the minimal GitHub PR gateway

## Status

pending

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
