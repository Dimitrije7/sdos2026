# Task 001 policy evidence

Evidence checked: 2026-10-18

Decision status: recommendations for the Task 001 maintainer checkpoint; no
choice in this note is an accepted project policy until the maintainer records
it.

This note answers the policy questions in
[`tasks/001-architecture-course-policy.md`](../../tasks/001-architecture-course-policy.md)
against primary sources. It deliberately labels source-backed facts,
inferences, recommendations, unresolved institutional facts, and provisional
benchmark thresholds. A vendor result measured on a simulator or another J7
board is context, not evidence of BeagleBone AI-64 performance.

## Recommended decision packet

| Choice | Recommendation | Main consequence | Reversibility |
|---|---|---|---|
| Student workstation | Guarantee Ubuntu 22.04 x86-64. Add Windows 11 x86-64 with Docker Desktop/WSL 2 only after a clean-machine course smoke test. Treat Intel macOS, Apple silicon, Windows Arm, and Arm Linux as best-effort until separately proven. Pin the course image to `linux/amd64` Ubuntu 22.04. | This follows TI's validated host and prevents architecture emulation from becoming part of the required path. It narrows the guaranteed student hardware set. | High: another host becomes supported after the same bootstrap/build/test/deploy qualification passes and its support cost is accepted. |
| Student firmware scope | Students implement a versioned algorithm interface that is statically linked with supplied, source-controlled FreeRTOS and bare-metal runtimes. The build still emits a complete remoteproc firmware ELF; students do not replace the runtime, resource table, memory map, IPC service, cache protocol, or lifecycle code in normal labs. | The platform owns the Linux/DSP contract while students still compile and optimize code for all three DSPs and both runtime profiles. | High: an advanced full-firmware track can be added later behind the same artifact and target tests. |
| GitHub topology | Use an institution-owned organization with at least two staff owners. Prefer a public authoritative monorepo and student/group forks with one PR per lab when public work is allowed. Use branches in the authoritative repository only when cohort-wide visibility is acceptable. | Public repositories avoid private Actions quotas and enable protections on GitHub Free, but public forks expose submissions and metadata. | Medium: repository visibility and submission collection can change, but GitHub warns that visibility changes alter fork networks and access. |
| Private submissions | If public work is prohibited, first verify the faculty GitHub Team or Campus entitlement and actual Actions quota. If submissions must be staff-only, use standalone private submission repositories or LMS delivery rather than assuming branches or forks in one network isolate peers. | This introduces submission repositories outside the platform monorepo or an LMS handoff, while the authoritative platform and CI implementation remain in this monorepo. | Medium: the platform repository stays unchanged; course automation and roster mapping change. |
| Language dialects | C99 for all firmware, wire-format, shared, and portable-reference C; C++14 for any DSP C++; C++17 for Linux host tools and tests. Keep the firmware-facing ABI in C. | These are the common dialects supported by the candidate C66x/C71x toolchain family while allowing modern host code. | Medium: raising a dialect requires every pinned compiler and runtime library to pass the full matrix; lowering it can require source changes. |
| Warning policy | Zero project-code compiler or linker warnings. Enable a pinned warning set and promote warnings to errors on GCC, Clang, C6000, and C7000 builds. Keep vendor/generated code in separately reported targets. Every suppression names a reviewed rationale; MISRA suppressions also name a deviation record. | New compiler versions can surface work that must be triaged before adoption. Vendor noise cannot silently weaken project checks. | High: the list evolves through reviewed toolchain updates without changing source interfaces. |
| MISRA policy | Enforce the candidate rule-ID subset below with a pinned open-source Cppcheck capability manifest, compiler checks, and review. Use the phrase **“MISRA C:2012-oriented checks for the declared checked subset”**. Do not claim compliance or certification. | The gate is honest and reproducible, but it does not cover every MISRA directive, rule, process obligation, or tool-confidence activity. | High: coverage can grow by updating the manifest, enforcement map, and licensed-process evidence. |
| Performance policy | Use fixed float32 gain/FIR/FFT workloads, correctness tolerances, warm-kernel and shared-DDR end-to-end profiles, normalized metrics, and relative/streaming thresholds. Do not set absolute cycle ceilings until board baselines exist for every core/runtime pair. | Results remain comparable despite different DSP architectures and expose transport/cache overhead separately from kernel quality. | High: freeze algorithm regression envelopes after Task 033 target qualification and Gate 5 acceptance without changing the workloads. |

