# Course image

`make image` builds the course SD-card image on your machine, from the pinned
BeagleBoard image plus the course additions:

```sh
make bootstrap TI=accept   # once
make image
```

**Output, in `build/image/`:**

- `sdos-course-bbai64.img.xz`, the image to flash;
- its `.sha256` file;
- `sdos-course-bbai64.manifest.json`, which records every input, every change,
  each check, and the output hashes.

Flash it with the board tool in [`board-access.md`](board-access.md).

## How it works

1. **Base image.** On your machine, `make image` downloads the base image named
   in [`image/recipe.json`](../image/recipe.json), unless it is already cached,
   and checks its SHA-256 against
   [`manifest/versions.json`](../manifest/versions.json).
2. **Inputs.** In the container, it builds what the recipe installs: `dspctl`
   for the board (`aarch64-gcc-release`) and, when the TI tools are installed,
   the six release firmware builds.
3. **Edit.** [`image/build.py`](../image/build.py) decompresses the base image,
   checking the expanded hash too. It then edits the root filesystem inside
   the image file with `debugfs`. Nothing is mounted, and the build needs no
   root privileges, loop devices, or emulation.
4. **Checks.** The builder confirms that each entry has the recorded type, mode,
   owner, and content. It runs `e2fsck -fn`, and confirms that the partition
   table and the boot and swap partitions are byte-for-byte unchanged. Any
   failure stops the build.
5. **Compress.** It compresses with a fixed `xz` block size, so the result
   does not depend on the number of CPU cores.

Every file, and every directory it touches, gets the recipe's
`source_date_epoch` as its time. Two builds from the same commit and inputs
produce the same image; `ci/test_image.py` checks this on a small synthetic
image.

## What the recipe adds

| Path | Contents |
|---|---|
| `/etc/sdos-release` | Image version, base image and hash, commit, kernel source |
| `/usr/local/bin/dspctl` | DSP control tool |
| `/usr/lib/firmware/sdos/` | Course firmware for each DSP, FreeRTOS and bare metal |
| `/usr/share/sdos/README` | Where things are, and a first `dspctl` session |

Accounts, credentials, services, and boot-time DSP state are
[Task 036](../tasks/036-course-image-content.md). Until then, the image keeps
BeagleBoard's default `debian` account.

## Kernel

The maintainer chose to support two kernel sources (Task 035). The recipe's
`kernel` field selects one:

- `beagleboard-package`, the default, keeps BeagleBoard's pinned kernel
  package from the base image;
- `source` will build the same kernel version from pinned source. It is not
  implemented yet, and asking for it stops the build with a message.

## Publication

The image is for your own boards. It contains the upstream image, whose
redistribution waits for LIC-003, and firmware linked with TI libraries, which
waits for LIC-001. The manifest says so in its `publication` field. CI does not
publish it.

## Resources

On an 8-core workstation, `make image` takes about 15 minutes once its inputs
are built; compression is most of it. The decompressed image is a 10 GiB
sparse file holding about 4.6 GB of data, and the result is about 1.1 GB.
