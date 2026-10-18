# Task 005 GitHub-hosted feasibility evidence

Status: probes written and linted; workflow not yet run  
Task: [005 — Prove GitHub-hosted runner feasibility](../../tasks/005-github-hosted-feasibility.md)

This note separates facts observed before any workflow run from the
measurements the Task 005 workflow will add. Nothing here is a board result.

## Observed before the run

All observations are from 2026-10-18.

| Fact | Value | Source |
|---|---|---|
| Standard public-repository Linux runner | 4 CPUs, 16 GB RAM, 14 GB SSD | [GitHub-hosted runners](https://docs.github.com/en/actions/reference/runners/github-hosted-runners) |
| Arm64 runner for public repositories | `ubuntu-24.04-arm`, same size | same page |
| Job time limit | 6 hours | [Actions limits](https://docs.github.com/en/actions/reference/limits) |
| Cache per repository | 10 GB | same page |
| C6000 CGT 8.3.7 Linux installer | 43,481,471 bytes; anonymous HTTP 200 | header-only request to the pinned URL |
| C7000 CGT 5.0.0.LTS Linux installer | 45,498,859 bytes; anonymous HTTP 200 | header-only request to the pinned URL |
| PSDK RTOS J721E 11.02.01.03 archive | 3,736,939,755 bytes; anonymous HTTP 200 | header-only request to the pinned URL |
| BeagleBone AI-64 IoT image | 1,126,001,648 bytes compressed; 10 GiB logical; 4.57 GiB mapped | pinned block map |
| Repository Actions secrets, variables, environments | none | `gh secret list`, `gh variable list`, environments API |
| Default workflow token permission | read | repository Actions settings |

No TI payload was downloaded for these facts. The header requests read only
the response status and size.

## Licensing boundary

TI serves every pinned TI input anonymously. A fork job therefore needs no
credential and gains nothing that the public cannot already fetch. The
repository holds no secret, so there is nothing for a fork job to leak.

The remaining licensing question is acceptance. Installing and using the TI
tools in a workflow accepts the terms shipped inside each package. Under the
accepted licensing policy, that requires the maintainer's confirmation for
project automation. The C6000 probe and the C7000 probe each record the names
of the license and manifest files shipped in the installed tree, without their
content, so the accepted terms are traceable.

TI payloads stay out of the Actions cache. Pull requests from forks can read
caches created on the base branch, so a cached TI tree would amount to project
redistribution, which the policy reserves for components cleared file by file.

## Probe design

The workflow `.github/workflows/feasibility.yml` runs on pushes to
`feasibility/**`, on manual dispatch, and on pull requests that touch the
probes. Every Action is pinned by commit, the token is read-only, credentials
are not persisted, and no cache is used. Each probe reads its pins from
`manifest/versions.json`, verifies SHA-256 before use, deletes all payloads on
exit, and uploads only a JSON summary.

| Job | Runner | Measures |
|---|---|---|
| Token and identity boundary | `ubuntu-24.04` | A tag-creation attempt is refused and no OIDC token is offered |
| TI c6000 and c7000 compilers | `ubuntu-24.04` | Download time, unattended install, installed size, peak memory, Q15 FIR compile, shipped license file names |
| TI SDK stream extraction | `ubuntu-24.04` | Streaming the 3.7 GB archive through `tar`, extracting only PDK and DSP libraries, hash check, extracted size, disk headroom |
| Image delta on arm64 | `ubuntu-24.04-arm` | Download and verify, sparse expansion with the block map, static kernel, DTB, and DSP firmware facts, `dpkg -i` of a probe package through `chroot` |

The image job installs `bmap-tools` and `device-tree-compiler` from the runner's
own package archive. That is acceptable for a feasibility probe; the production
image builder takes its tools from the pinned container.

Local checks passed on 2026-10-18 with the pinned linters: ShellCheck v0.10.0
on the four probe scripts, and actionlint 1.7.12 and zizmor 1.30.1 in auditor
mode on the workflow.

## Measurements

Pending the first workflow run. The run's URL, the job durations, and the four
JSON summaries are archived under `release/evidence/task-005/` when available.

## Recommendation

Pending measurements. The working hypothesis to test is:

- Fork pull requests run the open gates and, if the maintainer confirms TI
  terms for automation, compile both DSP compiler families from a fresh
  download of about 90 MB.
- The SDK extraction and the image delta job run on `main`, nightly, and
  release, because the SDK stream alone transfers 3.7 GB.
