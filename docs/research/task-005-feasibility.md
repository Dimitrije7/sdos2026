# Task 005 GitHub-hosted feasibility evidence

Status: complete; two GitHub-hosted runs passed on 2026-10-18  
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

Both runs used the pinned inputs, passed every job, and ran on runner images
`ubuntu24 20261012.334.1` and `ubuntu24-arm64 20261012.149.1`. Raw summaries are
in [`release/evidence/task-005/`](../../release/evidence/task-005/).

| Run | Commit | Result |
|---|---|---|
| [39133768293](https://github.com/Dimitrije7/sdos2026/actions/runs/39133768293) | `8db559d` | All five jobs passed; the image probe's DTB search was too narrow |
| [39134691847](https://github.com/Dimitrije7/sdos2026/actions/runs/39134691847) | `7ac0fcf` | All five jobs passed with the widened image probe |

| Measurement | Run 1 | Run 2 |
|---|---|---|
| C6000 8.3.7 download / unattended install | 15 s / 5 s | 7 s / 5 s |
| C6000 installed size; install peak memory | 349 MB; 401 MiB | 349 MB; 394 MiB |
| C7000 5.0.0.LTS download / unattended install | 6 s / 4 s | 1 s / 2 s |
| C7000 installed size; install peak memory | 496 MB; 659 MiB | 496 MB; 634 MiB |
| Q15 FIR compile, each compiler | under 1 s | under 1 s |
| SDK 3.7 GB stream with selective extraction | 662 s | 693 s |
| SDK extracted subset | 1.89 GB | 1.89 GB |
| Image download and verify / sparse expansion | 25 s / 62 s | 20 s / 66 s |
| Image allocated after expansion | 4.90 GB | 4.90 GB |
| Probe package `dpkg -i` through `chroot` on arm64 | under 1 s | under 1 s |
| Token tag-creation attempt | HTTP 403 | HTTP 403 |
| OIDC token offered | no | no |
| Free disk at job start, x64 / arm64 | 92 GB / 116 GB | 92 GB / 116 GB |

Each installer ships one manifest that records its license terms:
`C6000CodeGenerationTools_8.3.x_manifest.html` and
`C7000_Code_Generation_Tools_C7000-CGT-5_manifest.html`. These are the terms the
maintainer accepted for automation.

The second run also recorded static facts from the image. Booted-board evidence
comes from Task 009.

- The boot partition and the kernel package carry the same
  `k3-j721e-beagleboneai64.dtb`, SHA-256
  `7733dcee22f206565f480e015a690d45681d1b4ff77d16ee11850058b03f36ac`.
  `extlinux.conf` selects DTBs with `fdtdir /`.
- The DTB requests remote-core firmware `j7-c66_0-fw`, `j7-c66_1-fw`, and
  `j7-c71_0-fw`.
- The image contains no file by those names under `/usr/lib/firmware`. The DSPs
  therefore start only after SDOS installs firmware, which Tasks 011 and 013 own.

## Recommendation

GitHub-hosted runners are feasible for every Task 005 workload, so the
architecture needs no fallback. The measured peaks fit even the documented
14 GB disk: about 0.85 GB for both compilers, 1.9 GB for the SDK subset, and
6 GB for the image job. CI must not rely on the larger free space observed.

| Workload | Placement | Budget |
|---|---|---|
| Open gates: lint, unit tests, manifest validators | Every pull request, including forks | Minutes; no TI input |
| Both TI compilers, fresh download and SHA-256 check | Pushes, same-repository pull requests, nightly, release | About 30 s per compiler job |
| SDK subset for firmware builds | Same lanes as the compilers, in parallel with other gates | About 12 minutes of wall time per job, set by TI's download rate |
| Image delta job on `ubuntu-24.04-arm` | Changes under `image/`, plus `main`, nightly, and release | About 2 minutes |

Fork pull requests skip TI steps through a job condition. This follows the
maintainer's direction that forks do not matter, and it keeps the policy that
fork jobs never accept license terms.

Caching: no TI payload in Actions caches or artifacts. The SDK stream is the
only slow step. Task 019 should audit whether the extracted PDK and C66x DSPLIB
paths are BSD-only. If they are, a matrix revision can admit a cache of that
subset keyed by the archive hash, removing most of the 12 minutes.

Task 005's acceptance criteria are met by:

- the two runs, whose logs and summaries show both compiler families and the
  image approach within limits;
- the token-boundary job and the zero-secret repository configuration; and
- the budget and placement above.

Fork pull requests from another account were not exercised. The maintainer
ruled them out of scope, and TI steps skip them by design.
