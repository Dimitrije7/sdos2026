# SDOS 2026 course architecture and policy

Status: accepted by the maintainer  
Date: 2026-10-18  
Amended: 2026-10-18 by the Task 003 no-cost decision (P-001 Tier 2 and P-005)  
Task: [001 — Resolve architecture and course policy](../tasks/001-architecture-course-policy.md)

This document is the authoritative Phase 0 course-policy baseline. The
maintainer accepted P-001 through P-006 on 2026-10-18. Source-backed facts and
the reasoning behind these decisions are recorded in
[the Task 001 evidence note](research/task-001-policy-evidence.md).

## Accepted decision

P-001 through P-006 form one baseline. The choices keep one reproducible
workflow across all three DSPs and both runtime profiles while limiting the
student-facing surface to code that serves the course learning objectives.

### P-001 — Supported student hosts

Use a tiered support policy:

| Tier | Host | Release obligation |
|---|---|---|
| 1 | Ubuntu 22.04 LTS x86-64 with Docker Engine, Buildx, Compose v2, Git, and Make | Every documented workflow must pass before release. |
| 2 | Windows 11 x86-64 with a supported Docker Desktop and WSL2 configuration, running the same Linux container and repository checkout inside the WSL filesystem | Supported only after the Task 017 clean-host suite passes and Docker Desktop is confirmed free of charge for the cohort, or the open Docker Engine inside WSL2 replaces it; failures block claiming Windows support, not the Linux release. |
| Unsupported for the first release | macOS, Linux on Arm, Windows without WSL2, and other container engines | May be documented as community-tested only after repeatable evidence exists. |

The course SDK container remains Linux/amd64. Students deploy over the network;
ordinary labs do not require passing a serial adapter or removable media into
the development container. The lab must provide access to a Tier 1 machine for
students whose personal computer is outside the supported matrix.

Consequences: the first release has one native toolchain architecture and a
bounded support matrix. Arm Macs and other Arm hosts may emulate the container,
but the project makes no performance or compatibility promise for them.

Reversibility: add a host tier after the same bootstrap, build, test, quality,
deploy-to-mock, and documentation checks pass on a clean machine. This does not
change the firmware or protocol contracts.

### P-002 — Student/runtime boundary

Students implement algorithms and lab-specific control hooks behind a supplied,
versioned runtime interface. Their build still produces a complete firmware ELF
for `C66x_0`, `C66x_1`, or `C71x_0` in either FreeRTOS or bare-metal form; the
ELF links the student module with the course runtime.

The course owns startup, resource tables, linker and reserved-memory contracts,
RPMsg endpoints, shared-buffer ownership, cache maintenance, tracing, crash
records, and recovery. Normal labs do not permit replacing those components.
Artifact validation rejects incompatible firmware before deployment.

Consequences: labs teach DSP algorithms, optimization, scheduling, and measured
behavior without turning platform bring-up into student work. Both runtime
profiles expose the same student algorithm interface.

Reversibility: a later advanced lab may expose complete firmware replacement
behind a separate target and instructor review after it passes the same memory,
resource-table, protocol, and recovery checks. It does not weaken the normal
lab contract.

### P-003 — Repository and submission topology

Transfer the authoritative public monorepo from the current individual account
to an institution-governed GitHub organization with at least two continuing
staff owners. Keep `main` and `release/**` staff-controlled and protected as
specified by the bring-up plan.

Students receive read access and work in personal public forks, with one branch
and pull request per lab. The LMS records the submitted PR head SHA and
timestamp. Grading PRs are closed without merging student solutions into the
authoritative branches. Fork PR jobs use the `pull_request` event, read-only
permissions, no secrets, and no restricted payloads. Licensed TI inputs may be
used there only after Tasks 003 and 005 prove a license-compatible,
credential-free path that cannot expose the inputs through logs or artifacts.

This recommendation explicitly accepts that student source, names, commit
metadata, and review discussion are public. If institutional policy forbids
that, public-fork submission is a Gate 0 blocker. The alternative is a verified
GitHub Education private organization plus a privacy design that states whether
peer visibility is allowed. Branches or forks of one shared private repository
do not provide staff-only isolation.

Consequences: public GitHub-hosted standard runners and repository protection
features remain available without assuming an unverified education entitlement.
Students cannot administer or push directly to authoritative branches.

Reversibility: repository visibility can change before course enrollment after
the private Actions budget, protection features, fork policy, and privacy model
are verified. Moving after student forks exist requires a migration plan and is
therefore a Gate 0 decision rather than a mid-semester change.

### P-004 — Language and diagnostics policy

Use these language modes:

