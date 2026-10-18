# BeagleBone AI-64 DSP Course Platform — Bring-up Plan

Status: draft for execution | Updated: 2026-10-18

## 1. Outcome

Deliver a versioned, reproducible BeagleBone AI-64 teaching platform on which a
student can:

1. boot a supplied embedded Linux image;
2. build C/C++ FreeRTOS and bare-metal firmware for both C66x DSPs and the
   C71x DSP;
3. load, start, stop, and inspect that firmware without changing the kernel or
   device tree;
4. exchange commands and bulk sample buffers between a Linux application and a
   selected DSP;
5. validate numerical results and measure latency, throughput, and DSP load;
6. reproduce the development environment with Docker and run locally the same
   builds and tests used by CI;
7. receive automatic quality feedback on every pull request and every new
   commit to that pull request;
8. recover a board to a known-good state within one laboratory session.

The platform is the course product. Linux and multicore bring-up are
agent-executed platform engineering, not a student exercise.

### Execution model

Agents execute the bring-up from this repository and the connected development
PC: research, implementation, downloads, builds, flashing, serial capture,
SSH/remoteproc interaction, testing, evidence collection, and documentation.
The maintainer's role is to:

- connect the board, USB-UART, network, and removable media when a task reaches
  a hardware-ready checkpoint;
- approve a destructive operation only after the agent has positively
  identified its exact device target;
- decide architecture, licensing, course-policy, and release gates from an
  agent-prepared recommendation and evidence package;
- oversee results rather than execute procedures or type project commands.

There is no automated HIL service and no self-hosted runner. "Connected-board"
tests are interactive agent-run sessions from the development PC, outside
GitHub Actions. They are reproducible scripts with archived evidence, not a
manual command checklist delegated to the maintainer. The dependency-ordered
work orders live in [`tasks/`](../tasks/README.md).

## 2. Scope

### In scope

- Linux on the two Cortex-A72 cores.
- Student-programmable firmware on:
  - C66x DSP 0;
  - C66x DSP 1;
  - C71x DSP 0.
- FreeRTOS and bare-metal as supported, tested, first-class firmware profiles.
- Linux remoteproc for DSP lifecycle management.
- RPMsg for control and status messages.
- Reserved shared DDR buffers for high-volume sample exchange.
- TI DSP compilers and selected optimized libraries, including C66x DSPLIB and
  C7x DSPLIB/MMALIB where licensing permits course distribution.
- GitHub Actions pipelines for the platform, Linux image, containers, host
  software, both DSP firmware profiles, and student applications.
- Enforced formatting, linting, static analysis, MISRA-oriented checks, unit
  tests, smoke tests, system tests, regression tests, coverage, and artifact
  integrity checks.
- Fully Dockerized command-line development and build workflow, with an
  optional dev-container IDE experience.
- Deterministic image generation, containerized software/system tests,
  documented connected-board checks, and recovery media.

### Not initially in scope

- Teaching U-Boot, kernel, or device-tree porting.
- Student modification of memory carve-outs or system firmware.
- Vision, camera, TIDL, OpenVX, GPU, PRU, or R5F coursework.
- Secure boot or production device security.
- A desktop GUI on the board.
- Real-time analog audio in the first release. The AI-64 has no simple onboard
  analog audio path, so the baseline labs use generated data and WAV files.
  USB audio or a cape can be a later, separately validated extension.

## 3. Proposed architecture

```text
Student workstation
  course SDK container
    TI C6000/C7000 compilers + pinned PSDK RTOS/PDK
    host cross-compiler + build wrappers + tests
             |
             | SSH/deploy
             v
BeagleBone AI-64
  Debian Linux on A72
    student host application
      | RPMsg: commands, descriptors, status
      | shared DDR: sample blocks
      v
    remoteproc + rpmsg_char
      +-- C66x_0 firmware (FreeRTOS or bare metal)
      +-- C66x_1 firmware (FreeRTOS or bare metal)
      +-- C71x_0 firmware (FreeRTOS or bare metal)
```

Important design rules:

- Address DSPs by stable device identity, never by `remoteprocN`; enumeration
  numbers can change between kernels and boot configurations.
- Use RPMsg for the control plane, not for sustained sample traffic.
- Use a versioned wire protocol and explicit buffer ownership for the data
  plane.
- Keep one authoritative memory-map description and generate or validate Linux
  carve-outs, DSP linker sections, and resource tables from it.
- Give students a small `dspctl` interface rather than requiring direct writes
  to remoteproc sysfs nodes.
- Do development from microSD; keep a known-good eMMC/recovery route until the
  image is release-ready.
- Make the Dockerized local commands the source of truth; GitHub Actions must
  call those commands rather than implement a second build system in YAML.
- Require green CI checks and review before merge. Required checks run against
  the PR merge result and are rerun for each new PR commit.
- Use GitHub-hosted runners only. No course-owned CI runner or automated
  physical-board test infrastructure is required.

The detailed delivery model, workflow matrix, test pyramid, free-tool choices,
MISRA limitations, and GitHub-hosted execution model are specified in Section
6 of this plan.

## 4. Candidate version baseline

The versions below are candidates until Gate 2 passes on real hardware.

| Component | Candidate | Reason |
|---|---|---|
| Board image | BeagleBone AI-64 Debian 13.7 IoT, 2026-10-09 | Current board-specific, headless image |
| Linux | BeagleBoard 6.18 K3 kernel | Board DTS includes the standard J721E TI IPC firmware configuration |
| DSP SDK | TI Processor SDK RTOS J721E 11.02.x | Current coherent J721E/TDA4VM firmware and IPC stack |
| C66x compiler | TI C6000 CGT version pinned to the selected PDK | Avoid ABI/tool mismatch |
| C71x compiler | TI C7000 CGT version pinned to the selected PDK | Avoid ABI/tool mismatch |
| Host build environment | Pinned Ubuntu 22.04 x86-64 container | Matches the conservative TI host environment and is reproducible |
| CI | GitHub Actions on GitHub-hosted runners | No course-owned runner; free for standard runners in public repositories |
| Development UX | Docker CLI plus `.devcontainer` | Same pinned environment locally and in CI |

