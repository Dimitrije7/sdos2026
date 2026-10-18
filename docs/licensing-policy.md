# SDOS 2026 licensing and distribution policy

Status: accepted by the maintainer  
Date: 2026-10-18  
Task: [003 — Audit licensing and redistribution](../tasks/003-licensing-and-redistribution.md)  
Machine source: [`manifest/license-matrix.json`](../manifest/license-matrix.json)  
Evidence: [`docs/research/task-003-licensing-evidence.md`](research/task-003-licensing-evidence.md)

This policy decides how third-party inputs may enter Git, containers, CI,
caches, course images, student workflows, and releases. It is a technical
distribution control. An institutionally authorized person remains responsible
for accepting vendor terms and approving any interpretation of a non-open
license.

## Accepted decision

The maintainer accepted this model on 2026-10-18 and added a no-cost
constraint: the project owns no licenses, and every input must be free of
charge or open source. Item 10 records that constraint.

1. Keep project source, build recipes, checksums, manifests, and the fully open
   `oss-base`, `dev`, `ci`, `docs`, and image-builder layers public.
2. Keep TI SDK archives, C6000/C7000 installers, installed compiler trees,
   license-acceptance state, and proprietary MISRA text out of Git, public OCI
   images, public caches, logs, and release artifacts.
3. Acquire the pinned TI layer from TI on an ephemeral authorized runner or an
   institution-controlled cache only after the exact no-cost terms and manifest
   are recorded and the maintainer confirms acceptance of those terms for
   project automation. The project does not publish a TI toolchain container, even where
   one component's license may permit an unmodified TI-device-use copy.
4. Give untrusted fork jobs no secrets, credentials, accepted-license state, or
   access to restricted caches. TI jobs run on pushes, same-repository pull
   requests, nightly, and release, and skip fork pull requests. Task 005 proved
   that the pinned packages download anonymously and install unattended on
   GitHub-hosted runners.
5. Let students obtain TI development packages from TI under any presented
   terms. Institutional redistribution is limited to exact components whose
   included license permits it, with all notices and TI-device-use conditions.
6. Admit firmware, libraries, and Linux-image files to a release through an
   allowlist built from the exact installed and post-link manifests. No parent
   archive, SDK, image, or compiler receives a blanket license conclusion.
7. Obtain no MISRA documents. Checks, deviations, and course material use rule
   IDs, project-written descriptions, and qualified fixtures; they never load
   or reproduce MISRA guideline or headline text. A MISRA publication that
   MISRA distributes at no cost may be used after Task 024 records its edition
   and terms.
8. For the first course release, distribute SDOS deltas, packages, and
   scripts applied to the official hash-pinned Beagle image. Publish a
   complete derived image only after its Gate 6 license, SPDX, and source
   closure passes.
9. Deliver copyleft corresponding source as a permanent, release-adjacent
   source archive keyed to each published container or image digest, with its
   notices and build scripts. Mutable upstream URLs are not sufficient.
10. Admit only no-cost inputs. The project buys no license, subscription, or
    paid service. No-cost proprietary inputs, such as the TI SDK, compilers, and
    libraries, remain governed by items 2–6. A paid input may appear in the
    matrix only in the excluded lane with no permitted operation, and the
    validator enforces this.

This model makes the public build and test foundation usable immediately while
preserving a fail-closed boundary around licensed inputs. It can later admit a
private TI cache or more redistributable TI components through a matrix revision
without changing the public repository or student API.

## Layer and lane contract

| Layer or lane | Permitted content | Excluded content |
|---|---|---|
| Git repository | Project source, recipes, hashes, schemas, open-source code when vendoring is justified, license notices | TI installers/archives, installed TI tools, credentials, accepted-license state, proprietary MISRA text |
| Public OSS containers | Pinned Ubuntu packages and named open build/test/quality tools after SBOM, notice, source, and scan checks | TI SDK/toolchains, C7x proprietary libraries, MISRA materials |
| Public fork PR | Read-only source, open tools, public dependencies, mocks, static validation | TI steps, secrets, restricted cache restores, license acceptance, physical boards |
| Trusted GitHub lane | Fresh per-job TI downloads verified by SHA-256, firmware builds, public image inputs, firmware allowlists, release metadata | Any payload whose exact terms or access boundary remain unresolved |
| Institution-controlled layer | Exact TI archives and installed trees when approved, hashed, access-controlled, and manifest-complete | Public or student-wide access beyond the applicable terms |
| Course image/release | Project outputs and exact third-party components with license mapping, notices, source duties, SBOM, hashes, and provenance | Toolchain installers, development SDK archives, unknown-license files |

GitHub Actions cache is not automatically private enough for a restricted
payload. A public-repository workflow can execute contributor-controlled code,
and cache contents restored into that job can be copied. The matrix therefore
prohibits TI payloads in caches, artifacts, and logs. Task 019 may propose
caching an audited BSD-only subset through a matrix revision.

## Artifact conclusions

The machine matrix records all ten operations separately for each artifact.
The main decisions are:

| Artifact | Conclusion | Project handling |
|---|---|---|
| PSDK RTOS/PDK | Mixed BSD, MIT, TI Text/TSPA, and other path-specific terms | Recipe and manifest in Git; archive in authorized acquisition only; file allowlist for outputs |
| PSDK Linux | Mixed filesystem/SDK aggregate and fallback only | Excluded from the baseline; full package/source closure if Gate 2 selects it |
| C6000/C7000 CGT | Mixed compiler/RTS trees with TI and permissive terms | Acquire for authorized builds; never publish compiler payloads; classify linked RTS files before firmware release |
| C66x DSPLIB | Candidate 3.4.0; the J721E manifest says BSD-3-Clause | Public CI, cache, and student use only for a hash-verified, BSD-only extracted tree with its notice; otherwise the authorized TI acquisition path |
| C7x DSPLIB/FFTLIB/MMALIB | TI Text; supplied source and unmodified objects restricted to TI-device use | Controlled acquisition; output distribution only from exact manifest/post-link closure |
| TI Linux firmware | Redistributable repository with per-blob terms in `WHENCE` | Pin required blobs and ship their mapped licenses; never use a blanket firmware license |
| FreeRTOS kernel | MIT, with TI port files classified separately | Public pinned source/build and distribution with notice |
| Beagle Debian image | Mixed aggregate; the companion inventory is Syft JSON, not SPDX, and leaves 178 of 4,161 artifacts without a detected license | Use the hash-pinned official image as input; first release distributes SDOS deltas and scripts; a derived image waits for Gate 6 license and source closure |
| TI rpmsg-char library | BSD-3-Clause per the J721E Linux SDK manifest | Candidate host transport wrapper; pinned public source with notice if Task 027 selects it |
| Code Composer Studio | Free TI limited license for TI devices, plus third-party manifest | Optional maintainer workstation tool; never in CI, containers, images, or releases |
| Linux kernel | GPL-2.0-only with documented per-file exceptions | Ship binaries with exact corresponding source, config, patches, notices, and hashes |
| Ubuntu/tool containers | Mixed open-source package aggregate | Public after the Task 017 installed SBOM, notice, source, and reproducibility gates |
| Cppcheck add-on | GPL-3.0-or-later; no MISRA rule text included | Public, pinned checker with source compliance; project-owned rule IDs and messages only |
| MISRA materials | Proprietary and paid | Not obtained; the project uses rule IDs and project-written descriptions only |
| GitHub services/actions | Service terms plus per-action source licenses | Public fork model; full-SHA pins, least privilege, per-action license record |
| Docker Desktop | Separate subscription terms from the open engine | Tier 2 only where use is free of charge, or replaced by the open Docker Engine inside WSL2; installer not mirrored |

## Required records and enforcement

Every third-party input receives:

- a canonical source, immutable version rule, and SHA-256 or source commit;
- the upstream and installed license/component manifest;
- a decision for download, install, CI use, caching, student access, source and
  binary redistribution, public-container inclusion, course-image inclusion,
  and release distribution;
- an owner and unblock condition for every unresolved permission; and
- release evidence linking the distributed file or linked object back to its
  license, notice, source obligation, and approval.

Unknown permission means prohibited until resolved. A component disappears from
the product rather than being admitted on an assumption. A later package update
reopens its license result when its manifest, terms, or file closure changes.

Validate the matrix and its safety invariants with:

```sh
python3 ci/validate_licenses.py
python3 -m unittest -v ci.test_validate_licenses
```

External source links are checked explicitly because that step needs network
access:

```sh
python3 ci/validate_licenses.py --check-links
```

The validator rejects duplicate artifacts and components, unknown or orphaned
blockers, unresolved conclusions without blockers, credentials or license
acceptance in public PR lanes, proprietary material admitted to a public
container, non-open material marked as repository source, and any input that
is not unconditionally no-cost outside the excluded lane.

## Decision record

On 2026-10-18 the maintainer was asked to accept the distribution model,
including its two-lane CI consequence, and to name who owns TI license
acceptance and MISRA access. The answer was: "1. I accept it 2. We own no
licenses, everything must be free and/or open source".

Consequences:

- There is no institutional license owner. Each person accepts no-cost vendor
  terms for their own downloads. Project automation uses a no-cost proprietary
  input only after the maintainer confirms its exact recorded terms.
- "Free" means free of charge. The TI C6000 and C7000 compilers are no-cost but
  proprietary. They remain admitted as the plan's firmware toolchain for all
  three DSPs.
- The MISRA blocker `LIC-004` is closed by excluding MISRA documents. Matrix
  revision 2 records the decision, the excluded MISRA row, and the cost field.
- Tier 2 Windows support requires no-cost Docker Desktop eligibility or the
  open Docker Engine inside WSL2, tracked by `LIC-008`.

On 2026-10-18, for Task 005, the maintainer accepted the terms shipped with
the pinned TI compilers and SDK for project automation, and said fork pull
requests do not matter. Two GitHub-hosted runs proved unattended use, so
blocker `LIC-002` is closed in matrix revision 4. The evidence is in
[`docs/research/task-005-feasibility.md`](research/task-005-feasibility.md).

On 2026-10-18 the maintainer accepted Gate 0 with condition G0-4. Matrix
revision 5 therefore gates `LIC-001` on publication of any TI-derived artifact
instead of on Gate 0. Task 019 owns its remaining path-by-path inventory. See
[`release/evidence/gate-0/gate-0-record.md`](../release/evidence/gate-0/gate-0-record.md).