| Source domain | Standard | Reason |
|---|---|---|
| DSP, wire-format, shared, and portable-reference C | ISO C99 | It is the newest common C dialect of the candidate SDK toolchains: C6000 CGT 8.3 and C7000 CGT 5.0, subject to Task 004 pins. |
| DSP C++ and C++ shared with a DSP target | ISO C++14 | Both TI compiler families document C++14 support. |
| Linux host-only C | ISO C17 | The pinned GCC and Clang toolchains can provide it without affecting DSP or portable-reference sources. |
| Linux/portable host-only C++ | ISO C++17 | It is isolated from the DSP ABI, mature in the pinned host toolchain, and sufficient for the planned host interfaces. |

Compile project-owned code in strict standard mode where the target compiler
supports it. Keep TI intrinsics, pragmas, address-space details, and other
extensions inside target adapters. The host/DSP wire contract uses a C99 ABI
and fixed-width integer types rather than compiler-specific C++ layout.

Enable the strongest practical warning set for every compiler and treat every
project-owned warning as an error in debug and release builds. A suppression
must name the compiler diagnostic, have the narrowest possible scope, carry a
rationale, and receive review. Vendor and generated code are isolated and
reported separately; their warnings never justify lowering checks on project
code.

Consequences: shared code uses the C6000 common denominator while host-only
code can use a current standard. Cross-language and cross-compiler boundaries
stay explicit.

Reversibility: a newer shared standard requires successful compilation and ABI
tests across all three cores, both runtime profiles, and both build types. A
host-only standard update is local to the pinned container and host interfaces.

### P-005 — MISRA-oriented policy and deviations

Apply MISRA C:2012 with Amendments 1, 2, and 3 to project-owned firmware C.
Firmware C++ and host code use the diagnostics and static-analysis policy in
P-004; this project does not claim a MISRA C++ profile. Under the Task 003
no-cost decision the project holds no MISRA documents. Checks, deviations, and
course material cite rule IDs and project-written descriptions, and never
reproduce MISRA text.

The initial automatic candidate subset is the following rule-ID intersection:

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

The current open-source Cppcheck engine and `misra.py` add-on directly invoke
checks for these identifiers, but coverage within an identifier can be partial.
Task 024 must take each enforced rule's classification from a lawful no-cost
source; a rule without one stays reported but non-enforcing.
Task 004 pins the analyzer version. Task 024 must generate and commit its exact
machine-readable capability manifest, intersect it with this list, and qualify
each enabled rule with relevant conforming and violating fixtures. A missing or
changed mapping is a gate failure until reviewed. Other rule IDs remain
reported but non-enforcing until qualified. Directives, undecidable rules,
process obligations, and Amendment 3 gaps require compiler checks, other
analyzers, or documented review rather than a Cppcheck claim.

Every finding from the enforced subset in project-owned firmware C fails the
quality gate unless its guideline classification permits disposition or an
active deviation covers it. Mandatory violations are not deviated; Required
violations need approved deviations; Advisory findings are resolved or
explicitly dispositioned. A deviation records:

- a stable ID, guideline ID, status, owner, and approving reviewer;
- exact file/symbol scope and affected tool/compiler versions;
- technical rationale, risk, and why a compliant form is not reasonable;
- compensating controls and linked test/review evidence;
- introduction date, next review or expiry date, and closure evidence.

Broad file-tree suppressions and permanent suppressions without review dates
are prohibited. Pinned vendor/generated code is excluded from project
conformance counts but scanned and reported separately.

Allowed wording is “MISRA-oriented checks” and “checked MISRA C:2012 subset.”
“MISRA compliant,” “MISRA certified,” and equivalent formal claims are
prohibited without a separately approved compliance program and qualified
evidence.

Consequences: the free PR gate is reproducible and honest about its limits.
Manual and process obligations remain visible instead of being implied by a
tool badge.

Reversibility: Task 024 may propose a narrower or broader enforced set from
negative-fixture evidence, but changing it requires the task's maintainer
checkpoint. A commercial analyzer would first require the maintainer to
reverse the no-cost rule in the licensing policy.

### P-006 — Gain, FIR, and FFT performance objectives

Use FP32 canonical workloads with deterministic generated inputs:

| Kernel | Canonical kernel workload | Optimization objective |
|---|---|---|
| Vector gain | 65,536 contiguous samples, out-of-place, constant gain of `0.625f`; 1,024-sample streaming blocks | At least 1.5× the portable scalar implementation on the same core. |
| FIR | 4,096 outputs, 64 taps, contiguous state and coefficients; 1,024-sample streaming blocks with carried state | At least 2.0× the portable scalar implementation on the same core. |
| FFT | 1,024-point complex forward transform, 512-sample streaming hop, with magnitude reported separately | At least 2.0× the portable radix implementation, or wrapper-plus-kernel median no more than 1.25× a direct call to the same pinned TI library. |