The manifest must record exact versions, download URLs, SHA-256 hashes,
licenses, patches, kernel configuration, DTB hash, and firmware hashes. Do not
silently follow `latest` after the semester baseline is frozen.

## 5. Work breakdown and gates

### Phase 0 — Requirements and lab inventory

Tasks:

- Confirm board revision and SoC silicon revision for all available boards.
- Inventory 5 V / at least 3 A supplies, active cooling, 32 GB or larger SD
  cards, Ethernet, 3.3 V USB-UART adapters, and spare boards/media.
- Decide supported student host OSes. The baseline assumes an x86-64 Linux
  container; Windows/macOS support is through the same container workflow if
  the TI tools permit it.
- Define whether students may replace DSP firmware or only build algorithms
  linked into a supplied runtime.
- Select the GitHub organization/repository visibility model and confirm free
  GitHub Education or public-repository entitlements before depending on
  private-repository branch protection or Actions capacity.
- Define the enforced coding standard, the checked MISRA subset, the deviation
  process, and the explicit statement that free checks are not formal MISRA
  certification.
- Define which target checks require an agent-run connected-board session
  because GitHub-hosted runners cannot access the physical boards.
- Prove that the licensed TI compilers and the complete image build can run
  within GitHub-hosted runner licensing, storage, memory, and time limits; there
  is no self-hosted fallback in the project scope.
- Record redistribution constraints for TI compilers, PDK, libraries, and
  firmware before publishing an image or container.
- Define performance targets for three representative kernels: vector gain,
  FIR, and FFT.

The accepted host, student/runtime, repository, language, MISRA-oriented, and
performance policies are recorded in
[`docs/course-policy.md`](course-policy.md). Its named Gate 0 blockers remain
open until their owning tasks provide evidence.

The reference bench, quantity formulas, qualification checks, and structured
inventory contract are recorded in
[`docs/hardware-bom.md`](hardware-bom.md). The example inventory remains empty
until connected hardware is observed.

The proposed public/open layer, controlled TI layer, release allowlist, and
MISRA-material handling are recorded in
[`docs/licensing-policy.md`](licensing-policy.md). That policy becomes
authoritative after its Task 003 maintainer decision; its named blockers remain
fail-closed inputs to later tasks.

Deliverables:

- requirements document;
- hardware inventory and purchasing list;
- license/redistribution matrix;
- candidate version manifest.

Gate 0: scope, hardware, host OS, and redistribution approach approved.

### Phase 1 — Board and Linux baseline

Tasks:

- Flash the candidate BeagleBoard image to microSD and capture the complete
  serial boot log.
- Verify boot, Ethernet, SSH, storage, system time, clean shutdown/reboot,
  temperature reporting, and fan operation.
- Record U-Boot, kernel, DTB, firmware-package, and board identity information.
- Confirm the required kernel features: remoteproc, K3 DSP remoteproc, virtio,
  RPMsg, rpmsg_char, CMA/reserved memory, debugfs, and remoteproc trace support.
- Create `board-report`, a read-only diagnostic command that collects all
  version and health information needed for support.

Deliverables:

- immutable baseline report and boot log;
- initial `board-report` script;
- known-good recovery SD image.

Gate 1: 20 consecutive cold boots and a two-hour basic soak without kernel,
storage, network, power, or thermal faults.

### Phase 2 — Prove all DSP remote processors

Tasks:

- Map Linux device identities and expected firmware names for both C66x cores
  and the C71x core.
- Audit the effective device tree and reserved-memory map against the TI PDK
  memory map; check for overlap with Linux RAM, CMA, OP-TEE, other remote cores,
  and DMA pools.
- Boot TI's unmodified IPC echo firmware on one DSP at a time, then on all three
  DSPs concurrently.
- Verify firmware load, start, trace, RPMsg enumeration, clean stop, restart,
  crash reporting, and recovery.
- Check whether any preinstalled Edge AI service owns the DSPs and disable
  conflicting services in the course image.
- Run a multi-hour concurrent IPC soak and repeated start/stop test.

Deliverables:

- DSP identity/memory map;
- reproducible remoteproc and IPC smoke tests;
- compatibility report for the exact Beagle Linux + TI PDK combination.

Gate 2 — go/no-go architecture gate: all three DSPs independently and
concurrently boot known firmware, communicate with Linux, and survive the soak
test without carve-out, IOMMU, cache, or restart errors.

Fallback if Gate 2 fails:

1. patch only the board DT/kernel pieces necessary to match the TI 11.02 memory
   and remoteproc contract;
2. if the fix is not small and maintainable, evaluate the matching TI Linux SDK
   with the AI-64 board adaptations;
3. use the older Beagle Edge AI image only as a diagnostic reference, not as the
   long-term course baseline.

### Phase 3 — Containerized CI foundation and reproducible firmware builds

Tasks:

- Pin and automate installation of the PDK, C6000 CGT, and C7000 CGT.
- Create versioned Docker targets for development, CI, firmware builds, host
  builds, quality tools, documentation, and Linux-image construction.
- Add a `.devcontainer` configuration and a documented CLI workflow with the
  same entry points used by CI.
- Add GitHub Actions triggered by PR open/reopen/update, merge queue, main
  branch updates, release tags, manual dispatch, and nightly schedules.
- Establish stable required-check names and protect the default and release
  branches from direct or failing changes.
- Build TI's IPC echo examples from source for `c66xdsp_1`, `c66xdsp_2`, and
  `c7x_1` using the PDK's J721E platform support.
