# Task 018 gateway evidence, 2026-10-18

Workflow: [`.github/workflows/ci.yml`](../../../.github/workflows/ci.yml).
Required check: `ci-gate`. Run list: [`runs.json`](runs.json). Branch
protection export: [`main-protection.json`](main-protection.json).

| Run | Event | Commit | Result | What it shows |
|---|---|---|---|---|
| [39162211385](https://github.com/Dimitrije7/sdos2026/actions/runs/39162211385) | push to `main` | `5b9b728` | success | First run: container from pins, quality, 12 firmware presets on a hosted runner |
| [39162253190](https://github.com/Dimitrije7/sdos2026/actions/runs/39162253190) | Dependabot pull request #1 | `38de8e8` | failure, blocked | A real broken change: an Action bumped without the manifest pin. Closed unmerged |
| [39163570526](https://github.com/Dimitrije7/sdos2026/actions/runs/39163570526) | push to `main` | `887f658` | failure | TI's server refused Python's user agent with 403; fixed in `f23a259` |
| [39163580416](https://github.com/Dimitrije7/sdos2026/actions/runs/39163580416) | pull request #2 opened | `f69ddb4` | failure | Opening the pull request ran the gateway; same 403 cause |
| [39163867030](https://github.com/Dimitrije7/sdos2026/actions/runs/39163867030) | push to `main` | `f23a259` | success | The fix, with the SDK download on a hosted runner |
| [39163887808](https://github.com/Dimitrije7/sdos2026/actions/runs/39163887808) | pull request #2 new commit | `a92a379` | success | A new commit reran the gateway; about 16 minutes with the SDK download |
| [39165184190](https://github.com/Dimitrije7/sdos2026/actions/runs/39165184190) | pull request #2 new commit | `3806e1c` | failure, blocked | Deliberately wrong FIR expectation: `FAIL moving average with rounding: y[2] = 101, expected 102`; GitHub reported the pull request `BLOCKED` |

## Branch protection on `main`

Applied with `gh api -X PUT repos/Dimitrije7/sdos2026/branches/main/protection`
under the maintainer's choice "Admins may bypass":

- pull requests must pass `ci-gate` from GitHub Actions and be up to date;
- a pull request is required, with no required approval;
- force pushes and deletion are blocked;
- `enforce_admins` is off, so the maintainer and agents acting as the
  maintainer may push directly.

## Dependabot

Vulnerability alerts and automated security fixes are enabled. Routine
version updates are disabled with `open-pull-requests-limit: 0`, by the
maintainer's choice "Close; security-only". The first alerts are three npm
advisories in markdownlint-cli2 dependencies (`braces` high, no fix;
`smol-toml` medium; `katex` low), carried to Task 024 with the Trivy findings.
