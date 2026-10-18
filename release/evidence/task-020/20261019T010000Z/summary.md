# Task 020: TI reference firmware rebuilt from the pinned PDK

Built on 2026-10-19 with `make build PRESET=ti-reference`, in container
`sdos-dev:dev-ti-a7e82554a3c5`, from PSDK RTOS 11.02.01.03 (PDK 11.02.01.08).
The per-core JSON files are the build manifests: inputs, the exact PDK make
command, output hashes, the fit check, and the comparison with TI's prebuilt
blob from ti-linux-firmware `e853cc767f4b`.

| Core | TI core | Stripped ELF SHA-256 | Fits carve-out | Resource table equals TI's | Segment layout equals TI's |
|---|---|---|---|---|---|
| c66x_0 | c66xdsp_1 | `1e264565d6d2a0c3` | True | True | False |
| c66x_1 | c66xdsp_2 | `3e5f7664d2439bdc` | True | True | False |
| c71x_0 | c7x_1 | `8321bc7bc11b9c61` | True | True | True |

The C66x layouts differ in four data segments. TI's blobs come from
REL.PSDK.11.02.01.05, and this PDK is 11.02.01.08.

## Reproducibility, C66x_0 release, two clean builds

| Output | First | Second | Result |
|---|---|---|---|
| `ipc_echo_test_freertos_c66xdsp_1_release.xe66` | `abdb6a404d30d821` | `486aa1a4655a0730` | differs |
| `ipc_echo_test_freertos_c66xdsp_1_release.xe66.map` | `6ee7719c34b65d80` | `d301ef943714eedb` | differs |
| `ipc_echo_test_freertos_c66xdsp_1_release_strip.xe66` | `1e264565d6d2a0c3` | `1e264565d6d2a0c3` | same |

The stripped ELF, which is what Linux loads, is identical. The unstripped ELF
and the map file differ between builds, which points to build timestamps in
debug data and the map header; the loadable content is the same.

Not yet done: the per-core board smoke test of the rebuilt firmware (needs the
board, after Task 013).