- Separate SoC-common code from any TI EVM assumptions.
- Create minimal FreeRTOS and bare-metal course firmware templates for C66x and
  C71x with:
  - startup plus either a FreeRTOS task or a bare-metal event loop;
  - resource table and RPMsg endpoint;
  - linker command file;
  - trace buffer;
  - exception/crash information;
  - deterministic release and debug builds.
- Add formatting, compiler-warning, lint, static-analysis, MISRA-subset, unit,
  coverage, vulnerability, license, reproducibility, and artifact-hash checks.

Deliverables:

- course SDK container, dev container, and setup flow;
- GitHub Actions PR gateway and protected-branch policy;
- build wrappers and FreeRTOS/bare-metal hello/echo artifacts for all DSPs;
- compiler/linker map archival.

Gate 3: a clean workstation can build byte-identical or functionally identical
firmware, run the same required checks as CI, deploy it, and pass the Phase 2
smoke tests without manual SDK edits. A deliberately broken sample must be
rejected by each applicable gate.

### Phase 4 — Host control and high-throughput data path

Tasks:

- Implement a small, versioned host/DSP protocol with capability negotiation,
  request IDs, timeouts, status codes, and protocol version checks.
- Wrap `ti-rpmsg-char` or the upstream rpmsg-char interface behind a course
  library; avoid exposing transport details to each laboratory exercise.
- Implement shared-buffer descriptors and a strict ownership state machine.
- Implement required DSP cache writeback/invalidate operations and memory
  barriers; document what is and is not hardware-coherent.
- Add buffer bounds checks, sequence numbers, checksums, timeout recovery, and
  a safe firmware restart path.
- Measure control latency, bulk throughput, and CPU/DSP overhead for one and
  three active DSPs.

Deliverables:

- `libsdos_dsp` host library;
- common DSP-side runtime;
- `dspctl list|load|start|stop|status|trace|run|recover`;
- integrity, stress, and benchmark tools.

Gate 4: sustained bidirectional transfers pass data-integrity tests on all DSPs
individually and concurrently, including after deliberate timeout and firmware
crash tests.

### Phase 5 — Student-facing DSP SDK and examples

Tasks:

- Provide one-command configure/build/test/deploy/run workflows.
- Document the full Docker workflow from clone through PR, CI diagnosis,
  deployment, debugging, and recovery; validate every documented command in
  CI.
- Add golden-vector tests that run on the host before hardware deployment and
  compare DSP results with tolerance appropriate to the algorithm.
- Develop examples in increasing complexity:
  1. hello/echo and cycle counter;
  2. vector gain;
  3. FIR with scalar and optimized implementations;
  4. block FFT and magnitude spectrum;
  5. streaming overlap-save or multirate processing;
  6. optional workload split across both C66x cores and the C71x core.
- Teach compiler reports, alignment, restrict qualifiers, SIMD/vector types,
  memory placement, cache behavior, and library use.
- Provide comparable A72 scalar and optimized DSP measurements. Add NEON only
  if it supports a course learning objective.
- Provide optional Code Composer Studio/JTAG maintainer notes; JTAG must not be
  required for ordinary student labs.

Deliverables:

- FreeRTOS and bare-metal C66x/C71x starter projects;
- host application template;
- examples, tests, profiling helpers, and reference results;
- stable student API with semantic versioning.

Gate 5: a pilot student can go from a clean workstation to a verified FIR or
FFT result on each DSP using the student guide alone.

### Phase 6 — Reproducible course image

Tasks:

- Express every change to the upstream Beagle image as scripts/packages; do not
  maintain an undocumented hand-edited master SD card.
- Install pinned kernel/DTB/firmware, host tools, libraries, systemd units,
  permissions, examples, health checks, and local documentation.
- Ensure DSPs start with a harmless course service firmware or remain stopped
  in a well-defined state.
- Add `/etc/sdos-release` and a machine-readable version manifest.
- Remove unrelated/conflicting services and credentials; generate per-board
  SSH identity on first boot.
- Produce SD and eMMC-flasher artifacts, checksums, SBOM, release notes, and a
  tested recovery procedure.
- Make the complete image build a versioned container target and GitHub Actions
  pipeline, with provenance and SBOM output where the repository plan permits.

Deliverables:

- image build pipeline and signed/checksummed release artifacts;
- update and rollback procedure;
- instructor recovery media.

Gate 6: a released image can be rebuilt from the repository, installed on a
blank card, and produces the expected manifest and complete hardware-test pass.

### Phase 7 — Connected-board qualification and pilot lab

Tasks:

- Test at least three boards early and the full lab fleet before semester
  freeze.
- Run 30 cold boots, 100 DSP start/stop cycles, an overnight three-DSP soak,
  network interruption tests, storage-full tests, and abrupt-power-loss tests.
- Establish performance and temperature envelopes with active cooling and a
  documented CPU/DSP frequency policy.
- Verify recovery from a corrupt student firmware artifact and from a damaged
  root filesystem.
- Run a complete pilot lab with someone who did not build the platform.
- Turn every failure into an automated pre-lab or health-check test where
  possible.
- Provide one-command connected-board smoke/system/regression scripts that consume
  the exact artifacts produced by GitHub Actions; archive release results in
  this repository.

Deliverables:

- qualification report per board;
- performance baselines and acceptable ranges;
- issue/runbook database;
- release candidate image and SDK.

Gate 7: at least 95% of pilot lab setups complete without instructor
intervention, every board passes the health check, and failed media can be
recovered in under 10 minutes.

### Phase 8 — Semester release and maintenance

Tasks:

- Freeze image, SDK, protocols, and lab handouts for the semester.
- Publish student quick start, instructor guide, architecture guide,
  troubleshooting decision tree, and release notes.
- Keep development and semester-release branches separate.
- Accept only critical fixes during the semester; validate them against the
  complete hardware regression suite.
- Keep required checks, CODEOWNERS, dependency update automation, action SHA
  pinning, and workflow security review enabled throughout the semester.