## 1. Supported student host operating systems and architectures

### Source-backed facts

- TI's Processor SDK RTOS J721E 11.02 release identifies **x86-64 Ubuntu
  22.04** as the build host it validated for J721E, including C6x and C7x
  FreeRTOS targets. It does not identify Windows, macOS, Arm Linux, or Arm
  Windows as validated build hosts ([TI PSDK RTOS J721E 11.02 release
  notes](https://software-dl.ti.com/jacinto7/esd/processor-sdk-rtos-jacinto7/11_02_00_06/exports/docs/psdk_rtos/docs/user_guide/release_notes.html)).
- Docker Desktop supports Linux containers on x86-64 Windows using WSL 2, and
  Docker's current Windows requirements include supported 64-bit Windows 10 or
  Windows 11, WSL 2.1.5 or later, virtualization, and 8 GB RAM
  ([Docker Desktop for Windows](https://docs.docker.com/desktop/setup/install/windows-install/)).
- Docker Desktop is available for Intel and Apple-silicon macOS. Docker
  documents x86-64/amd64 emulation on Apple silicon, while its multi-platform
  guidance warns that QEMU emulation can be much slower for compilation and
  compression workloads and its UI warns that emulated images can have poor
  performance or fail
  ([Docker Desktop for Mac](https://docs.docker.com/desktop/setup/install/mac-install/),
  [Docker multi-platform builds](https://docs.docker.com/build/building/multi-platform/),
  [Docker image architecture warnings](https://docs.docker.com/desktop/use-desktop/images/)).
- Docker Engine itself supports several Linux architectures, including amd64
  and arm64. That fact does not make TI's x86-64 compiler binaries native on
  arm64
  ([Docker Engine install matrix](https://docs.docker.com/engine/install/)).
- Docker's licensing page says Docker Desktop is free for education, while the
  current Subscription Service Agreement narrows personal educational use and
  contains separate institutional and public-sector conditions. The
  institution must determine which terms govern this course rather than infer
  entitlement from the short product summary
  ([Docker Desktop license summary](https://docs.docker.com/subscription-billing/desktop-license/),
  [Docker Subscription Service Agreement](https://www.docker.com/legal/docker-subscription-service-agreement/)).

### Inferences

- A `linux/amd64` Ubuntu 22.04 container makes the guest userspace consistent,
  but it cannot make an Arm host native to TI's x86-64 tools. Apple-silicon or
  Windows-Arm support would include an emulator in the required build path.
- Windows x86-64 is a reasonable secondary course host because WSL 2 runs the
  same Linux container architecture. It is still a distinct integration target
  for file permissions, bind mounts, line endings, resource allocation, SSH,
  and device discovery, so the repository must qualify it before promising
  support.
- Intel macOS avoids CPU-architecture emulation but still adds a different
  container VM and filesystem boundary. Supporting it without an inventory of
  student demand would expand the course support matrix without helping TI
  tool compatibility.

### Recommendation

Define these support tiers in the student prerequisites:

1. **Guaranteed reference:** Ubuntu 22.04 LTS on x86-64 with Docker Engine.
2. **Guaranteed secondary only after qualification:** Windows 11 x86-64 with a
   current supported Docker Desktop and WSL 2 configuration. Qualification must
   run bootstrap, all portable checks, every DSP build variant, package/deploy
   over SSH, and first-use download/disk/time measurements on a clean machine.
3. **Best effort:** current Intel macOS, Apple-silicon macOS, Windows Arm, and
   Arm Linux. Do not make graded work depend on these paths until each passes
   the same qualification. For Apple silicon and other Arm hosts, test the
   pinned `linux/amd64` image explicitly rather than publishing an arm64 image
   that cannot run the TI tools.

This is a support commitment, not a claim that Docker cannot start elsewhere.
If the student inventory shows that Apple silicon is common and no x86-64 lab
fallback exists, successful amd64-emulated TI builds become a named Gate 0
blocker.

### Unresolved institutional facts

- Student workstation inventory by OS, CPU architecture, RAM, free disk, and
  ability to enable virtualization or install Docker.
- Whether the institution's Docker Desktop use is covered by educational terms
  and whether a centrally managed install is required.
- Proxy, TLS interception, Docker Hub/registry, VPN, and SSH restrictions on
  campus networks.
- Whether the institution can supply an Ubuntu x86-64 lab or remote fallback
  for students whose personal machines are outside the guaranteed tier.

## 2. Full firmware replacement or supplied runtime

### Source-backed facts

- Linux remoteproc loads a remote processor firmware image. Besides ELF
  segments, firmware commonly contains a resource table describing memory,
  trace, virtio devices, vrings, and other resources the host must prepare
  before starting the processor
  ([Linux remoteproc documentation](https://docs.kernel.org/staging/remoteproc.html)).
- TI's J721E Linux IPC documentation assigns firmware names to each C66x and
  C71x remote processor, describes the reserved-memory split for vrings and
  remote code/data, and says the resource table must be at the beginning of the
  remote processor external-memory section
  ([TI J721E IPC guide](https://software-dl.ti.com/jacinto7/esd/processor-sdk-linux-jacinto7/09_02_00_05/exports/docs/linux/Foundational_Components_IPC_J721E.html)).
- TI's PDK IPC library is linked into RTOS applications and supplies the RPMsg,
  vring, and mailbox layers that communicate with Linux
  ([TI J721E IPC application guide](https://software-dl.ti.com/jacinto7/esd/processor-sdk-rtos-jacinto7/09_00_00_02/exports/docs/psdk_rtos/docs/user_guide/developer_notes_ipc.html)).

### Inference

A student who replaces the entire source tree is not changing only an
algorithm. They can change the ELF layout, resource table, linker placement,
RPMsg service identity, shared-memory ownership, cache maintenance, startup,
and failure behavior as one coupled contract. Linux still loads a complete
firmware ELF in either course model; the practical policy choice is who owns
the runtime portions of that ELF.

### Recommendation

Supply a small source-controlled runtime for each core and both FreeRTOS and
bare-metal. Give student code a versioned C interface with bounded descriptors,
explicit sample types and lengths, scratch-memory requirements, initialization,
process, reset, and optional performance-counter hooks. Link the selected
student algorithm into the runtime to produce the complete ELF. Keep resource
tables, linker files, startup, IPC endpoints, shared-buffer/cache ownership,
watchdog/timeout handling, and identity reporting outside the normal student
edit area.

The consequence is less freedom in introductory labs and much stronger
comparability, recovery, and host-side testing. The choice remains reversible:
an advanced lab may expose a full-firmware template after the platform has
artifact inspection and target recovery tests. Such a lab must still satisfy
the same versioned contract on `C66x_0`, `C66x_1`, and `C71x_0` under both
runtime profiles.

### Unresolved institutional facts

- Whether firmware architecture itself is a learning outcome or the course is
  strictly about algorithm implementation and optimization.
- Whether an advanced/full-firmware assignment is required in the first
  semester; the recommended baseline does not require one.

## 3. GitHub ownership, visibility, and submission topology

### Source-backed facts

- GitHub organizations are shared accounts with roles and teams. GitHub
  recommends maintaining more than one owner for continuity
  ([GitHub account types](https://docs.github.com/en/get-started/learning-about-github/types-of-github-accounts),
  [organization ownership continuity](https://docs.github.com/en/organizations/managing-peoples-access-to-your-organization-with-roles/maintaining-ownership-continuity-for-your-organization)).
- A branch belongs to one repository; a fork is a separate repository with its
  own collaboration space. Public forks are public, private forks are private,
  private forks inherit upstream team permissions, and Git data remains in a
  connected repository network
  ([GitHub forks](https://docs.github.com/en/pull-requests/reference/forks)).
- Protected branches are available for public repositories on GitHub Free and
  for public and private repositories on Pro, Team, and Enterprise plans.
  Protection can require PR reviews, CODEOWNER review, current status checks,
  conversation resolution, and restrictions on force pushes/deletion
  ([GitHub protected branches](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches)).
- Organization-level rulesets require Team or Enterprise. Merge queues are
  available to organization-owned public repositories, but private repository
  merge queues require Enterprise Cloud
  ([GitHub rulesets](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/about-rulesets),
  [GitHub merge queues](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/configuring-pull-request-merges/managing-a-merge-queue)).
- Standard GitHub-hosted runners are free in public repositories. Private
  repositories consume plan-specific included minutes and storage before
  usage-based billing. GitHub currently documents 2,000 minutes/500 MB for
  Free organizations, 3,000 minutes/2 GB for Team, and 50,000 minutes/50 GB
  for Enterprise Cloud
  ([GitHub Actions billing overview](https://docs.github.com/en/actions/concepts/billing-and-usage),
  [GitHub Actions plan allowances](https://docs.github.com/en/billing/concepts/product-billing/github-actions)).
- Verified faculty may apply for GitHub Team for teaching or academic research;
  the benefit is application-based. The school-wide Campus Program includes
  Enterprise Cloud/Server and 50,000 Actions minutes, but has institutional
  eligibility and participation conditions
  ([GitHub discounted plans](https://docs.github.com/en/billing/concepts/discounted-plans),
  [GitHub Education for teachers](https://docs.github.com/en/education/about-github-education/github-education-for-teachers/about-github-education-for-teachers),
  [GitHub Campus Program](https://docs.github.com/en/education/about-github-education/use-github-at-your-educational-institution/about-github-campus-program)).
- On fork PRs, GitHub withholds secrets other than a read-only `GITHUB_TOKEN`.
  Public-fork workflows may require maintainer approval; private-fork PR
  workflows run only if the repository or organization enables them. GitHub
  warns that `pull_request_target` has base-repository trust and should not be
  used to execute untrusted PR code
  ([GitHub workflow events and forks](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows),
  [GitHub Actions fork settings](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/enabling-features-for-your-repository/managing-github-actions-settings-for-a-repository),
  [`pull_request_target` security guidance](https://docs.github.com/en/actions/reference/security/securely-using-pull_request_target)).
- GitHub Classroom was retired on 2026-08-28
  ([GitHub Classroom documentation notice](https://docs.github.com/en/education/manage-coursework-with-github-classroom/get-started-with-github-classroom/about-github-classroom),
  [GitHub's retirement announcement](https://github.com/orgs/community/discussions/205975)).

### Inferences

- A public fork/PR model gives untrusted submissions GitHub's read-only fork
  boundary and avoids private-runner quotas. It also publishes student source,
  identity, commit metadata, review discussion, Actions logs, and artifacts.
- Branch submissions require student write access to the authoritative
  repository and are visible to everyone who can read that repository.
- A private fork must not be assumed to isolate students from peers: visibility
  depends on the upstream access model, inherited team permissions, fork
  policy, and repository-network behavior. A two-student dummy-account test is
  required before adopting it for confidential assessment.
- A fork branch can be rewritten. A grading system must record the submitted
  head SHA and timestamp rather than later grade the moving branch tip.

### Recommendation

The default topology should be:

1. Institution-governed organization with at least two continuing staff
   owners; students have no administration rights.
2. Public authoritative monorepo if institutional privacy policy permits.
3. Personal student or group fork, one topic branch and PR per lab. Record the
   PR head SHA and deadline in the LMS; close grading PRs without merging
   student solutions into `main`.
4. `pull_request` workflows with explicit read-only permissions, no secrets or
   restricted licensed inputs, and approval required for all external
   contributors if students are not organization members. A TI build may run
   only after Tasks 003 and 005 prove a license-compatible, credential-free
   path that cannot expose restricted inputs through logs or artifacts.
5. Protect `main` and `release/**` with a PR, one staff/CODEOWNER approval,
   stale-review dismissal, resolved conversations, the aggregate current-code
   check, strict up-to-date checks, and no force push/deletion. Use a merge
   queue only if the selected plan supports it and PR volume justifies it.
6. Keep the university LMS authoritative for roster identity, deadlines,
   submitted SHAs, grades, and any private feedback. Do not introduce a
   dependency on a replacement classroom SaaS during bring-up.

If public submissions are forbidden, confirm the Education entitlement and
actual organization settings first. A private monorepo with branches or forks
is acceptable only when peer visibility is allowed and the measured Actions
budget fits. Staff-only submissions require standalone private submission
repositories or LMS delivery; this is a course-topology exception, not a split
of the platform implementation or CI source of truth.

### Unresolved institutional facts

- Whether student source, names, commits, PR discussions, Actions logs, and
  artifacts may be public.
- Whether peers may see one another's work before or after the deadline.
- Existing institution organization ownership, verified faculty Team or Campus
  status, displayed private Actions allowance, billing controls, and license
  seats.
- Cohort size, pushes per lab, retention period, expected monthly runner
  minutes, and whether proprietary TI inputs are needed in any PR check.
- Staff owners, CODEOWNERS, roster-to-GitHub mapping, academic-integrity
  process, and whether the LMS can record immutable commit SHAs.

## 4. Language versions, warnings, and MISRA-oriented checks

### Source-backed facts

- A recent J721E SDK validation matrix (11.00) used C6000 CGT 8.3.7 and C7000
  CGT 5.0.0. The exact Task 004 version manifest still has to pin and verify the
  11.02-compatible pair
  ([TI PSDK RTOS J721E 11.00 TIOVX validation](https://software-dl.ti.com/jacinto7/esd/processor-sdk-rtos-jacinto7/11_00_00_06/exports/docs/tiovx/tiovx_release_notes.html)).
- C6000 CGT 8.3 exposes ISO C99 and C++14 modes; TI documents C++14 support with
  target-specific omissions. C11 support arrived in the later C6000 CGT 8.5
  line, so C11 is not a safe common assumption for the SDK's 8.3 toolchain
  ([C6000 compiler options](https://downloads.ti.com/docs/esd/SPRUI04/changing-the-compiler-s-behavior-with-options-stdz0587618.html),
  [C6000 CGT 8.3 C++14 notes](https://software-dl.ti.com/codegen/esd/cgt_public_sw/C6000/8.3.12/README.html),
  [C6000 CGT 8.5 C11 notes](https://software-dl.ti.com/codegen/esd/cgt_public_sw/C6000/8.5.0.LTS/README.html)).
- The C7000 compiler documentation supports C89, C99, C11, and C++14. It also
  recommends fixed-width standard integer types when representation matters
  across devices
  ([C7000 compiler guide](https://downloads.ti.com/docs/esd/SPRUIG8/index.html),
  [C7000 data types](https://downloads.ti.com/docs/esd/SPRUIG8/data-types-stdz0555922.html)).
- TI compilers provide diagnostic numbering, remarks, per-diagnostic
  reclassification/suppression, and a switch that treats warnings as errors
  ([C6000 diagnostic controls](https://downloads.ti.com/docs/esd/SPRUI04/controlling-diagnostic-messages-stdz0585687.html)).
- GCC documents that `-Wall` and `-Wextra` enable groups rather than every
  warning, `-Wpedantic` diagnoses non-ISO extensions for the selected dialect,
  and `-Werror` promotes warnings to errors
  ([GCC warning options](https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html)).
- Open-source Cppcheck's `misra.py` says it implements MISRA C:2012 checks with
  Amendments 1 and 2, cannot distribute proprietary rule text, and explicitly
  invokes individual rule-ID checkers. Cppcheck's own manual says open-source
  coverage is partial
  ([pinned-source candidate: Cppcheck `misra.py`](https://raw.githubusercontent.com/danmar/cppcheck/main/addons/misra.py),
  [Cppcheck manual](https://cppcheck.sourceforge.io/manual.pdf)).
- MISRA's freely published compliance guidance says Mandatory guideline
  violations are never permitted, Required violations need approved
  deviations, and Advisory violations must be identified but do not always
  require deviations. It specifies the information in a deviation record and
  requires identifiable affected locations
  ([MISRA Compliance:2020](https://misra.org.uk/app/uploads/2021/06/MISRA-Compliance-2020.pdf)).

### Recommended language and warning policy

- Compile firmware, shared contracts, and portable-reference C as strict C99:
  `--c99` on C6000/C7000 and `-std=c99` on GCC/Clang. Do not use a GNU dialect
  in portable interfaces.
- Compile DSP C++ as C++14 only where C is insufficient. Compile Linux host
  tools/tests as C++17. Keep host/DSP protocol headers valid C99 and expose a C
  ABI from student algorithm modules.
- For project C/C++ under GCC and Clang, begin with `-Wall -Wextra -Wpedantic
  -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Wundef -Werror`; add
  `-Wstrict-prototypes` for C. Pin the exact per-compiler set in CMake because
  warning groups change between releases.
- For C6000 and C7000, enable `--display_error_number`, `--issue_remarks`, and
  `--emit_warnings_as_errors` for compiler and linker invocations. Review the
  emitted remarks and promote a pinned high-risk set by diagnostic ID. Enable
  linker section warnings.
- Apply the zero-warning rule to project source. Compile vendor/generated code
  in isolated targets, retain its diagnostics as artifacts, and prevent its
  suppressions from leaking into project targets. Every suppression must be
  narrow and documented with tool, diagnostic ID, scope, rationale, owner, and
  review date.

### Concrete candidate MISRA checked subset

The initial enforced automatic subset should be the following rule IDs, all of
which are directly invoked by the current open-source add-on source (Rule 22.4
is mapped to a native Cppcheck checker there):

```text
2.2, 2.7
7.1-7.4
8.2, 8.4-8.6, 8.8-8.10, 8.14
9.2-9.5
10.1-10.8
11.1-11.9
12.1-12.4
13.1, 13.3-13.6
14.1, 14.2, 14.4
15.2-15.4, 15.6, 15.7
16.1-16.7
17.1-17.3, 17.7, 17.8
18.4, 18.5, 18.7, 18.8
20.2-20.5, 20.7-20.14
21.3, 21.6, 21.8, 21.14, 21.15, 21.19-21.21
22.4, 22.5, 22.7-22.10
```

This is a **candidate capability intersection**, not a claim that each checker
is sound and complete for its official rule. Task 024 should:

1. pin the Cppcheck release/commit and hash;
2. generate a machine-readable capability list from the pinned add-on's
   `getAddonRules()` plus native Cppcheck mappings;
3. intersect it with the approved list above and fail if a rule disappears or
   changes mapping;
4. test each enabled rule with positive and negative fixtures relevant to this
   codebase;
5. run with the actual defines/includes and explicit C66x and C71x platform
   models, treating missing-configuration diagnostics as gate failures;
6. report all other add-on findings separately until their behavior is
   qualified; and
7. maintain a guideline-enforcement map for compiler, Cppcheck, other tools,
   review, and process-only obligations.

### Deviation process and honest wording

Use a versioned deviation record containing: unique ID and status; rule ID;
exact file/symbol/location or bounded pattern; technical need; rejected
alternatives; correctness/safety/security risk; precautions and tests; runtime
profiles/cores affected; owner; independent technical approver; approval and
expiry/review dates. Source suppression syntax and tool configuration must cite
that ID. Students may propose but not approve a deviation. Mandatory rules are
not deviated; Required-rule deviations require technical-authority approval;
Advisory findings are resolved or explicitly dispositioned. Vendor/adopted
code uses a separate enforcement map and report.

Use this exact public wording:

> Firmware is checked against a declared subset of MISRA C:2012 using pinned
> open-source Cppcheck, compiler diagnostics, project checks, and review. These
> are MISRA-oriented checks; the project does not claim MISRA compliance or
> certification.

### Unresolved facts

- Exact C6000, C7000, GCC, Clang, Cppcheck, and CMake versions selected by Task
  004 and the diagnostics each emits on the initial codebase.
- Whether the institution will license MISRA C:2012 and its amendments for
  maintainers/reviewers and who is the deviation technical authority.
- Whether Task 024 will add a second open tool for rules outside Cppcheck's
  qualified subset; this should expand evidence, not change the wording to a
  compliance claim.

## 5. Gain, FIR, and FFT performance objectives

### Source-backed facts and limits of comparison

- TI's J721E SDK datasheet reports its SDK performance environment with C66x
  at 1.25 GHz and C7x at 1 GHz, caches enabled, release builds, and code/data
  in DDR. The final BeagleBone AI-64 image still has to verify actual clocks,
  governors, memory placement, and cooling
  ([TI J721E SDK datasheet](https://software-dl.ti.com/jacinto7/esd/processor-sdk-rtos-jacinto7/09_00_00_02/exports/docs/psdk_rtos/docs/user_guide/datasheet_sdk_j721e.html)).
- TI's C66x DSPLIB report publishes cycle formulae for vector multiply and FIR
  and cycle counts for small FFTs, but the report identifies a CCS simulator
  and an older compiler in its test configuration. Those values are useful for
  workload design, not TDA4VM acceptance limits
  ([TI C66x DSPLIB test report](https://software-dl.ti.com/jacinto7/esd/processor-sdk-rtos-jacinto7/11_02_00_06/exports/docs/dsplib_c66x_3_4_0_0/docs/DSPLIB_C66x_TestReport.html)).
- TI's C7x DSPLIB performance summary reports warm-run vector and FIR results
  on a J721S2 EVM, and an older FFTLIB summary reports warm bare-metal results
  on a J7AEP EVM. The current FFTLIB notes also make data layout, batch size,
  and L2 fit explicit performance conditions
  ([TI C7x DSPLIB performance summary](https://software-dl.ti.com/jacinto7/esd/processor-sdk-rtos-j721s2/latest/exports/docs/dsplib/docs/user_guide/performance_summary.html),
  [TI C7x FFTLIB performance summary](https://software-dl.ti.com/jacinto7/esd/processor-sdk-rtos-jacinto7/08_05_00_11/exports/docs/fftlib_02_05_00_02/docs/datasheet/performance_summary.html),
  [current FFTLIB release notes](https://software-dl.ti.com/jacinto7/esd/processor-sdk-rtos-jacinto7/latest/exports/docs/fftlib/docs/FFTLIB_release_notes.html)).

**Inference:** an absolute cycle target copied from any of these tables would
mix different silicon, compiler, cache, memory, data-layout, and runtime
conditions. Task 001 should freeze workloads and provisional relative targets.
Gate 4 establishes data-path and transport behavior; Task 033 target
qualification should freeze algorithm regression envelopes for Gate 5.

### Fixed benchmark workloads

All inputs are deterministic and versioned. Use float32 for the first course
release; any later fixed-point variant must declare Q format, rounding,
saturation, and bit-exact golden vectors.

| Kernel | Kernel workload | Correctness gate |
|---|---|---|
| Vector gain | `N = 65,536`, `y[i] = 0.625f * x[i]`; 64-byte-aligned buffers; deterministic impulse, extrema-safe ramp, sine mix, and seeded pseudo-random corpora. Streaming profile uses blocks of 1,024 samples. | Every output satisfies `abs(err) <= 1e-6 + 1e-6 * abs(reference)`. |
| FIR | 64-tap normalized low-pass, 4,096 output samples after deterministic state pre-roll; aligned input/coefficients/output. Streaming profile uses 1,024-sample blocks with state carried between blocks. | Every output satisfies `abs(err) <= 1e-5 + 1e-5 * abs(reference)` against a double-precision host reference; state continuity is checked across block boundaries. |
| FFT | 1,024-point forward complex-float FFT, interleaved real/imaginary layout, fixed twiddle convention; impulse, bin-centered tone, non-bin tone pair, and seeded pseudo-random inputs. Streaming profile uses a 512-sample hop. | Relative complex L2 error `<= 1e-4` and maximum absolute component error `<= 1e-3 * max(1, max(abs(reference)))`; bin order and scaling are part of the golden-vector contract. |

The numerical values above are **candidate course tolerances**. They must be
validated against the pinned portable reference and TI library behavior before
acceptance; a tolerance may be relaxed only with error-distribution evidence,
not to hide an implementation error.

### Measurement protocol

Run the same release-build harness on `C66x_0`, `C66x_1`, and `C71x_0`, under
FreeRTOS and bare metal, reporting each combination separately.

1. Record firmware hash, compiler/version/flags, board/image identity, DSP
   clock/governor, runtime profile, buffer alignment/location, cache state,
   cooling, start/end temperature, and measurement overhead.
2. **Warm kernel profile:** place the documented working set as intended,
   perform 10 untimed warmups, time at least 1,000 invocations with a DSP cycle
   counter, subtract measured empty-harness overhead, and report min, median,
   p95, p99, cycles/sample (gain), cycles/output and cycles/tap (FIR), and
   cycles/point plus cycles/(point·log2(point)) (FFT).
3. **End-to-end profile:** use the real shared-DDR/RPMsg path and explicit cache
   maintenance for at least 200 blocks. Report host-observed and DSP-only p50,
   p95, p99, throughput, timeout/error count, and the difference attributable
   to transport/cache/runtime overhead.
4. Repeat after a cold start and after a 10-minute sustained run. No timeout,
   crash, numerical failure, dropped block, or unexpected thermal throttling is
   permitted.
5. Keep CI host-reference timing as trend data only. Pass/fail DSP performance
   comes from the versioned connected-board suite and release evidence.

### Provisional performance thresholds

These are recommendations to test the harness and teach optimization; they are
not vendor guarantees and should remain labeled **provisional** until Task 033
qualifies them on target and Gate 5 accepts the course baseline:

- Correctness is mandatory before a timing result is accepted.
- On the same core, runtime, workload, memory placement, and compiler settings,
  the optimized vector-gain implementation should be at least **1.5×** faster
  than the supplied portable scalar implementation by median warm-kernel time.
- The optimized FIR implementation should be at least **2.0×** faster than the
  portable scalar implementation by median warm-kernel time.
- The FFT path should either be at least **2.0×** faster than the supplied
  portable radix implementation, or—when it intentionally wraps the pinned TI
  library—have median wrapper-plus-kernel time no more than **1.25×** the same
  library called by the benchmark harness directly.
- For a nominal 48 ksample/s stream, vector gain and 64-tap FIR use 1,024-sample
  blocks (21.333 ms block period) and must complete end to end at p99 within
  **10.667 ms**. A 1,024-point FFT with 512-sample hop has a 10.667 ms hop
  period and must complete end to end at p99 within **5.333 ms**. The 50% margin
  leaves room for the rest of a teaching pipeline and makes the threshold
  independent of unverified peak-cycle claims.
- Apply every threshold separately to `C66x_0`, `C66x_1`, and `C71x_0` under
  both FreeRTOS and bare-metal, and repeat the streaming thresholds with all
  three DSPs active.
- After Task 033 and Gate 5 freeze per-core/per-runtime baselines, fail a
  regression when median kernel time worsens by more than **10%** or p99
  end-to-end time by more than **20%**, unless an approved record links the
  change to correctness, safety, compiler, or memory-contract evidence. Never
  compare C66x and C71x against one another as a pass/fail ratio.

### Unresolved facts before thresholds become policy

- Actual clocks/governors and cycle-counter behavior on the selected image for
  all three DSPs.
- L1/L2/MSMC/shared-DDR placement available to each runtime and the exact cache
  maintenance cost of the final data path.
- Which TI DSPLIB/FFTLIB functions and redistribution terms are available in
  the pinned toolchain layer.
- Board-to-board and thermal variance with the selected cooling solution.
- Whether 48 ksample/s is the desired teaching scenario; changing it changes
  the streaming deadline but not the fixed microbenchmark workloads.

## Gate 0 facts that still require maintainer or institutional answers

The evidence supports recommendations, but Task 001 cannot convert these into
accepted policy without answers to the following:

1. May student work and identity metadata be public, and may peers see one
   another's submissions?
2. Which workstation OS/architectures actually occur in the cohort, and is an
   Ubuntu x86-64 fallback available?
3. Which institution-owned GitHub organization and staff owners will be used,
   and what Education/Campus plan and Actions allowance are visible in its
   settings?
4. Is the course outcome algorithm development within a supplied runtime, as
   recommended, or must the first release teach full DSP firmware ownership?
5. Will the institution obtain the MISRA documents, and who approves
   deviations?
6. Does the course accept the provisional workload, tolerance, speedup, and
   48 ksample/s streaming objectives pending connected-board baselines?
