# Platform version manifest

Status: candidate baseline, revision 1  
Task: [004 — Create the candidate version manifest](../tasks/004-version-manifest.md)  
Machine source: [`manifest/versions.json`](../manifest/versions.json)  
Schema: [`manifest/schemas/versions.schema.json`](../manifest/schemas/versions.schema.json)

The manifest names every external input the platform builds from, with an
immutable version and an integrity anchor. Containers, firmware builds, image
builds, and CI read pins from it instead of repeating them. Licensing decisions
stay in [`manifest/license-matrix.json`](../manifest/license-matrix.json); every
manifest entry points at its license-matrix artifact.

## Candidate baseline

All entries are `candidate` until connected-board evidence qualifies them.

| Section 4 component | Pinned candidate |
|---|---|
| Board image | BeagleBone AI-64 Debian 13.7 IoT image of 2026-10-09, with its block map and Syft inventory |
| Linux | Kernel `6.18.55-arm64-k3-r59` from the image, source tag at commit `dbedc304`, BeagleBoard device trees at `550f7711` |
| DSP SDK | TI Processor SDK RTOS J721E 11.02.01.03 with PDK 11.02.01.08, C66x DSPLIB 3.4.0.0, C7x DSPLIB, FFTLIB, MMALIB, and FreeRTOS kernel V10.5.1 |
| C66x compiler | TI C6000 CGT 8.3.7 |
| C71x compiler | TI C7000 CGT 5.0.0.LTS |
| Host build environment | Ubuntu `jammy-20260924.1` by digest, Ubuntu snapshot `20261004T000000Z` packages, Node.js 22, Cppcheck 2.22.0 from source |
| CI | Eight GitHub Actions pinned by commit, on the `ubuntu-24.04` hosted runner |
| Development UX | BuildKit v0.33.1 and Dockerfile frontend 1.27.1 by digest |

The host compilers are GCC 12 and Clang 15 from the Ubuntu 22.04 archive, plus
GCC 12 cross compilers for the board's arm64 Linux. The tool set also pins
lychee as the documentation link checker and LCOV 1.15 for coverage reports.

## Selection rule

Each open input uses the highest stable release published on or before the
manifest's `release_cutoff`, two weeks before the manifest date. The delay keeps
a just-published release out of the baseline. TI inputs use the exact versions
that the selected SDK declares. The board image is the plan's named candidate.

## Integrity anchors

Every entry carries an anchor that matches its kind. The schema rejects an entry
without one.

| Kind | Anchor |
|---|---|
| `file`, `pypi` | SHA-256 of the exact file, from the publisher where it publishes one |
| `npm` | The registry's SHA-512 integrity string |
| `git`, `github-action` | Full 40-character commit ID |
| `oci-image` | Index digest and per-platform digests |
| `apt-snapshot` | Snapshot timestamp, archive signing-key fingerprint, InRelease hashes, and each package's `.deb` SHA-256 |
| `contained` | The hashed input that contains it, plus its path |
| `runner-label` | A versioned label; each job records the hosted image version |

TI publishes SHA-256 values on its download pages, so TI archives are pinned
without being downloaded. They stay `gated` by license blockers `LIC-001` and
`LIC-002` until Task 005 records the exact no-cost terms and the maintainer
confirms them.

## Lifecycle

| State | Meaning | Requirement |
|---|---|---|
| `candidate` | Selected and hash-pinned, not yet proven on hardware | Integrity anchor |
| `qualified` | Proven by connected-board or build evidence | `qualification_evidence` under `release/evidence/` that exists |
| `semester-frozen` | Locked for a teaching semester | Same as qualified; changes need a new revision |
| `superseded` | Replaced by a newer entry | `superseded_by` names a live entry |

## Validation

Validate the manifest and its cross-references to the license matrix:

```sh
python3 ci/validate_versions.py
python3 -m unittest -v ci.test_validate_versions
```

Expected output ends with:

```text
validated version manifest sdos-platform-versions revision 1
```

Verify every file that is present in a local download cache against its
recorded hash. Absent files are counted, and any mismatch fails:

```sh
python3 ci/validate_versions.py --verify-cache /path/to/cache
```

The validator also rejects:

- moving references such as `latest`, `main`, or `stable`;
- raw GitHub URLs on a branch rather than a commit;
- unknown license-matrix artifacts, components, or blockers;
- an allowed acquisition where the license matrix does not allow download;
- an ungated input inside a gated archive; and
- containment cycles.

## Changing a pin

1. Resolve the new version from its primary source and record the published
   hash, digest, or commit.
2. Add a new entry, then mark the old one `superseded` with `superseded_by`.
3. Increase `revision`, update `updated`, and keep the Section 4 map current.
4. Run the validation commands above, then commit the change.

## Open items

- C7x DSPLIB and MMALIB versions differ between TI's components page and its
  SDK manifest. Task 019 records the folders in the hashed archive.
- git.ti.com was unreachable during selection. The TI Linux firmware revision
  is deferred to Task 013, and the full ti-rpmsg-char commit to Task 027.
- Task 009 records the booted kernel configuration and DTB hashes, and Task 017
  records minimum Docker host versions and the full package closure.