- Schedule the next version evaluation after the course ends.

Gate 8: release is archived with sources, binary artifacts, hashes, SBOM,
documentation, and recovery media.

## 6. CI, containers, testing, and quality specification

### 6.1 Required build matrix

| Product | Required variants |
|---|---|
| Linux host tools | GCC and Clang; debug and release |
| C66x firmware | DSP 0 and DSP 1; FreeRTOS and bare metal; debug and release |
| C71x firmware | FreeRTOS and bare metal; debug and release |
| Algorithms | Portable host reference plus each applicable DSP target |
| Development image | Pinned Docker image and `.devcontainer` |
| Course Linux image | SD image and eMMC-flasher image |
| Documentation | Student and instructor instructions, links, code snippets |

FreeRTOS and bare-metal support have equal release status. A lab may use only
one profile, but both profiles must continue to build and pass their applicable
tests.

### 6.2 Monorepo and GitHub policy

Use a GitHub organization owned by the institution, not by an individual.
Everything maintained by the project stays in this monorepo:

- Linux image construction;
- Dockerfiles and the development container;
- platform and DSP firmware sources;
- host tools and libraries;
- student applications and lab starter code;
- public tests and connected-board target-test scripts;
- documentation and release evidence;
- all workflows, composite actions, and CI policy under `.github/` and `ci/`.
- agent working agreements, shared project skills, and harness entry points.

Do not create a separate CI, image, or course-support repository. Reusable
workflows remain local. Students work through branches and pull requests in
this repository, or through personal forks when course policy requires them;
the authoritative source and gateway remain here.

AI-assisted development follows the same single-source rule. `AGENTS.md` is
the working agreement, `.opencode/skills/` is the canonical vendored skill
tree, and Codex, Claude, and GitHub integrations point to it rather than
maintaining divergent copies. Every vendored skill must have a pinned upstream
revision, provenance record, and compatible license notice. Harness-specific
agent and prompt files stay thin and defer to `AGENTS.md` and this plan.

Do not design around GitHub Classroom: GitHub retired it on 2026-08-28. Use
branches or forks of this monorepo, organization teams, GitHub APIs/CLI, and
the university LMS instead.

For a zero-cost baseline, prefer making the monorepo public: standard
GitHub-hosted Actions and protected branches are free for public repositories.
If student submissions must be private, confirm GitHub Education organization
benefits, private-repository Actions capacity, and branch-policy features
during Phase 0. Do not assume an entitlement without testing it.

Protect `main` and `release/**` with:

- pull requests required;
- at least one approving review;
- CODEOWNERS review for build, test, workflow, memory-map, and linker changes;
- dismissal of stale reviews after new commits;
- conversation resolution;
- required status checks against the latest commit;
- branch up-to-date or merge queue;
- no force push or deletion;
- no routine administrator bypass.

Students must not administer the authoritative repository. Instructor-owned CI
definitions, test infrastructure, and protected files require instructor
review.

### 6.3 Workflow triggers

Required workflows explicitly support:

```yaml
on:
  pull_request:
    types: [opened, synchronize, reopened, ready_for_review]
  merge_group:
  push:
    branches: [main, "release/**"]
    tags: ["v*"]
  workflow_dispatch:
  schedule:
    - cron: "17 1 * * *"
```

`opened` runs when the PR is created; `synchronize` runs for every new commit
pushed to it. The PR build tests the prospective merge result. `merge_group`
keeps required checks compatible with a future merge queue. Use concurrency
cancellation for superseded PR software runs.

Required check names must be stable and always report. Path filtering belongs
inside jobs or reusable orchestration; a required workflow that never starts
can leave a PR permanently waiting.

### 6.4 Pipeline lanes

#### PR fast lane

Run on every PR update, targeting no more than 15 minutes after caches are
warm. Required aggregate checks:

1. `pr-policy`
   - commit/PR metadata;
   - protected-file and generated-file consistency;
   - no large binary or secret accidentally committed.
2. `format-and-lint`
   - C/C++, CMake, shell, Python, YAML, Markdown, Dockerfile, and Actions.
3. `static-analysis`
   - compiler warnings as errors;
   - clang-tidy and Clang Static Analyzer;
   - Cppcheck and the declared MISRA subset;
   - CodeQL when repository visibility and plan permit.
4. `host-unit-tests`
   - GCC and Clang;
   - debug and release;
   - ASan/UBSan where supported;
   - coverage threshold and golden DSP vectors.
5. `firmware-build-matrix`
   - C66x_0, C66x_1, and C71x_0;
   - FreeRTOS and bare metal;
   - debug and release;
   - linker-map and section-budget validation.
6. `host-build`
   - AArch64 cross-build and package validation.
7. `container-build`
   - clean BuildKit build;
   - image lint and vulnerability scan;
   - tool-version and reproducibility checks.
8. `image-validation`
   - manifest/schema validation;
   - deterministic customization dry-run or cached build;
   - filesystem, partition, and static content assertions.
9. `docs`
   - formatting and links;
   - execute all quick-start shell snippets suitable for CI.
10. `ci-gate`
    - one always-present summary job depending on every applicable check;
    - this is the stable branch-protection check.

Run independent checks in parallel and cache only immutable, checksummed
inputs. PR jobs have read-only repository permissions and no deployment
secrets.

#### Main-branch lane

On every merge:

- repeat the complete software matrix from a clean context;
- build publishable development/CI containers if licensing permits;
- build the full Linux SD image;
- publish short-lived, checksummed artifacts and test reports;
- update continuous software performance history without changing the
  semester baseline automatically.

#### Nightly regression lane

- full image rebuild from uncached or independently verified inputs;
- both firmware profiles for all DSPs;
- mocked remoteproc lifecycle repetition;
- concurrent protocol/shared-buffer model stress;
- host/model fault injection and timeout recovery;
- numerical regression corpus;
- host-side performance, code-size, and memory-budget regression;
- dependency, container, and vulnerability rescan;
- documentation link check.

