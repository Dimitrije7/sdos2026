# Task 019 TI toolchain evidence, 2026-10-18

## What the TI layers install

| Layer | Content | Check |
|---|---|---|
| `ti-toolchain` | C6000 CGT 8.3.7 and C7000 CGT 5.0.0.LTS, unattended install from TI's URLs | Installer SHA-256 against the manifest under `set -e`; `--compiler_revision` printed 8.3.7 and 5.0.0 |
| `ti-sdk` | PDK 11.02.01.08 with FreeRTOS 10.5.1, C66x DSPLIB 3.4.0.0, C7x DSPLIB and FFTLIB 11.00.00, MMALIB 11.02.00, `psdk_rtos` scripts: 26,317 files, about 2.1 GB | Archive SHA-256 `8421152e…50bc` checked by the facade's download and again inside the build |

Both layers exist only after `make bootstrap TI=accept`, or with
`SDOS_TI_TERMS=accept` in trusted CI, and are never pushed to a registry.

## Negative tests

- A wrong C6000 installer SHA-256 stopped the build at `sha256sum --check`.
- A wrong SDK archive SHA-256 stopped the `ti-sdk` stage at `sha256sum --check`.
- `-DSDOS_TI_EXPECTED_VERSION=8.3.8` failed configuration: "is version 8.3.7;
  the manifest pins 8.3.8".
- `trivy fs --scanners secret` on the repository found nothing, and the image
  history holds no credential: every TI download is anonymous.

## Inventory against TI's manifests

[`psdk-inventory.json`](psdk-inventory.json) is produced by
`python3 ci/ti_inventory.py` inside the TI image. It lists, per component, the
file count, size, a tree hash, the TI manifest and its SHA-256, every manifest
row, and the number of files under each license. Every extracted file maps to
a manifest row; none is uncovered.

| Component | Files | License stated by TI's manifest |
|---|---|---|
| PDK | 16,960 | BSD-3-Clause |
| FreeRTOS kernel in the PDK | 908 | MIT |
| PDK SA LLD | 215 | BSD-3-Clause, SSLeay |
| PDK TIFS and PKA firmware | 41 | TI Text File License (TSPA) or TI Text File |
| C66x DSPLIB 3.4.0.0 | 4,625 | TI BSD |
| C7x DSPLIB and FFTLIB 11.00.00, MMALIB 11.02.00 | 3,279 | TI Text File |
| `psdk_rtos` scripts | 289 | TI Text File |

## Findings

1. The C7x libraries ship in unversioned folders `dsplib/` and `fftlib/`. The
   Task 005 probe pattern `*/dsplib_*` missed them; the layer now extracts
   exact folder names. Version manifest revision 3 records the real paths and
   the versions TI's manifests declare.
2. 737 of 746 C files in the C66x DSPLIB carry BSD-3-Clause text, but 49 files
   under `packages/ti/dsplib/src/*/c66/` also state that the software "is
   licensed for use with Texas Instruments TMS320 family DSPs" under a
   `TI_license.PDF` that the package does not include. Before any published
   firmware includes code from those files, the classification needs TI's text
   or the files must be excluded. This is recorded under `LIC-001`.
3. Everything the course needs from the C7x libraries, MMALIB, and the TIFS
   firmware is under TI Text File terms, so publishing artifacts that contain
   them needs the maintainer's approval of that classification under
   `LIC-001`.
