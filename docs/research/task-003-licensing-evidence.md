# Task 003 licensing and redistribution evidence

**Evidence date:** 2026-10-18
**Scope:** non-project material planned for SDOS containers, GitHub Actions caches,
course images, student downloads, and releases
**Status:** technical licensing evidence for maintainer and institutional review;
it does not accept a license or provide legal representation

## Recommended decision

Keep the monorepo, its history, the `oss-base`, source, tests, manifests, and
course-authored release material public. Put every TI installer, accepted-license
record, proprietary compiler or library, licensed MISRA text, and any container
layer containing those items behind a separately administered, access-controlled
licensed layer. A public fork pull request must run entirely from public source
with a read-only token and no secrets. A trusted branch/tag workflow may obtain
the licensed layer with a short-lived credential only after institutional terms
and each exact component manifest have been approved.

This is the only presently supportable distribution model. The official
[TI J721E 11.02.01.03 manifest](https://software-dl.ti.com/jacinto7/esd/processor-sdk-rtos-jacinto7/11_02_01_03/exports/docs/psdk_rtos/PROCESSOR_SDK_RTOS_J721E_manifest.html)
is a mixed-license inventory, not one license for the SDK. It includes BSD and
MIT source, TI-licensed source/object libraries, and TI firmware. The current
BeagleBoard image is likewise an aggregate. Its published inventory is useful
evidence, but it is neither an SPDX document nor complete license clearance.

The immediate consequence is a gate conflict that the maintainer must resolve:
until public redistribution of the exact TI toolchain layer is approved, fork
PRs cannot build the complete C66x/C71x firmware matrix. They can run the public
host/reference, formatting, tests, analysis, mock, manifest, and image-structure
gates. Trusted main/nightly/release workflows can build the TI matrix from the
controlled layer. Do not weaken the no-secrets-in-forks rule to close this gap.

## How to read the findings

The operation columns deliberately separate rights that are often conflated:

- **Download** covers obtaining an upstream archive or image.
- **Install** covers copying or unpacking it on a workstation or runner.
- **CI** covers executing it on a GitHub-hosted runner.
- **Cache** covers copying it to GitHub Actions cache, a registry, or another
  persistent third-party store.
- **Student** covers how an enrolled student may obtain/use it.
- **Source redist.** and **binary redist.** cover publishing or handing copies
  to others, including inside a public container or course image.
- **Duties** records notice, source, attribution, or component-audit work.

`Allowed` means the cited open-source terms support the operation, subject to
their conditions. `Controlled` means use may be possible but only for an
authorized person and an access-controlled copy. `Blocked` means SDOS must not
perform the operation until the named evidence exists. `N/A` means the artifact
is a hosted service or documentation rather than a distributable payload.

Statements marked **Fact** are directly supported by the cited primary source.
Statements marked **Inference** apply those facts to the proposed SDOS design.

## TI SDK, compilers, libraries, and firmware

The coherent starting point is Processor SDK RTOS J721E **11.02.01.03**. Its
[component table](https://software-dl.ti.com/jacinto7/esd/processor-sdk-rtos-jacinto7/11_02_01_03/exports/docs/psdk_rtos/docs/user_guide/sdk_components_j721e.html)
names C6000 CGT 8.3.7 and C7000 CGT 5.0.0.LTS. The standalone product pages now
offer newer compiler releases, so `latest` is not a compatibility rule:
[C6000-CGT](https://www.ti.com/tool/C6000-CGT) and
[C7000-CGT](https://www.ti.com/tool/C7000-CGT). Task 005 must either pin the
SDK-declared pair or prove a different pair across all targets and retain the
terms and manifest shipped with the selected installers.

| Artifact and exact rule | License evidence | Download | Install | CI execution | Cache | Student access | Source redist. | Binary redist. | Notice/source duties |
|---|---|---|---|---|---|---|---|---|---|
| Processor SDK RTOS J721E 11.02.01.03 aggregate | Mixed, path-by-path [TI manifest](https://software-dl.ti.com/jacinto7/esd/processor-sdk-rtos-jacinto7/11_02_01_03/exports/docs/psdk_rtos/PROCESSOR_SDK_RTOS_J721E_manifest.html) | Controlled; an authorized person may fetch the exact published archive and record TI's hash | Controlled; installation may manifest acceptance | Trusted jobs only after approval | Blocked in GitHub cache; private registry only after approval | Individual TI acquisition/acceptance or institution-controlled access | No aggregate grant; split by component | Blocked for the aggregate until every included path is cleared | Preserve the downloaded manifest and every referenced license; produce a shipped-file inventory |
| PDK 11.02.01 main source, excluding named firmware exceptions | Manifest classifies main PDK as BSD-3-Clause, source and binary | Allowed from official SDK after authorized aggregate acquisition | Allowed under BSD conditions | Allowed in trusted CI; public copy only after separating it from restricted SDK paths | Public cache allowed only for the separated, hash-verified BSD payload | May be public if the separation audit proves no TI-restricted files | Allowed | Allowed | Retain copyright, conditions, disclaimer, and per-path exceptions |
| PDK TIFS/PKA and other embedded TI firmware | TI-TSPA/TI Text rows in the SDK manifest | Controlled | Controlled on TI-supported devices | Trusted CI only | Blocked in public/GitHub cache | Institution-controlled or permitted exact binary bundle | Generally unavailable; do not infer source rights | Conditional for unmodified TI-device-use binaries only where the exact license row grants it | Ship exact TI license and notices; no reverse engineering; audit each filename |
| FreeRTOS Kernel used by PDK/course targets | MIT in the SDK manifest and [upstream kernel license](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/LICENSE.md) | Allowed | Allowed | Allowed | Allowed | Public | Allowed | Allowed | Retain MIT notice; audit separately selected FreeRTOS libraries/demos because the kernel license does not classify unrelated additions |
| C66x DSPLIB 3.4 | SDK manifest classifies it BSD-3-Clause source | Controlled download as part of SDK; separated upstream source may be public if identical | Allowed | Allowed after hash/path audit | Allowed for audited BSD-only copy | Public copy may be provided after audit | Allowed | Allowed | Retain BSD notice; record archive/path/hash |
| C7x DSPLIB (manifest 11.02.00; component page reports 11.00.00.01) | TI Text, source and object, in SDK manifest | Controlled | Controlled | Trusted jobs | Blocked in public/GitHub cache | Institution-controlled unless exact redistribution review permits a course copy | Conditional, only to the extent the exact TI Text row grants source redistribution for TI-device use | Conditional, unmodified/authorized TI-device-use form only | Preserve TI license/notices and use restriction; resolve version disagreement before pinning |
| MMALIB (manifest 11.02.01; component page reports 11.02.00.11) | TI Text, limited source and object, in SDK manifest | Controlled | Controlled | Trusted jobs | Blocked in public/GitHub cache | Institution-controlled unless exact terms authorize distribution | Conditional and limited to supplied source rights | Conditional, exact TI-device-use terms | Preserve TI license/notices; resolve version disagreement and enumerate files |
| C6000 CGT: prefer SDK-declared 8.3.7; current standalone page offers 8.3.15 | Compiler binaries under TI Text plus permissive components; inspect installer manifest. Current reference: [8.3.15 manifest](https://software-dl.ti.com/codegen/esd/cgt_public_sw/C6000/8.3.15/C6000_Code_Generation_Tools_8.3.15_manifest.html) | Controlled; exact historical availability/click-through still unverified | Controlled; installation/copy may accept terms | Trusted jobs only after institutional approval | Blocked in GitHub cache; controlled private layer only if approved | Individual TI acquisition or institution-controlled toolchain | Only components/files whose manifest grants source rights | Blocked for public container until the exact 8.3.7 terms are retained and reviewed | Retain compiler, RTS, third-party notices, manifest, hash, and TI-device restrictions |
| C7000 CGT: prefer SDK-declared 5.0.0.LTS; current standalone page offers 5.0.1.LTS or newer | Compiler binaries under TI Text; RTS is mixed per file. Current reference: [5.0.1.LTS manifest](https://software-dl.ti.com/codegen/esd/cgt_public_sw/C7000/5.0.1.LTS/C7000_Code_Generation_Tools_5.0.1.LTS_manifest.html) | Controlled | Controlled | Trusted jobs only after approval | Blocked in GitHub cache | Individual TI acquisition or institution-controlled toolchain | Per-file only | Blocked for public container pending exact 5.0.0.LTS terms | Retain manifest, TI license, RTS/per-file notices, hash, and device-use conditions |
| Processor SDK Linux J721E 11.02.01.03 aggregate/build inputs | Mixed open source, TI firmware, and third-party entries in the [current TI Linux manifest](https://dr-download.ti.com/software-development/software-development-kit-sdk/MD-U6uMjOroyO/11.02.01.03/ProcessorSDKLinux_J721E_11_02_01_manifest.html); TI also documents a [GPLv3 content warning](https://software-dl.ti.com/jacinto7/esd/processor-sdk-linux-jacinto7/latest/exports/docs/linux/Overview_GPLv3_Disclaimer.html) | Controlled aggregate; public upstream sources can be fetched separately | Controlled aggregate | Trusted image job only after manifest audit | No aggregate in GitHub cache; cache audited source packages separately | Prefer public upstream/Beagle packages; otherwise institution-controlled | Per component under its license | No aggregate publication until installed-content audit and source plan pass | Retain manifest and all notices; provide exact corresponding source for copyleft binaries; satisfy firmware-specific terms |
| `ti-linux-firmware` or any firmware copied from it | `WHENCE` maps filenames to distinct license files; example [TI firmware repository](https://git.ti.com/cgit/processor-firmware/ti-linux-firmware/tree/WHENCE) and [Keystone license](https://git.ti.com/cgit/processor-firmware/ti-linux-firmware/tree/LICENCE.ti-keystone) | Allowed only after resolving each selected filename to `WHENCE` | Allowed under that file's terms | Trusted/public as the resolved license permits | Only if that file's license permits redistribution; otherwise controlled | Exact permitted binary may be bundled for its supported TI device | Usually unavailable for binary-only firmware | Per-file; never infer a repository-wide grant | Preserve the applicable license with every redistributed firmware file and record its hash/path |

**Fact.** TI's [online terms](https://www.ti.com/legal/terms-conditions/terms-of-use.html)
say that software-specific terms control. TI's generic
[download agreement](https://www.ti.com/corp/docs/legal/downloads/LICENSE-AGREEMENT.pdf)
is a fallback when no accompanying terms apply; it must not replace a component
manifest or installer license. Installation, copying, or use can constitute
acceptance under the applicable agreement.

**Inference.** An anonymous URL and a published SHA do not establish that a
GitHub runner, every student, or the public is authorized to accept or
redistribute an installer. SDOS therefore needs an accountable institutional
acceptor, retained terms, and a file-level export inventory before a TI-bearing
container or cache is published.

## BeagleBoard image, Debian, kernel, bootloader, and firmware

The official [BeagleBoard distribution index](https://github.com/beagleboard/distros/blob/main/os_list.json)
currently identifies the planned image as
`bbai64-debian-13.7-iot-v6.18-k3-arm64-2026-10-09-10gb.img.xz`, download SHA-256
`69796038e452c6c5e006031ecab995a0ec734a866176e37a0de38d43b35380cf`,
expanded-image SHA-256
`4b8ca45a631693fc4b45a09d26f1d98fbc378d6de9e2a6c2715364fd0452ea3c`,
and a companion
[published inventory](https://raw.githubusercontent.com/beagleboard/sbom-archive/refs/heads/main/sbom/bbai64-debian-13.7-iot-v6.18-k3-arm64-2026-10-09-10gb.syft.spdx.json.xz).

Local read-only inspection of that inventory found **4,161 artifacts**: 3,200
kernel modules, 680 Debian packages, and 281 artifacts from Python, npm, Go,
Rust, .NET, Java, and workflow metadata. It has 178 artifacts with no detected
license, including 24 Debian packages. Material examples among those 24 are
`bbb.io-kernel-6.18-k3`, `bb-u-boot-beagleboneai64`, `bb-wl18xx-firmware`,
`cnm-firmware`, and `ti-pru-cgt-v2.3`.

Despite the filename suffix, the decompressed file is **Syft native JSON**, not
SPDX JSON: it declares Syft 1.54.1, schema 16.1.11, and the Syft schema URL, and
has `artifacts` rather than an SPDX `packages` document. This is a useful
inventory, but it does not satisfy the plan's SPDX release evidence by itself.
The listed license strings also include non-normalized values and firmware
families. Regenerate a valid SPDX document from the frozen image, inspect every
`NOASSERTION`/empty result, and pair packages with Debian copyright/source data.

| Artifact and exact rule | License evidence | Download | Install | CI execution | Cache | Student access | Source redist. | Binary redist. | Notice/source duties |
|---|---|---|---|---|---|---|---|---|---|
| Frozen BeagleBone AI-64 Debian 13.7 IoT image above | Aggregate of Debian packages, Beagle packages, kernel, bootloader, and firmware; published inventory is incomplete licensing evidence | Allowed for evaluation from official URL with both hashes checked | Allowed on verified removable media/board | Static inspection allowed; full build/flash follows device safety policy | Public cache only after full aggregate audit; prefer immutable official URL plus release mirror approved by counsel | Students may download the official unmodified image | Per included source component | Re-publication of the whole image is blocked until missing licenses and source duties are closed | Archive package list, copyright files, firmware licenses, exact sources, valid SPDX SBOM, hashes, and notices |
| Debian 13.7 packages in the image | Each binary package carries package-specific copyright data; [Debian Policy](https://www.debian.org/doc/debian-policy/ch-docs.html#copyright-information) requires `/usr/share/doc/package/copyright`. `main`, `contrib`, `non-free`, and `non-free-firmware` have different status under the [Debian archive policy](https://www.debian.org/doc/debian-policy/ch-archive.html) | Allowed from pinned Debian/Beagle snapshots | Allowed | Allowed | Allowed for exact packages whose licenses permit redistribution | Public repositories or Beagle image | Per package | Per package | Preserve copyright/license files; for GPL-family binaries provide corresponding source by a compliant method for the exact binary build |
| BeagleBoard Linux 6.18 K3 kernel/package | Kernel source uses GPL-2.0 WITH Linux-syscall-note; see [official BeagleBoard kernel mirror](https://github.com/beagleboard/linux/blob/master/COPYING). The current image inventory did not assign a license to its `bbb.io-kernel` binary package | Allowed source download | Allowed | Build allowed | Source cache allowed; binary cache/release after source mapping | Public | Allowed under GPL-2.0 conditions | Allowed with GPL conditions | Preserve COPYING/notices, configuration and patches; provide complete corresponding source/scripts for exact package |
| U-Boot, device trees, Beagle customizations, image builder | Mixed packages; [Beagle image-builder](https://github.com/beagleboard/image-builder) supplies source and a `LICENSES` tree, while U-Boot is primarily GPL-2.0-or-later with per-file exceptions | Allowed from exact commits | Allowed | Allowed | Allowed after dependency/license inventory | Public | Per repository/file | Per package/file | Retain notices; publish exact commits, patches, build scripts, and corresponding source. Clear the current SBOM's empty Beagle package license fields |
| Beagle/TI/Linux firmware blobs in the image | Per-file license, normally mapped through Debian copyright metadata or firmware `WHENCE`; no blanket kernel/image license | Allowed only per exact file terms | Allowed | As exact terms permit | Only files with redistribution permission | Official image or audited course bundle | Usually unavailable | Per file | Include the exact license alongside each blob and retain filename/hash/origin mapping |

**Blocker.** The current official image may be used as the candidate baseline,
but SDOS must not publish a derived course image until the 24 unclassified
Debian packages, every firmware blob, and copyleft corresponding-source plan are
closed. An SBOM is an inventory aid; it is not permission or a source offer.

## Ubuntu base and public OSS tool/test/quality layer

Canonical permits downloading, installing, and using Ubuntu and redistribution
of unmodified Ubuntu images. Its
[intellectual-property policy](https://ubuntu.com/legal/intellectual-property-policy)
also says a modified distribution using Ubuntu trademarks needs approval or
must remove/replace those trademarks and recompile as appropriate. The
[Docker Official Ubuntu image](https://hub.docker.com/_/ubuntu) is an aggregate:
its components retain their own licenses. SDOS should identify its image as an
SDOS build based on Ubuntu, pin the OCI digest, and avoid presenting it as an
official Ubuntu product.

The following register covers every tool named by the plan. The version column
is intentionally a rule: Task 004 still must select exact versions, package
sources, hashes/digests, transitive dependencies, and the license text found at
that selected revision. A source repository's current license is evidence, not
proof that an older package or vendored subcomponent has the same terms.

For every row marked `OSS profile`, the operation decision is:

- **Download/install/CI/cache/student:** Allowed from a pinned official source;
  public caches must contain no credentials and must use content-addressed keys.
- **Source/binary redistribution:** Allowed under the named license and the
  selected version's third-party terms.
- **Duties:** retain copyright and license texts. Apache-2.0 packages also retain
  any supplied `NOTICE`; GPL/LGPL packages require the applicable corresponding
  source and relinking/library duties when binaries are distributed. An SDOS
  public container is binary distribution, so generate a component manifest,
  copy installed license files, and publish/furnish exact source packages by a
  license-compliant method.

| Planned artifact | Version/source rule and primary license evidence | Profile / material exception |
|---|---|---|
| Ubuntu 22.04 x86-64 OCI base and installed APT packages | Pin manifest-list and platform digest from the [official image](https://hub.docker.com/_/ubuntu); package licenses from `/usr/share/doc/*/copyright` and exact Ubuntu source packages | Aggregate profile: download/install/CI/student allowed; cache and redistribution only after package-level SBOM/notices/source closure; apply Canonical trademark policy |
| Git | Pin Ubuntu package/source; [Git COPYING](https://github.com/git/git/blob/master/COPYING) is GPL-2.0-only | OSS profile; corresponding source for distributed binary |
| GNU Make | Pin Ubuntu package/source; [GNU Make license](https://www.gnu.org/software/make/license.html) is GPLv3-or-later | OSS profile; corresponding source |
| CMake and CTest | Pin Ubuntu package or Kitware release; [CMake copyright/license](https://cmake.org/licensing/) is BSD-3-Clause with separately identified third-party material | OSS profile; retain bundled third-party notices |
| Ninja | Pin Ubuntu package or release; [Ninja COPYING](https://github.com/ninja-build/ninja/blob/master/COPYING) is Apache-2.0 | OSS profile |
| GCC, gcov, libgcc/libstdc++ and sanitizers | Pin Ubuntu source/binaries; GCC is GPLv3-or-later and shipped runtime libraries use the [GCC Runtime Library Exception](https://www.gnu.org/licenses/gcc-exception-3.1.html) where applicable | OSS profile; retain exact runtime/library license mapping and corresponding source |
| LLVM/Clang, clangd, clang-format, clang-tidy, Clang Static Analyzer, ASan/UBSan/LSan, llvm-cov | Pin one LLVM release; [LLVM license](https://github.com/llvm/llvm-project/blob/main/LICENSE.TXT) is Apache-2.0 WITH LLVM-exception, with separately licensed third-party directories | OSS profile; inventory selected subprojects/runtime components |
| Python | Pin Ubuntu package; [Python license history](https://docs.python.org/3/license.html) contains PSF and predecessor terms plus incorporated components | OSS profile; retain the full license history and package dependencies |
| FreeRTOS Kernel | Pin upstream tag/commit matching the PDK compatibility decision; [MIT](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/LICENSE.md) | OSS profile for the kernel only; separately audit optional libraries |
| Unity | Pin upstream commit/release; [source files identify MIT](https://github.com/ThrowTheSwitch/Unity/blob/master/src/unity.c) | OSS profile |
| CMock | Pin upstream commit/release including submodules; [CMock repository/license](https://github.com/ThrowTheSwitch/CMock) is MIT | OSS profile; separately inventory vendored Unity/CException/Ruby dependencies |
| GoogleTest/GoogleMock | Pin one release; [GoogleTest LICENSE](https://github.com/google/googletest/blob/main/LICENSE) is BSD-3-Clause | OSS profile |
| Cppcheck engine and `misra.py` | Pin one release; [Cppcheck COPYING](https://github.com/danmar/cppcheck/blob/main/COPYING) is GPLv3; [misra.py](https://github.com/danmar/cppcheck/blob/main/addons/misra.py) explicitly does not supply MISRA rule text | OSS profile for code; rule text follows the separate MISRA row below |
| lcov-compatible reports / LCOV | Pin selected implementation; [LCOV repository](https://github.com/linux-test-project/lcov) uses GPL-2.0-or-later material | OSS profile; corresponding source. If only gcov/llvm-cov output is used, remove LCOV from the manifest |
| Lizard | Pin PyPI artifact and upstream commit; [package metadata](https://github.com/terryyin/lizard/blob/master/setup.py) identifies MIT | OSS profile; include Python dependency licenses |
| ShellCheck | Pin Ubuntu package/release; [ShellCheck license](https://github.com/koalaman/shellcheck/blob/master/LICENSE) is GPL-3.0 | OSS profile; corresponding source |
| shfmt | Pin release; [shfmt license](https://github.com/mvdan/sh/blob/master/LICENSE) is BSD-3-Clause | OSS profile |
| cmake-format (`cmakelang`) | Pin Python artifact/commit; [upstream repository](https://github.com/cheshirekow/cmake_format) is GPL-3.0 | OSS profile; corresponding source and Python dependency inventory |
| yamllint | Pin Python artifact/commit; [yamllint license](https://github.com/adrienverge/yamllint/blob/master/LICENSE) is GPL-3.0 | OSS profile; corresponding source/dependencies |
| markdownlint-cli2 | Pin npm package, lockfile, and commit; [upstream LICENSE](https://github.com/DavidAnson/markdownlint-cli2/blob/main/LICENSE) is MIT | OSS profile; npm transitive inventory required |
| Link checker | **No implementation is named in the plan** | Blocked until Task 004 selects a product/version/license; do not silently add one to the container |
| hadolint | Pin release/commit; [hadolint LICENSE](https://github.com/hadolint/hadolint/blob/master/LICENSE) is GPL-3.0 | OSS profile; corresponding source and bundled dependency inventory |
| Docker Engine/CLI, Buildx, Compose, and BuildKit | Pin Ubuntu packages/plugins and BuildKit version; [Moby](https://github.com/moby/moby/blob/master/LICENSE), [BuildKit](https://github.com/moby/buildkit/blob/master/LICENSE), [Buildx](https://github.com/docker/buildx/blob/master/LICENSE), and [Compose](https://github.com/docker/compose/blob/main/LICENSE) use Apache-2.0 | OSS profile for these components. Docker Desktop is a different product with separate commercial terms and must not be redistributed by SDOS |
| Trivy | Pin release and vulnerability database policy; [Trivy LICENSE](https://github.com/aquasecurity/trivy/blob/main/LICENSE) is Apache-2.0 | OSS profile; database and downloaded scanner assets need their own hashes/license inventory |
| actionlint | Pin release/commit; [actionlint LICENSE](https://github.com/rhysd/actionlint/blob/main/LICENSE.txt) is MIT | OSS profile |
| zizmor | Pin release/commit; [zizmor LICENSE](https://github.com/zizmorcore/zizmor/blob/main/LICENSE) is MIT | OSS profile; Rust crate dependencies in SBOM |
| gitleaks | Pin release/commit; [gitleaks LICENSE](https://github.com/gitleaks/gitleaks/blob/master/LICENSE) is MIT | OSS profile; scan output must not reproduce live secrets |
| Dependabot | GitHub service plus optional [dependabot-core](https://github.com/dependabot/dependabot-core/blob/main/LICENSE) MIT code | Hosted service: no source/binary distribution when only enabling GitHub Dependabot; generated PRs still pass normal review |
| REUSE tool and SPDX data | Pin Python artifact/commit; [reuse-tool LICENSE](https://github.com/fsfe/reuse-tool/blob/main/LICENSES/Apache-2.0.txt) is Apache-2.0; SPDX identifiers/texts have their own published terms | OSS profile; generated manifest records licenses but does not grant them |
| Syft | Pin release/commit; [Syft LICENSE](https://github.com/anchore/syft/blob/main/LICENSE) is Apache-2.0 | OSS profile; validate the requested output format rather than trusting a filename suffix |
| CodeQL | Use GitHub-hosted code scanning for the public repository under [GitHub CodeQL terms](https://docs.github.com/en/site-policy/github-terms/github-codeql-terms) | CI execution is conditional on those terms. Do not publish or embed the CodeQL CLI/database bundle until its redistribution terms are separately approved; students need no copy for the required local workflow |
| GitHub/Docker provenance actions and all third-party Actions | Select each action explicitly, pin a full commit SHA, and retain its license/dependency inventory; use GitHub's [artifact attestations documentation](https://docs.github.com/en/actions/concepts/security/artifact-attestations) | Hosted execution allowed under GitHub terms; source/binary redistribution only if an action is vendored or packaged, under that action's license. A SHA pin is integrity, not license evidence |

## MISRA document and rule-text access

The open Cppcheck engine/add-on and the MISRA publications are separate
artifacts. `misra.py` can report rule identifiers without supplying the
proprietary guideline text. Its optional rule-text mapping does not create a
right to copy that text. MISRA's own
[Cppcheck licensing guidance](https://forum.misra.org.uk/showthread.php?tid=1551)
says users should obtain the standard and that organizational use of rule
headlines may require a license; exact rights depend on whether access is for an
individual or a workgroup and should be confirmed with MISRA. A representative
[MISRA publication product page](https://misra.org.uk/product/misra-c2008/)
describes a named-person, single-user document license that cannot be shared or
transferred. The exact **MISRA C:2012 plus Amendments 1, 2, and 3** institutional
terms must therefore be obtained and retained; a different publication's
single-user terms must not be generalized as that exact contract.

| Operation | Decision for MISRA standards/rule text |
|---|---|
| Download | Blocked for automation. A named, authorized person or institution obtains the exact publications through lawful access |
| Install/store | Controlled document repository with named access; never in Git, a container layer, runner workspace, devcontainer, or course image |
| CI execution | Public CI may run Cppcheck by rule ID and project-owned configuration/fixtures. It must not load licensed rule text |
| Cache | Prohibited in GitHub Actions cache, artifacts, logs, or public/private container layers unless the exact institutional license expressly permits that storage and audience; the recommended design uses none |
| Student access | Each student uses institutionally licensed access or obtains their own authorized copy. Course notes may link to MISRA and use rule identifiers; do not reproduce guideline/headline text without permission |
| Source/binary redistribution | N/A for software binaries; reproduction/distribution of document or rule text is blocked absent an express grant |
| Duties | Record licensed edition/amendments, licensee and authorized audience outside Git; keep only IDs, project interpretations, examples, deviations, and tool capability evidence in the public repository |

The freely downloadable
[MISRA Compliance:2020](https://misra.org.uk/app/uploads/2021/06/MISRA-Compliance-2020.pdf)
is process guidance, not a substitute license for the MISRA C rule text. SDOS
must continue to say “MISRA-oriented checks” or “checked MISRA C:2012 subset.”
Cppcheck's own manual describes partial coverage, so no free CI result supports
a formal compliance claim.

## GitHub Actions, cache, artifact, and fork implications

**Fact.** On `pull_request` runs from forks, GitHub does not pass repository
secrets and gives `GITHUB_TOKEN` read-only permissions, subject to repository
settings. GitHub also warns that `pull_request_target` runs in the base-repository
context and can be dangerous when it executes untrusted code. See
[workflow event behavior](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows)
and [secure use guidance](https://docs.github.com/en/actions/security-guides/security-hardening-for-github-actions#understanding-the-risk-of-script-injections).

**Fact.** GitHub's
[dependency-cache reference](https://docs.github.com/en/actions/reference/workflows-and-actions/dependency-caching-reference)
warns against storing sensitive information; pull requests from forks can read
caches made on the base branch. Cache contents are not an access-control
boundary. A cache entry can also be influenced by untrusted work, so consumers
must treat restored files as untrusted and re-verify hashes.

**Inference and required implementation:**

1. `pull_request` from any branch/fork runs only the public `oss-base` image and
   project source. Use `permissions: contents: read`, no environment secrets,
   no OIDC token, and no `pull_request_target` checkout/execution of PR code.
2. Public caches contain only reconstructible OSS package/build data. Cache keys
   include the lock/manifest digest. The job verifies archives before use.
3. TI installers, TI-bearing layers, MISRA material, license-acceptance records,
   private registry credentials, and proprietary outputs never enter Actions
   cache, workflow artifacts, logs, or PR summaries.
4. A trusted protected-branch/tag workflow may request a short-lived credential
   and pull an immutable private `ti-toolchain` layer after source checkout and
   event/ref authorization. Do not expose that credential to build scripts from
   an untrusted fork.
5. Publish OSS test reports freely. Before publishing firmware, course images,
   containers, SBOMs, or provenance, compare the output file list with the
   approved license manifest and fail closed on unknown paths/licenses.
6. GitHub-hosted runners are ephemeral, but runner deletion is not a license
   grant. Installation and execution still require applicable permission.

## Public/controlled layer design

| Layer | Contents | Visibility and workflow |
|---|---|---|
| Public monorepo | SDOS source, manifests and hashes, Dockerfiles without restricted payloads, CMake, host mocks, tests, labs, rule IDs/configuration, public patches, notices | Public forks and PRs; no secrets |
| Public `oss-base` / `ci` / `dev` / docs images | Exact Ubuntu digest; audited OSS tools and dependencies; license bundle, source manifest, valid SBOM | Public registry after reproducible build and license/source gate |
| Controlled `ti-toolchain` layer | Exact authorized PDK subsets, C6000/C7000 CGT, TI libraries/firmware only where approved; no MISRA text | Private immutable registry/object store, named institutional/student access, audit logs, no GitHub cache |
| Student entitlement flow | Bootstrap resolves public layer, then either guides the student through individual TI acquisition or obtains a course layer after institutional authentication | No credentials or accepted-license state in Git; record only non-secret artifact hashes/version checks |
| Trusted build lane | Protected branch/tag obtains controlled layer through short-lived secret/OIDC after authorization; builds full matrix | GitHub-hosted runner; artifacts pass file-level export allowlist before publication |
| Fork PR lane | Public layer, reference algorithms, mocks, portable tests, static analysis, manifest and image checks | Required check with read-only token; TI matrix explicitly reports `not-run-unlicensed-layer`, never false success |
| Course image/release | Only approved redistributable binaries plus notices, valid SPDX SBOM, sources/source offers, provenance, and hashes | Public only after release license gate; otherwise distribute through controlled course access |

The controlled layer must be rebuildable from an institution-owned acquisition
record. It must not be a manual snowflake: record upstream URL, selected version,
SHA-256, acceptance authority/date outside Git, license document hash, extraction
allowlist, installed file hashes, notices, and layer digest. This metadata can be
public except personal/credential/contract-confidential fields.

## Machine-readable decision seed

This CSV block is the Task 003 seed for the later repository manifest. `exact`
means the named baseline is fixed; `task004` or `task005` means that task must
add the version/hash before use. `conditional` always means fail closed until
the blocker and exact terms are recorded.

```csv
id,version_rule,license_class,distribution_profile,public_cache,public_release
ti-psdk-rtos-j721e,11.02.01.03,mixed,controlled,no,no
ti-pdk,11.02.01-path-audited,BSD-3-Clause-plus-exceptions,split,no,conditional
freertos-kernel,task005,MIT,oss,yes,yes
ti-tifs-pka-firmware,11.02.01-path-audited,TI-TSPA-or-TI-Text,controlled,no,conditional
ti-c66-dsplib,3.4,BSD-3-Clause,split,conditional,conditional
ti-c7-dsplib,resolve-11.02.00-vs-11.00.00.01,TI-Text,controlled,no,conditional
ti-mmalib,resolve-11.02.01-vs-11.02.00.11,TI-Text,controlled,no,conditional
ti-c6000-cgt,prefer-8.3.7-after-installer-audit,TI-Text-plus-third-party,controlled,no,no
ti-c7000-cgt,prefer-5.0.0.LTS-after-installer-audit,TI-Text-plus-third-party,controlled,no,no
ti-psdk-linux-j721e,11.02.01.03,mixed,controlled,no,no
ti-linux-firmware,task005-per-file-WHENCE,mixed-per-file,split,no,conditional
beagle-ai64-image,2026-10-09-sha256-69796038e452c6c5e006031ecab995a0ec734a866176e37a0de38d43b35380cf,mixed,aggregate-audit,no,conditional
beagle-linux-kernel,2026-10-09-package-source-commit-required,GPL-2.0-WITH-Linux-syscall-note,copyleft,yes,conditional
ubuntu-oci,22.04-platform-digest-task004,mixed,aggregate-audit,yes,conditional
git,task004,GPL-2.0-only,copyleft,yes,yes
gnu-make,task004,GPL-3.0-or-later,copyleft,yes,yes
cmake-ctest,task004,BSD-3-Clause-plus-third-party,oss,yes,yes
ninja,task004,Apache-2.0,oss,yes,yes
gcc-gcov-runtime,task004,GPL-3.0-or-later-plus-runtime-exception,copyleft,yes,yes
llvm-clang-tools,task004,Apache-2.0-WITH-LLVM-exception,oss,yes,yes
python,task004,PSF-2.0-plus-history,oss,yes,yes
unity,task004,MIT,oss,yes,yes
cmock,task004,MIT-plus-dependencies,oss,yes,yes
googletest-googlemock,task004,BSD-3-Clause,oss,yes,yes
cppcheck-misra-addon,task004,GPL-3.0,oss-without-rule-text,yes,yes
misra-c-documents,licensed-edition-plus-amendments,proprietary-document,controlled,no,no
lcov,task004,GPL-2.0-or-later,copyleft,yes,yes
lizard,task004,MIT,oss,yes,yes
shellcheck,task004,GPL-3.0,copyleft,yes,yes
shfmt,task004,BSD-3-Clause,oss,yes,yes
cmake-format,task004,GPL-3.0,copyleft,yes,yes
yamllint,task004,GPL-3.0,copyleft,yes,yes
markdownlint-cli2,task004,MIT-plus-npm-dependencies,oss,yes,yes
link-checker,select-in-task004,unknown,blocked,no,no
hadolint,task004,GPL-3.0,copyleft,yes,yes
docker-engine-cli-buildx-compose,task004,Apache-2.0-plus-dependencies,oss,yes,yes
buildkit,task004,Apache-2.0,oss,yes,yes
trivy,task004,Apache-2.0-plus-database,oss,yes,yes
actionlint,task004,MIT,oss,yes,yes
zizmor,task004,MIT-plus-crates,oss,yes,yes
gitleaks,task004,MIT,oss,yes,yes
dependabot,github-service-or-task004-core,service-or-MIT,hosted,na,na
reuse-tool,task004,Apache-2.0-plus-dependencies,oss,yes,yes
syft,task004,Apache-2.0,oss,yes,yes
codeql,github-hosted-pinned-bundle,GitHub-CodeQL-Terms,hosted-conditional,no,no
github-actions,full-SHA-per-action,per-action-plus-GitHub-terms,hosted,no,conditional
github-cache,service,GitHub-terms,public-oss-only,oss-only,na
```

## Plan cross-check

The matrix was cross-checked on 2026-10-18 against every third-party input
named in the bring-up plan: the Section 6.7 quality table, the Phase 4 host
transport, the Phase 5 maintainer tooling, the image inputs, and the
container layers. The check found two planned inputs without a matrix row.

| Input | Primary-source finding | Matrix handling |
|---|---|---|
| `ti-rpmsg-char` host library, Phase 4 | The [TI Processor SDK Linux J721E 11.02.01.03 manifest](https://dr-download.ti.com/software-development/software-development-kit-sdk/MD-U6uMjOroyO/11.02.01.03/ProcessorSDKLinux_J721E_11_02_01_manifest.html) lists `libti-rpmsg-char0`, `-dev`, `-staticdev`, and the examples at 0.6.10, commit `057b1a2492`, as BSD-3-Clause. The same manifest lists `libti-rpmsg-dma` separately under TI-TFL. The upstream [git.ti.com repository](https://git.ti.com/cgit/rpmsg/ti-rpmsg-char) timed out during the audit, so its LICENSE file is a pin-time check. | `ti.rpmsg-char-library`, public lane, conditional on the Task 004 pin and license-file confirmation |
| Code Composer Studio and XDS drivers, Phase 5 | The [CCS 20.1 overview](https://software-dl.ti.com/ccs/esd/documents/users_guide_ccs_20.1.0/ccs_overview.html) states there is no license fee. The [CCS legal page](https://software-dl.ti.com/ccs/esd/documents/users_guide_ccs_20.1.0/ccs_legal.html) carries a limited license headed Technology Software Publicly Available: use only with TI devices, no reverse engineering, and bundled third-party software under its own manifest. | `ti.code-composer-studio`, excluded lane, maintainer workstation only |

U-Boot, device trees, and the kernel `rpmsg_char` module remain inside the
Beagle image and Linux kernel rows. Probe hardware belongs to the hardware BOM. The plan does not yet name the
image-builder tooling (Task 035), the documentation build tool, or the link
checker. Each needs its own matrix row before first use; `LIC-005` and the
unresolved link-checker row keep that default-deny until then.

## Blockers and exact maintainer decisions

### Blocking evidence work

1. **Exact TI acquisition terms:** retain and review the license files/manifests
   actually shipped with PSDK RTOS 11.02.01.03, C6000 CGT 8.3.7, and C7000 CGT
   5.0.0.LTS. Record official URLs and SHA-256 values. Current-version manifests
   are useful comparisons but do not prove historical installer terms.
2. **TI library version disagreement:** resolve C7x DSPLIB and MMALIB versions
   between the RTOS component page and manifest, then inventory the selected
   archive by path/hash/license.
3. **Unattended acquisition:** prove whether the exact TI downloads are
   credential-free and noninteractive after an authorized acceptance, or design
   a lawful institution-controlled mirror. Do not automate click-through.
4. **Current Beagle image clearance:** close all 24 Debian-package empty-license
   entries and every other empty/ambiguous inventory entry; map firmware through
   copyright/`WHENCE`; capture exact kernel/U-Boot/image-builder source and
   patches; generate a valid SPDX SBOM.
5. **Copyleft source delivery:** select and test the release mechanism for exact
   corresponding source of every GPL/LGPL component in published containers and
   images. Merely linking to a moving upstream branch is insufficient evidence.
6. **MISRA entitlement:** obtain written institutional terms for MISRA C:2012
   and Amendments 1-3 stating named users/workgroup, storage, classroom use, and
   whether any guideline headlines may appear in internal tooling. Public CI
   remains rule-ID-only regardless.
7. **OSS lock:** Task 004 must pin versions, digests, hashes, package/transitive
   lockfiles, and license texts for the full register, and select the unnamed
   link checker. It must also decide whether LCOV is installed or reports are
   merely LCOV-compatible.
8. **CodeQL:** confirm the public-repository hosted-use model and keep the CLI
   bundle out of public container/releases unless its exact redistribution
   terms are approved.

### Maintainer decisions required

1. **Approve the recommended public-monorepo plus controlled licensed-layer
   model.** Alternatives are (a) individual student TI downloads on every
   workstation, which avoids institutional redistribution but worsens
   reproducibility, or (b) a public TI-bearing image, which remains blocked
   until written review approves the exact contents.
2. **Name the institution and authorized license acceptor** for TI downloads and
   MISRA publications, or choose individual student acceptance. No agent may
   accept these terms on the maintainer's behalf.
3. **Accept the two-lane CI consequence** while TI redistribution is unresolved:
   fork PRs run public checks; trusted main/nightly/release jobs run the complete
   firmware matrix. If every PR must compile all TI targets, the maintainer must
   obtain terms that permit a credential-free public toolchain layer.
4. **Choose the board-image release policy:** publish only SDOS deltas/scripts
   over the official hashed Beagle image, or publish a complete derived image
   after its aggregate license/source gate passes. The delta model is the lower
   risk recommendation for the first course release.
5. **Choose the copyleft source-delivery mechanism:** recommended is a permanent
   release-adjacent source archive keyed to each container/image digest, plus
   notices and build scripts, rather than relying on mutable upstream URLs.

No proprietary payload, MISRA rule text, license acceptance, or secret was
obtained or stored during this research. Only public manifests, documentation,
and the public BeagleBoard inventory were inspected.
