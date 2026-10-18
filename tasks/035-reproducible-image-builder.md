# 035 — Implement the reproducible Linux image builder

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: `make image` builds the course image from the pinned BeagleBoard
  image without privileges and verifies it; the from-source kernel variant
  and the eMMC flasher layout are left.
- Done:
  - `image/build.py`: decompress with expanded-hash check, edit the root
    filesystem with `debugfs` (no mount, no root, no emulation), fixed
    timestamps from the recipe, `e2fsck -fn`, unchanged partition table and
    other partitions, deterministic `xz`, manifest with every hash.
  - `image/recipe.json`: `/etc/sdos-release`, `dspctl`, the six release
    firmware files, `/usr/share/sdos/README`.
  - `make image` fetches the pinned base on the host, builds the inputs in
    the container, then runs the builder; documented in `docs/image.md`.
  - `ci/test_image.py` on a synthetic 16 MiB image: recipe applied, two builds
    byte-identical, replacement of existing files, refusals.
- Next:
  1. Record the first full build: duration, peak disk, output hashes, and a
     second build's hashes.
  2. Kernel from source (maintainer decision "both options"): build
     `linux.kernel-source-6.18.55-arm64-k3-r59` with the package's
     configuration in the container (needs bison, flex, bc, libssl-dev, and
     cpio pinned in the toolchain), install the kernel, modules, and DTBs into
     the image, and regenerate the initramfs. The initramfs step runs AArch64
     code, so it needs either emulation or a recorded host-side generator.
  3. eMMC flasher layout, and a CI lane within GitHub-hosted runner limits.
- Decisions: on 2026-10-18 the maintainer wrote "proceed with plan, but do everything except board tests, also postpone any decision making from my side because I will not be able to respond". This board-free work therefore started before its board-side dependencies were done; acceptance still needs them. The kernel decision from 2026-10-18 above still applies.
- Verification, 2026-10-19: `python3 -m unittest ci.test_image` in the
  container passed 7 tests.

## Depends on

009, 015, and 017.

## Plan references

Phase 6 and Sections 6.4–6.6.

## Objective

Turn the pinned upstream Beagle image into a deterministic, inspectable course
image without maintaining a hand-edited master card.

## Scope

- Implement an `image-builder` container with the minimum required privileges.
- Verify the upstream image and apply versioned package/file/configuration
  transformations from source.
- Produce SD and eMMC-flasher layouts with manifests and deterministic metadata
  where upstream formats permit.
- Add static partition/filesystem/package/configuration tests and dry runs.
- Fit main/nightly GitHub-hosted runner storage, memory, and time budgets.

## Maintainer decision

On 2026-10-18 the agent asked "Should the image keep BeagleBoard's kernel
package, or must the kernel also be built from source?" and the maintainer
answered "both options". The builder therefore supports two kernel sources
selected by one input: the default installs BeagleBoard's pinned kernel
package, and the second builds the same kernel version from pinned source with
the matching configuration. Both variants must pass the same image tests.

## Out of scope

Embedding secrets, mutable downloads, undocumented live edits, or publishing an
unqualified release.

## Maintainer checkpoint

None unless runner limits force an architecture change.

## Acceptance criteria

- `make image` builds from pinned inputs in a clean authorized environment.
- All customizations are traceable to repository source and testable offline.
- The job operates with documented least privilege and measured resource use.

## Verification and evidence

Run two clean builds, compare files/manifests, execute negative/static image
tests, and record resource/timing data.
