# 018 — Establish the minimal GitHub PR gateway

## Status

done

## Completion evidence

- `.github/workflows/ci.yml` runs on the section 6.3 triggers with
  `contents: read`, timeouts, cancellation of superseded pull-request runs, and
  safe fork behavior: fork pull requests run without TI. Its `quality` job only
  calls the facade; `ci-gate` always reports.
- `ci/validate_workflows.py` requires every Action to match its manifest commit
  and the gateway to keep its triggers and `ci-gate`; `make quality` runs it
  with actionlint and zizmor, and `ci/fixtures/workflows/unpinned.yml` proves
  the rejection. CODEOWNERS names the maintainer.
- [`release/evidence/task-018/summary.md`](../release/evidence/task-018/summary.md):
  pull request #2 ran the gateway when opened and on each new commit; its
  deliberately broken commit failed `ci-gate` and GitHub reported it
  `BLOCKED`. Dependabot pull request #1 was also blocked, by the pin check.
- Branch protection on `main` is applied and exported in
  `main-protection.json`.
- Dependabot: security updates only; three npm advisories in markdownlint-cli2
  dependencies go to Task 024.
- Decisions, 2026-10-18: asked "May I push local main (23 commits ...) to
  github.com/Dimitrije7/sdos2026?", the maintainer chose "Yes, push
  (Recommended)". Asked "How strictly should main be protected?", the
  maintainer chose "Admins may bypass": pull requests must pass an up-to-date
  `ci-gate`, force pushes and deletion are blocked, no approval is required,
  and the maintainer and agents acting as the maintainer may still push
  directly to `main`.
  On 2026-10-18 the maintainer chose "Close; security-only (Recommended)"
  for Dependabot: pull request #1 was closed with an explanation, and routine
  version updates were disabled.

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
