# SDOS 2026 licensing and distribution policy

Status: proposed for maintainer decision  
Task: [003 — Audit licensing and redistribution](../tasks/003-licensing-and-redistribution.md)  
Machine source: [`manifest/license-matrix.json`](../manifest/license-matrix.json)  
Evidence: [`docs/research/task-003-licensing-evidence.md`](research/task-003-licensing-evidence.md)

This policy decides how third-party inputs may enter Git, containers, CI,
caches, course images, student workflows, and releases. It is a technical
distribution control. An institutionally authorized person remains responsible
for accepting vendor terms and approving any interpretation of a non-open
license.

## Recommended decision

Accept the following model for the public monorepo:

1. Keep project source, build recipes, checksums, manifests, and the fully open
   `oss-base`, `dev`, `ci`, `docs`, and image-builder layers public.
2. Keep TI SDK archives, C6000/C7000 installers, installed compiler trees,
   license-acceptance state, and proprietary MISRA text out of Git, public OCI
   images, public caches, logs, and release artifacts.
3. Acquire the pinned TI layer from TI on an ephemeral authorized runner or an
   institution-controlled cache only after the exact terms and manifest are
   recorded. The project does not publish a TI toolchain container, even where
   one component's license may permit an unmodified TI-device-use copy.
4. Give untrusted fork jobs no secrets, credentials, accepted-license state, or
   access to restricted caches. TI builds may run there only if Task 005 proves
   that the exact pinned packages can be fetched and used directly, unattended,
   and within their terms. Failure is an architecture blocker because the
   project has no self-hosted runner fallback.
5. Let students obtain TI development packages from TI under any presented
   terms. Institutional redistribution is limited to exact components whose
   included license permits it, with all notices and TI-device-use conditions.
6. Admit firmware, libraries, and Linux-image files to a release through an
   allowlist built from the exact installed and post-link manifests. No parent
   archive, SDK, image, or compiler receives a blanket license conclusion.
7. Obtain or confirm lawful institutional staff access to MISRA C:2012 and
   Amendments 1–3. Keep the text staff-only. Public CI uses project-owned rule
   IDs, qualified fixtures, and project messages; it never loads or emits
   proprietary guideline text. Separate MISRA permission is required before
   headline text is loaded into a shared tool.
8. For the first course release, distribute SDOS deltas, packages, and
   scripts applied to the official hash-pinned Beagle image. Publish a
   complete derived image only after its Gate 6 license, SPDX, and source
   closure passes.
9. Deliver copyleft corresponding source as a permanent, release-adjacent
   source archive keyed to each published container or image digest, with its
   notices and build scripts. Mutable upstream URLs are not sufficient.

This model makes the public build and test foundation usable immediately while
preserving a fail-closed boundary around licensed inputs. It can later admit a
private TI cache or more redistributable TI components through a matrix revision
without changing the public repository or student API.

## Layer and lane contract

| Layer or lane | Permitted content | Excluded content |
|---|---|---|
| Git repository | Project source, recipes, hashes, schemas, open-source code when vendoring is justified, license notices | TI installers/archives, installed TI tools, credentials, accepted-license state, proprietary MISRA text |
| Public OSS containers | Pinned Ubuntu packages and named open build/test/quality tools after SBOM, notice, source, and scan checks | TI SDK/toolchains, C7x proprietary libraries, MISRA materials |
| Public fork PR | Read-only source, open tools, public dependencies, mocks, static validation; TI compile only after Task 005 proof | Secrets, restricted cache restores, license acceptance, physical boards |
| Trusted GitHub lane | Public image inputs, firmware allowlists, release metadata; licensed TI use only after Task 005 and institutional approval | Any payload whose exact terms or access boundary remain unresolved |
| Institution-controlled layer | Exact TI archives and installed trees when approved, hashed, access-controlled, and manifest-complete | Public or student-wide access beyond the applicable terms |
| Course image/release | Project outputs and exact third-party components with license mapping, notices, source duties, SBOM, hashes, and provenance | Toolchain installers, development SDK archives, unknown-license files |
| Staff-only materials | MISRA documents and any separately licensed headline text | Git, CI, student images, reports, public artifacts |

GitHub Actions cache is not automatically private enough for a restricted
payload. A public-repository workflow can execute contributor-controlled code,
and cache contents restored into that job can be copied. The matrix therefore
keeps TI caches unresolved until Task 005 demonstrates the exact fork boundary
and the institution approves the license use.

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
| MISRA materials | Proprietary staff reference | Institutionally licensed access only; never redistributed |
| GitHub services/actions | Service terms plus per-action source licenses | Public fork model; full-SHA pins, least privilege, per-action license record |
| Docker Desktop | Separate subscription terms from the open engine | Tier 2 only after user/institution eligibility is confirmed; installer not mirrored |

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
container, and non-open material marked as repository source.

## Decision and named ownership still required

The recommended model requires two maintainer inputs before Task 003 is `done`:

1. accept or replace the distribution model above, including its two-lane CI
   consequence: fork PRs run the public checks, and TI firmware builds run on
   trusted lanes unless Task 005 proves a lawful fork-safe path; and
2. name the institution role or person authorized to own TI license acceptance
   and MISRA access, and confirm whether MISRA C:2012 plus Amendments 1–3 access
   already exists or must be obtained.

Until then, matrix revision 1 remains `proposed`, Task 003 remains in progress,
and `LIC-004` is a Gate 0 blocker. Task 005 separately owns the measured TI
GitHub-hosted feasibility proof; accepting this policy does not waive that gate.