#### Release lane

Agent-run connected-board qualification is performed before creating the
release tag and its evidence is stored under `release/evidence/`. A release additionally
requires:

- the complete automated build, test, and quality matrix;
- reproducibility comparison;
- license manifest and SBOM;
- SHA-256 checksums;
- container and image provenance attestations;
- signed release metadata;
- archived test, compiler, linker-map, coverage, and performance reports.

### 6.5 Container architecture and command contract

Use BuildKit multi-stage Dockerfiles with pinned base-image digests and explicit
tool versions:

| Target | Purpose |
|---|---|
| `oss-base` | Ubuntu, CMake/Ninja, GCC/Clang, Python, linters, and test tools |
| `ti-toolchain` | Pinned PDK and TI C6000/C7000 tools, subject to licensing |
| `dev` | Interactive student environment and clangd |
| `ci` | Non-interactive deterministic build/test environment |
| `image-builder` | Linux image construction with minimal privileges |
| `docs` | Documentation validation/build |

The repository provides:

- `.devcontainer/devcontainer.json` for supporting editors;
- `docker compose` profiles for development, tests, and optional board access;
- CMake presets;
- a stable command facade:

```text
make bootstrap
make configure
make build
make test
make quality
make package
make deploy BOARD=<name>
make target-smoke BOARD=<name>
make image
```

Each command must work from a clean clone and document its inputs, outputs, and
expected duration. `make quality` must reproduce all software PR checks. CI
YAML orchestrates these commands; it must not contain a second build
implementation.

TI license handling is a design gate:

- never commit TI installers or proprietary binary packages;
- publish an open container layer with all redistributable tools;
- add TI tools through a checksummed setup step requiring the appropriate
  license acceptance, or store the completed image in an institutionally
  controlled registry only if TI terms allow it;
- never pass credentials through Docker build arguments;
- maintain an offline institutional cache only after license review.

Before accepting the CI architecture, prove that both TI toolchains can be
obtained and used non-interactively on a GitHub-hosted runner without exposing
credentials to PRs or violating their licenses. There is no self-hosted runner
fallback; failure of that proof is an architecture blocker.

The full image job must fit GitHub-hosted runner storage, memory, and time
limits. Keep fast validation on every PR and reserve the expensive complete
image build for `main`, nightly, and release jobs if necessary. Avoid a broadly
privileged image-builder container; grant only the required devices or
capabilities when loop devices or mounts are unavoidable.

### 6.6 Test model

#### Unit tests

Run without a board and finish quickly:

- DSP algorithm reference implementations;
- protocol encoding/decoding and version negotiation;
- ring/buffer ownership state machines;
- bounds, timeout, error, and malformed-input behavior;
- fixed-point saturation/rounding and floating-point tolerances;
- FreeRTOS-independent modules with OS abstraction mocks.

Use CTest as the common runner, Unity/CMock for embedded C where useful, and
GoogleTest/GoogleMock for C++. Produce JUnit XML for GitHub summaries.

#### Component and integration tests

- host library against a mock remoteproc/RPMsg service;
- firmware services against mocked transport and platform layers;
- package installation in a clean AArch64 root filesystem;
- image filesystem, partition, and manifest checks;
- host reference versus DSP golden vectors.

#### Automated smoke tests

- validate every firmware ELF, resource table, required symbol, segment, and
  memory budget;
- start the host application against a mocked remoteproc/RPMsg backend;
- execute hello/echo, firmware identity, and a small gain/FIR vector for each
  modeled core/profile;
- verify timeout/recovery and image/SDK protocol compatibility;
- install packages in a clean AArch64 root filesystem and run basic commands;
- validate container startup and course-image filesystem contents.

#### Automated software system tests

- Linux host application plus three concurrent mocked DSP services;
- large-buffer, boundary, malformed-input, and ownership-state cases;
- simulated firmware crash, timeout, restart, and protocol-version mismatch;
- image package/service ordering, permissions, update, and rollback logic;
- Dockerized end-to-end lab flow from build through result verification.

#### Automated regression tests

- numerical corpus for FIR, FFT, and streaming examples;
- memory budgets from linker maps and modeled runtime high-water marks;
- code-size and stack regressions;
- portable-reference performance and container build-time trends;
- long-running protocol/shared-buffer model stress.

#### Connected-board target tests

These are agent-run versioned scripts under `tests/target/`, not GitHub Actions
jobs. The maintainer connects the hardware and approves verified destructive
targets; the agent executes the suite and archives its evidence:

- load/start/trace/stop/restart for every real DSP and runtime profile;
- Linux host application plus real firmware and shared DDR;
- all three DSPs concurrently;
- cache-boundary and large-buffer integrity;
- firmware crash, timeout, recovery, and board reboot;
- image boot, networking, storage, service ordering, and permissions;
- installation, update, rollback, and recovery media;
- cycles/block, latency, throughput, CPU/DSP load, DDR bandwidth, thermals,
  overnight soak, and repeated cold boot/restart.

### 6.7 Quality toolchain

All selected tools require a usable free/open-source mode and pinned versions.

| Concern | Proposed tools or policy |
|---|---|
| C/C++ format | `clang-format`; zero-diff gate |
| Compiler diagnostics | GCC/Clang and TI CGT warnings as errors with reviewed suppressions |
| C/C++ lint/static analysis | `clang-tidy`, Clang Static Analyzer, Cppcheck |
| MISRA-oriented C | Cppcheck `misra.py`, project rules, manual deviation log |
| Security analysis | CodeQL for public repositories; Clang/Cppcheck regardless of plan |
| Unit tests | CTest, Unity/CMock, GoogleTest/GoogleMock |
| Host dynamic analysis | ASan and UBSan; LSan where stable |
| Coverage | gcov/llvm-cov plus lcov-compatible reports |
| Complexity | `lizard` thresholds and trend report |
| Shell | `shellcheck` and `shfmt` |
| CMake | `cmake-format`, configure/build warnings, CMake presets |
| YAML/Markdown | `yamllint`, `markdownlint-cli2`, link checker |
| Docker | `hadolint`, BuildKit checks, Trivy |
| GitHub Actions | `actionlint`, `zizmor`, actions pinned by full commit SHA |
| Secrets | `gitleaks` with verified baseline and no broad suppressions |
| Dependencies | Dependabot plus scheduled vulnerability scan |
| Licensing | REUSE/SPDX checks and generated dependency license manifest |
| Supply chain | Syft/SPDX SBOM, checksums, GitHub/Docker provenance attestations |