The portable scalar reference is an optimized release build, not an
optimization-disabled straw baseline. A performance result is valid only after
the corresponding golden-vector correctness test passes. Candidate FP32
tolerances are `1e-6 + 1e-6 * abs(reference)` per gain sample,
`1e-5 + 1e-5 * abs(reference)` per FIR sample against a double-precision host
reference, and relative complex L2 error at most `1e-4` plus maximum component
error at most `1e-3 * max(1, max(abs(reference)))` for the FFT. Task 033 must
validate these against the pinned implementations before they become release
limits.

Measure kernel cycles on the DSP and end-to-end latency on Linux separately.
Record logical core, runtime profile, build type, compiler and flags, firmware
hash, clock frequency, memory placement, cache state, alignment, input hash,
and whether a vendor library is used. Run at least 10 warmups and 1,000 measured
kernel iterations, plus at least 200 real shared-DDR/RPMsg blocks; report
median, p95, p99, dispersion, and normalized cycles. Repeat after a cold start
and a 10-minute sustained run. Compare like-for-like builds on the same board
and core.

For a nominal 48 ksample/s stream, vector gain and 64-tap FIR have a 21.333 ms
block period and must complete end to end at p99 within 10.667 ms. The FFT has
a 10.667 ms hop period and must complete end to end at p99 within 5.333 ms.
Each target applies individually on all three DSPs and with all three active,
under both FreeRTOS and bare-metal. The speedups and streaming deadlines are
provisional until Task 033 qualifies them on target and Gate 5 accepts the
course baseline. After that acceptance, a median kernel regression over 10% or
a p99 end-to-end regression over 20% fails connected-board qualification unless
an instructor-reviewed record explains the correctness, safety, compiler, or
memory-contract tradeoff. Gate 4 establishes the transport and data-path
baseline; it does not freeze algorithm-performance claims.

These are course acceptance objectives, not peak-throughput claims for the
TDA4VM. Task 033 must replace provisional results with measured per-core and
per-runtime baselines for Gate 5. A target change after students receive lab
material is a course policy change.

Consequences: students have comparable, measurable optimization goals without
guessing absolute silicon limits before bring-up. The gain target recognizes a
memory-bound kernel; FIR and FFT require a material optimized-path result.

Reversibility: workload sizes and thresholds can change before Gate 5 from
connected-board evidence through a reviewed policy update. The measurement
schema remains stable so old and new evidence can be compared.

## Alternatives considered

| Topic | Alternative | Why it is not the baseline |
|---|---|---|
| Hosts | Promise macOS and Arm-host support immediately | The required TI Linux/amd64 toolchain path and emulation cost have not passed the clean-host suite. |
| Firmware | Let every lab replace startup, linker, resource table, and IPC code | It moves Linux/DSP platform bring-up into student work and makes recovery less predictable. |
| Repository | Start private and assume GitHub Education benefits | Approval, Actions capacity, protection features, and the required privacy boundary have not been observed. |
| Languages | Use C11 or newer in shared DSP C | The candidate C6000 CGT 8.3 line documents C99, while the candidate C7000 toolchain adds C11. |
| MISRA | Claim formal compliance from Cppcheck | The open-source add-on is partial and formal compliance needs licensed standards and process evidence. |
| Performance | Set architecture-peak absolute cycle limits now | No course firmware, memory placement, or connected-board baseline exists yet. |

## Phase 0 coverage and blockers

| Phase 0 question | Resolution or owner |
|---|---|
| Student hosts | P-001. |
| Student firmware ownership | P-002. |
| Repository visibility and submissions | P-003; public student work is accepted, while the institution organization and its staff owners remain to be named. |
| C/C++, warnings, MISRA, deviations | P-004 and P-005. |
| Connected-board versus PR checks | Existing plan policy remains: physical behavior is agent-run under `tests/target/`; no PR depends on a board. |
| GitHub-hosted feasibility | Task 005 must provide measured proof; failure is an architecture blocker because there is no self-hosted fallback. |
| Redistribution | [`docs/licensing-policy.md`](licensing-policy.md), accepted 2026-10-18 with a no-cost-only constraint. |
| Hardware inventory | Task 002 owns the BOM and observed inventory. |
| Version pins | Task 004 owns immutable versions and hashes after Tasks 001 and 003. |
| Gain/FIR/FFT objectives | P-006. |

The maintainer's acceptance resolves the Task 001 policy choices. Gate 0
remains blocked until the institution-governed repository destination and
staff owners are named, and Tasks 002–005 provide their required evidence.