Initial coverage policy:

- core protocol/buffer libraries: at least 90% line and 80% branch coverage;
- student algorithm modules: at least 80% line and 70% branch coverage;
- no reduction of the accepted baseline without an instructor-reviewed
  rationale;
- report changed-code coverage separately so new untested code cannot hide
  behind legacy coverage.

Coverage is measured on portable host builds. Target-only code is covered by
traceable connected-board checks until a suitable GitHub-hosted simulation or TI
target-coverage mechanism is proven.

### 6.8 MISRA policy

Firmware code targets MISRA C:2012 plus relevant amendments. C++ host tooling
uses an explicit project standard based on compiler warnings, clang-tidy,
CERT-oriented checks, and reviewed safe-C++ rules.

The accepted language modes, initial checked rule-ID subset, deviation process,
and permitted public wording are defined in
[`docs/course-policy.md`](course-policy.md#p-005--misra-oriented-policy-and-deviations).

The open-source Cppcheck MISRA add-on is useful but partial. MISRA rule text is
proprietary and is not shipped by Cppcheck or this repository. Therefore:

- name exactly which rules are automatically checked;
- obtain any required MISRA documents legally;
- give unsupported decidable rules another automated check where possible;
- handle undecidable/process rules through review and a deviation record;
- isolate and report generated/vendor code separately;
- use "MISRA-oriented checks" or "checked subset," never "MISRA compliant" or
  "MISRA certified."

Formal compliance requires licensed standards, qualified tooling, documented
process evidence, and substantially more than a free CI job.

### 6.9 GitHub-hosted-only execution model

The project does not operate self-hosted Actions runners. Every automated job
runs on a standard GitHub-hosted runner and inside the pinned project
containers where practical.

Consequences:

- CI can compile every firmware target but cannot execute C66x/C71x binaries on
  a physical AI-64 board;
- transport, protocol, algorithm, image, and lifecycle behavior use portable
  reference code, mocks, freely usable compiler emulation, and static artifact
  validation;
- real remoteproc, RPMsg, shared-memory/cache behavior, thermals, and physical
  performance remain agent-run connected-board release checks;
- no PR is blocked on access to a physical board;
- target-test scripts and result formats remain in the monorepo so the
  connected-board process is repeatable and can be automated later without
  redesign.

Do not use `pull_request_target` to check out and execute PR code. PR workflows
use read-only permissions and GitHub-hosted ephemeral workers.

### 6.10 Student workflow and documentation

The supported-host tiers, supplied-runtime boundary, and fork/PR submission
model are defined in [`docs/course-policy.md`](course-policy.md).

The documented happy path is:

1. clone this monorepo or the student's fork;
2. run one bootstrap command to build/start the pinned development container;
3. run the host unit tests before changing code;
4. implement the algorithm inside the provided interface;
5. run `make quality test build` locally;
6. push a branch and open a PR;
7. inspect annotations and artifacts from the PR checks;
8. fix failures and push again, automatically rerunning the pipeline;
9. merge only after all automated software gates and review pass.

Student documentation includes:

- supported host OS/architecture and Docker prerequisites;
- first-build download, disk, and time costs;
- repository layout and permitted edit areas;
- FreeRTOS versus bare-metal target selection;
- local equivalents for every CI check;
- reading compiler, linker, static-analysis, coverage, and test reports;
- deployment and board selection;
- debugging without JTAG, optional JTAG notes, and log collection;
- common failure decision tree;
- reset, recovery, and support requests with `board-report` output.

Run documentation commands in CI wherever safe. A quick-start document that no
longer works is a release failure.

### 6.11 Artifact and supply-chain policy

- Pin Docker bases by digest and update them through reviewed Dependabot PRs.
- Pin third-party Actions by full commit SHA; record the readable release in a
  comment.
- Download toolchains and dependencies only from an allowlist and verify
  checksums or signatures.
- Use minimal explicit `GITHUB_TOKEN` permissions per job.
- Never expose secrets to fork/PR build jobs.
- Generate and archive compiler versions, build manifests, linker maps, test
  results, coverage, SBOM, checksums, and provenance.
- Publish release artifacts only from protected tags/branches after all gates.
- Use short retention for PR builds and permanent archival for course releases.
- Prove that a release rebuild does not depend on mutable `latest` tags.

### 6.12 Free-cost strategy

- Standard GitHub-hosted Actions are free for public repositories.
- Public repositories on GitHub Free can use protected branches and artifact
  attestations.
- A private monorepo requires confirmation of included minutes and
  Education/organization features before the course model is frozen.
- Do not make an optional commercial dashboard a merge dependency.
- The TI compiler/SDK license and MISRA documents are the expected non-open
  inputs; handle them through institutional access without unauthorized
  redistribution.
- Keep PR jobs incremental and parallel; reserve full image rebuilds and deep
  analysis for main/nightly/release lanes.

### 6.13 CI implementation order

1. Establish Docker `oss-base`, `dev`, and `ci` targets.
2. Add the stable command facade and CMake presets.
3. Add PR triggers and a minimal always-present `ci-gate`.
4. Add format/lint, host builds, unit tests, sanitizers, and coverage.
5. Add the licensed TI layer and complete core/runtime firmware matrix.
6. Add static analysis, MISRA subset, workflow/container/security checks.
7. Add image builder, static image validation, SBOM, and provenance.
8. Add containerized smoke/system gates and nightly regression history.
9. Add connected-board target-test scripts and a versioned release-evidence format.
10. Validate the student guide from a clean workstation and run a pilot PR.

Each step adds at least one deliberately failing fixture to prove that the gate
rejects the defect it claims to detect.

### 6.14 CI definition of done

- PR creation and every new PR commit trigger the required software pipeline.
- The complete FreeRTOS/bare-metal and C66x/C71x matrix builds cleanly.
- Required checks cannot be bypassed by ordinary contributors.
- `make quality test build` inside the development container matches CI.
- Unit, smoke, system, and nightly regression suites have documented ownership,
  runtime, inputs, outputs, and failure triage.
- All automated jobs run on GitHub-hosted runners; no course-owned runner is a
  prerequisite.
- Physical-board validation is agent-run from the connected development PC and
  required for platform releases, not ordinary student PRs.
- The image and environment build from pinned inputs and emit checksums, SBOM,
  and provenance.
- Free tools do not create misleading compliance claims.
- A new student needs no local toolchain installation beyond Git, Docker, and
  an editor.

## 7. Acceptance test matrix

| Area | Minimum release test |
|---|---|
| Linux | boot, SSH, network, storage, RTC/time, reboot/shutdown |
| Remoteproc | enumerate by device identity; load/start/stop/restart each DSP |
| Concurrent use | C66x_0 + C66x_1 + C71x_0 active together |
| IPC control | echo, timeout, malformed request, version mismatch, recovery |
| Shared data | sizes from one cache line through multi-megabyte blocks; checksum and guard regions |
| Numerical | deterministic gain/FIR/FFT vectors with documented tolerances |
| Performance | cycles, latency, throughput, DSP load, A72 load, DDR bandwidth, temperature |
| Stability | overnight workload, 100 restarts, 30 cold boots |
| Reproducibility | clean SDK build and clean image build from pinned inputs |
| Firmware matrix | FreeRTOS and bare-metal builds for C66x_0, C66x_1, and C71x_0 |
| Unit/quality | host unit tests, sanitizers, coverage, format, lint, static analysis, MISRA subset |
| CI contract | identical local/container and CI entry points; checks rerun on every PR update |
| Connected-board validation | agent-run per-core and concurrent board tests consume exact CI-produced artifacts |
| Supply chain | pinned dependencies/actions, vulnerability scan, SBOM, checksums, provenance |
| Documentation | quick-start commands executed successfully in CI |
| Recovery | reimage and return to health-check pass in under 10 minutes |

## 8. Primary risks

| Risk | Mitigation |
|---|---|
| BeagleBoard Linux and TI PDK version skew | Gate 2 compatibility spike; pin every artifact; preserve a known-good stack |
| TI validates the PDK on the J721E EVM, not the AI-64 | Avoid board-peripheral dependencies; test SoC-common firmware on several AI-64 boards |
| Linux/DSP memory-map mismatch | One source of truth; map validation; linker-map and effective-DT tests |
| RPMsg is too slow for sample streaming | RPMsg control plane plus shared-buffer data plane |
| Cache coherency corrupts shared data | Explicit ownership, cache maintenance, barriers, checksums, stress tests |
| `remoteprocN` numbering changes | Resolve by stable sysfs device name and firmware identity |
| DSP firmware cannot cleanly restart | Graceful shutdown protocol, timeout, tested recovery, board reboot fallback |
| Edge AI firmware/services occupy DSPs | Remove or disable conflicts; health check verifies ownership |
| Compiler/library redistribution is restricted | Legal review; setup downloader with license acceptance if bundling is not allowed |
| Thermal throttling invalidates measurements | Active cooling, fixed benchmark policy, temperature recorded with every result |
| Students brick or misconfigure media | SD-first labs, golden recovery media, automated reimage and health checks |
| Toolchain is too large or fragile | Prebuilt course SDK container, local artifact mirror, offline lab test |
| Free MISRA tooling is incomplete | Enforce a named Cppcheck/Clang subset, track deviations, and never claim formal compliance |
| CI configuration is bypassed or edited | Protected branches, CODEOWNERS, least-privilege tokens, pinned actions, required aggregate checks |
| Free Actions capacity is insufficient for private student work | Validate Education entitlement; keep fast PR gates small; prefer a public monorepo when course policy permits |
| Proprietary TI tools cannot be published in a container registry | Publish the open layer; build the licensed layer locally or in an access-controlled institutional registry after license review |
| TI/image builds exceed GitHub-hosted runner limits | Minimize layers/artifacts, cache immutable inputs, split jobs, and run full image builds on main/release only; treat inability to fit as an architecture blocker |
| CI cannot prove behavior on physical DSPs | Maintain one-command connected-board suites; require recorded target validation for platform releases, but not for every student PR |

## 9. Proposed repository structure

```text
.devcontainer/          editor-independent Docker development entry point
.opencode/skills/       canonical provenance-tracked project agent skills
.codex/, .claude/       agent entry points and links to canonical skills
.github/                Actions, Copilot agent/prompts, templates, CODEOWNERS
AGENTS.md               single working agreement for humans and AI agents
LICENSES/               third-party notices for vendored project tooling
ci/                     reusable local/CI orchestration and quality policy
cmake/                  shared toolchains, presets, warnings, sanitizers
docs/                  architecture, bring-up, student and instructor guides
manifest/              pinned versions, hashes, licenses, SBOM inputs
image/                 deterministic image customization and packaging
toolchain/             Dockerfiles/setup scripts; no unlicensed binary blobs
firmware/common/       protocol, resource table, shared runtime
firmware/c66x/         C66x templates and examples
firmware/c71x/         C71x templates and examples
host/libsdos_dsp/      Linux host library
host/dspctl/           lifecycle/deploy/diagnostic CLI
examples/              end-to-end DSP exercises
labs/                  student applications, starter code, and lab contracts
tasks/                 dependency-ordered agent work orders and gate checkpoints
tests/unit/             portable unit and golden-vector tests
tests/smoke/            containerized artifact and mocked-service confidence tests
tests/system/           containerized host/protocol/image behavior
tests/target/           agent-run connected-board smoke, system, and regression tests
tests/regression/       nightly software, numerical, and static regressions
release/               image/SDK release metadata, not generated large files
```

## 10. Indicative schedule

For one experienced engineer with boards in hand:

| Week | Target |
|---|---|
| 1 | Phase 0 and Linux baseline |
| 2 | All three DSPs running TI IPC firmware; Gate 2 decision |
| 3 | Docker/dev-container foundation and required PR gateway |
| 4 | FreeRTOS and bare-metal builds for all three DSPs |
| 5 | Host library, RPMsg control, shared-buffer prototype |
| 6 | Unit/static-analysis/MISRA pipeline and FIR/FFT examples |
| 7 | Reproducible image pipeline, SBOM, and recovery flow |
| 8 | Connected-board target-test scripts and release evidence format |
| 9 | Board qualification, nightly software regression, and pilot lab |
| 10 | Documentation validation and semester freeze |

Allow additional time for licensing, procurement, or low-level debugging if
Gate 2 exposes a kernel/DT/firmware incompatibility.

## 11. Definition of done

The first course release is done when:

- the supplied image boots reliably on the lab fleet;
- Linux can independently and concurrently run student firmware on both C66x
  cores and the C71x core;
- students can build, deploy, run, verify, and profile a DSP exercise using a
  documented command-line workflow;
- FreeRTOS and bare-metal targets for all three DSPs build and pass their
  automated software tests and documented connected-board release checks;
- the complete development and build environment is Dockerized, versioned, and
  documented, and its local commands are the commands invoked by CI;
- every PR and every subsequent PR commit reruns required build, test, and
  quality gates, and failing checks prevent merge;
- control IPC and bulk sample transfer are robust and recoverable;
- the full image and SDK are reproducible from pinned inputs;
- automated unit, software smoke/system/regression, numerical, and
  static-analysis tests pass;
- scripted connected-board health, performance, recovery, and soak checks pass
  before a platform release;
- licensing and redistribution are documented;
- an agent or operator can restore any board to the course baseline in under
  10 minutes;
- students never need to modify U-Boot, the kernel, or the device tree.

## 12. Immediate next action

Use the dependency graph in [`tasks/README.md`](../tasks/README.md). Tasks
001–003 are initially ready and may proceed without hardware. Execute Tasks
001–015 before committing to the student SDK or final course image. The first
bring-up sequence should end with a short evidence package containing:

- board and silicon identifiers;
- exact image/bootloader/kernel/DTB/firmware versions and hashes;
- serial boot log and effective device tree;
- stable mapping for the two C66x and one C71x remote processors;
- successful echo/trace output from each DSP and all three together;
- memory-map audit;
- restart and soak results;
- a recommendation to freeze the candidate stack or take the documented
  fallback path;
- a skeleton dev container and PR workflow proving that the same diagnostic
  command runs locally and in GitHub Actions.

Do not invest in the student SDK or final image until Gate 2 has passed.

## 13. Planning references

- [BeagleBone AI-64 design and specifications](https://docs.beagleboard.org/boards/beaglebone/ai-64/03-design-and-specifications.html)
- [Current BeagleBoard image catalog](https://github.com/beagleboard/distros/blob/main/os_list.json)
- [Linux AI-64 device tree](https://github.com/torvalds/linux/blob/master/arch/arm64/boot/dts/ti/k3-j721e-beagleboneai64.dts)
- [TI Processor SDK Linux IPC for J721E](https://software-dl.ti.com/jacinto7/esd/processor-sdk-linux-sk-tda4vm/latest/exports/docs/linux/Foundational_Components_IPC_J721E.html)
- [TI Processor SDK RTOS J721E](https://software-dl.ti.com/jacinto7/esd/processor-sdk-rtos-jacinto7/latest/exports/docs/psdk_rtos/docs/user_guide/index.html)
- [TI J721E memory-map guidance](https://software-dl.ti.com/jacinto7/esd/processor-sdk-rtos-jacinto7/11_02_00_06/exports/docs/psdk_rtos/docs/user_guide/developer_notes_memory_map.html)
- [TI C6000 code generation tools](https://www.ti.com/tool/download/C6000-CGT)
- [TI C7000 code generation tools](https://www.ti.com/tool/download/C7000-CGT)
- [GitHub Actions pull-request triggers](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows)
- [GitHub protected branches](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches)
- [GitHub required status checks](https://docs.github.com/en/pull-requests/how-tos/merge-and-close-pull-requests/troubleshooting-required-status-checks)
- [GitHub-hosted runners](https://docs.github.com/en/actions/using-jobs/choosing-the-runner-for-a-job)
- [GitHub Actions billing and usage](https://docs.github.com/en/actions/concepts/billing-and-usage)
- [GitHub artifact attestations](https://docs.github.com/en/actions/how-tos/secure-your-work/use-artifact-attestations/use-artifact-attestations)
- [GitHub Classroom retirement notice](https://docs.github.com/en/education/manage-coursework-with-github-classroom/teach-with-github-classroom/use-autograding)
- [Docker build best practices](https://docs.docker.com/build/building/best-practices/)
- [Docker build attestations](https://docs.docker.com/build/ci/github-actions/attestations/)
- [Clang Static Analyzer](https://clang.llvm.org/docs/ClangStaticAnalyzer.html)
- [clang-tidy](https://clang.llvm.org/extra/clang-tidy/)
- [Cppcheck MISRA add-on](https://cppcheck.sourceforge.io/manual.pdf)
