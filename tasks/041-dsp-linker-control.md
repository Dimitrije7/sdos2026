# 041 — Give DSP firmware full, modular linker-script control

## Status

in-progress

## Handoff

- Updated: 2026-10-19, claude.
- State: the board-free part is implemented: every firmware build links from
  editable native TI linker files over a manifest memory contract, with
  module fragments, load/run copying, overlays, generalized validation, real-
  linker negative tests, and map reports. Left: the ownership audit of MSMC and
  shared DDR (needs Task 012) and the board proof.
- Done:
  - Memory contract: `manifest/dsp-cores.json` schema 2 lists every region per
    core (`ddr`, `l2sram`, `l1dsram`, `l1psram`, `dma_pool`, `msmc`,
    `shared_ddr`) with owner, `loadable`, `runtime_use`, per-runtime
    `cache_reserve`, reason, and source, plus `boot_alignment` with its reason.
  - Generated definitions: `cmake/SdosCores.cmake` writes `sdos/cores.h` (core
    table, load ranges, region macros) and, for firmware presets,
    `sdos_memory.cmd` (ORIGIN, LENGTH, USABLE per region for the preset's
    runtime).
  - Linker files: `firmware/linker/<core>/SDOS.cmd` (main entry per core, both
    runtimes via `SDOS_RTOS_*`), `common/c66x_runtime.cmd`,
    `common/c71x_runtime.cmd`, `modules/fir_memory.cmd` (load DDR, run L2,
    `table(BINIT)`, LOAD_START/RUN_START/RUN_SIZE), `modules/buffers_memory.cmd`
    (coefficients, `UNION` overlay of scratch, NOINIT). TI's linker files are no
    longer used. `SDOS_LINKER_SCRIPT` selects any other file.
  - Whole firmware placed: resource table, trace, boot and vectors, code, near
    and far data, stack, heap, IPC buffers, runtime state, task stack, module
    code, coefficients, scratch. The FreeRTOS C66x vector symbol map moved into
    `SDOS.cmd`.
  - Startup: `sdos_layout_check()` runs the FIR from its run address over
    linker-placed coefficients and scratch and traces the addresses and result;
    the TI runtimes process BINIT before `main()` on both families.
  - Validation: `ci/firmware_elf.py --runtime` accepts any loadable region minus
    that runtime's cache, refuses reserved and unaudited regions with their
    reason, overlapping load segments, and a misaligned entry point; the C check
    in `libsdos_dsp` uses the generated load ranges and the boot alignment.
    Every link treats linker warnings as errors.
  - Reports: `ci/linker_report.py` writes `<name>.layout.txt` and `.json` after
    every link (regions with first and last address and use; sections with run,
    load, size, region).
  - Alternative layouts as presets: `c66x_0-baremetal-release-l2` (everything in
    L2, 43.6% of 288 KiB) and `c71x_0-freertos-release-ddr` (data first, code at
    the top, fixed-address coefficients, inline module placement).
  - Tests: `ci/test_firmware_elf.py` (L2 up to the runtime's cache, reserved and
    unaudited regions, overlap, alignment), `ci/test_linker_layouts.py` (real TI
    links: default and reordered layouts build; vring pool, FreeRTOS L2 cache,
    and misaligned entry refused), C load tests in `test_dsp_mock`.
  - Documentation: `docs/linker.md`; policy P-002 amended; Task 032 scope.
  - Platform findings recorded in `docs/linker.md`: FreeRTOS keeps 64 KiB of C66x
    L2 as cache through TI's prebuilt kernel library; C66x L1 is all cache in
    both runtimes; C71x code regions need room for three 2 and 4 MiB-aligned
    slots; MSMC and shared DDR cannot be loaded by remoteproc and wait for the
    ownership audit.
- Next:
  1. With Task 012's live device tree: confirm the L2 sizes (C71x 448 KiB as
     TI's linker files declare) and MSMC and shared-DDR ownership; then mark
     usable parts `runtime_use: any` (still `loadable: false`) with evidence.
  2. Board session after Task 013: `course-smoke` for the default layout, then
     the two example presets; record the `layout:` trace line and a restart for
     each core and runtime under `release/evidence/task-041/`.
  3. Optional: rebuild TI's FreeRTOS C66x kernel library with a smaller L2
     cache to give FreeRTOS the full L2, if a lab needs it.
- Decisions: on 2026-10-19 the maintainer wrote "make a task for this and
  schedule it to be done as you find best", with the requirement quoted
  below, which states "Treat the DSP linker-policy change as authorized."
- Verification, 2026-10-19: all 14 firmware presets (12 plus 2 example
  layouts) built in the container with warnings as errors and passed the
  runtime-specific check; `ci.test_linker_layouts` passed 4 real-link cases;
  host tests passed in six configurations. Board checks not run.

## Depends on

021 and 022 for the firmware tree and validator it reshapes (both
in-progress; board-free work may proceed alongside them). Ownership claims
beyond the current carve-outs need 012. Board proof needs 013.

## Plan references

Section 3 (one authoritative memory-map description), Phase 3, Sections 6.1
and 6.6, and the memory-map risk in Section 8. Supersedes the linker part of
course policy P-002.

## Maintainer requirement

Received 2026-10-19, verbatim:

> Implement complete, general-purpose DSP linker-script control in this repository.
>
> This is an explicit maintainer requirement: users must have the flexibility expected in an industrial embedded project. Teaching restrictions must not limit DSP memory layout. This supersedes the existing course policy wherever it reserves linker-layout control exclusively to the course runtime.
>
> The model I want resembles an automotive project with ADAS.ld: one main linker script declares memory regions and controls placement, with separate included linker files for individual modules.
>
> Scope and boundaries:
>
> - Support C66x_0, C66x_1, and C71x_0, with FreeRTOS and bare-metal maintained equally.
> - Keep the Linux installation, kernel, device tree, and Linux memory reservations unchanged. Linux bring-up is outside this task.
> - Expose all memory legitimately usable by the DSPs under that platform configuration, including applicable local SRAM, assigned DDR, and available shared memory.
> - Establish actual ownership and accessibility. Memory outside Linux's reservation is not automatically free: account for other processors, firmware, peripherals, address translation, and cache/SRAM configuration.
> - Restrictions must have a concrete hardware, loader, ownership, or interoperability reason. Avoid arbitrary application-only partitions and teaching-policy restrictions.
>
> Required design:
>
> 1. Provide complete, editable native TI linker command files.
>
>    Use a clear main entry point, such as SDOS.cmd, with appropriate per-core and per-runtime composition. Verify syntax against the pinned TI toolchains; existing .lds and .cmd files may both contain TI linker syntax.
>
>    Users must directly control MEMORY and SECTIONS, region origins and lengths, fixed addresses, section ordering and grouping, alignment, linker symbols, and separate load/run placement. Preserve access to native linker capabilities, including intentional overlays where supported.
>
>    Document derived end addresses consistently.
>
> 2. Support modular organization.
>
>    Allow the main script to include module-specific files such as fir_memory.cmd and fft_memory.cmd. Provide working examples of source objects assigned to named sections and those sections assigned to memory.
>
>    Module fragments are an organizational convention. Users may reorganize or replace the supplied layout without going through a restrictive placement API or custom configuration language.
>
> 3. Cover the whole DSP firmware.
>
>    Editable placement must include application and runtime code/data, stacks, heaps, coefficients, buffers, and scratch storage. Supplied layouts are working defaults.
>
>    Preserve genuine boot and communication requirements, but distinguish mandatory constraints from default addresses that can move when related DSP configuration is updated. Explain the reason for every mandatory reservation.
>
>    Include necessary DSP startup, initialization, cache/MMU, and address-translation changes. Separate load/run placement requires a working copy mechanism. Document NOINIT accurately without promising persistence across firmware loading, reset, or power loss.
>
> 4. Preserve one authoritative platform memory contract.
>
>    Reuse or extend the existing manifests for ownership, accessible ranges, address views, and unavoidable constraints. Generate or validate shared definitions from that source.
>
>    Keep user-authored section placement editable. Maintain reproducibility and avoid duplicating hardware facts across scripts.
>
> 5. Validate legitimate custom layouts.
>
>    Inspect the current ELF and linker-map validators and remove assumptions that unnecessarily require the default layout. In particular, determine whether existing DDR-only validation needs expansion for supported local memory.
>
>    Validate address ranges, arithmetic, ownership, alignment, load/run semantics, and required boot/IPC metadata. Accept intentional, valid arrangements without requiring application sections to retain default names or addresses.
>
>    Distinguish accidental overlap from explicitly managed linker overlays.
>
> Execution and acceptance:
>
> - Read AGENTS.md, the relevant bring-up plan sections, course policy, and task/handoff records. Inspect the current worktree because implementation may have advanced since this requirement was discussed. Preserve unrelated ongoing work.
> - Treat the DSP linker-policy change as authorized. Update the affected policy, plan, task acceptance criteria, and documentation accordingly.
> - Inspect the installed pinned SDK/compiler inputs and authoritative TI documentation. Respect existing licensing and redistribution rules.
> - Implement the capability; updating documents alone does not complete this request.
> - Demonstrate materially different valid layouts, including DDR repartitioning and supported local-SRAM placement. Provide examples of module placement, alignment, linker symbols, and load/run initialization.
> - Test valid alternatives and rejection of invalid ownership/range/layout cases.
> - Build all three cores × both runtimes × debug/release through the project's containerized entry points.
> - Produce useful linker maps and reports showing regions, section addresses, sizes, and utilization.
> - Verify actual loading, initialization, correctness, and restart behavior on a compatible connected board when available. Keep physical-board evidence separate from compilation and static validation; record unavailable hardware checks explicitly.
> - Document exact platform limitations instead of silently reducing the requested flexibility. If a memory region cannot be used without changing Linux, explain that constraint and complete the DSP-only scope.
>
> The intended result is a complete DSP firmware development environment with industrial linker flexibility, an automotive-style modular organization, and no artificial teaching restrictions.

## Objective

Every DSP firmware build, on all three cores and both runtimes, is placed by
an editable, native TI linker command file with a main entry point and
module fragments, over a memory contract that states what each DSP may use
and why, validated without requiring the default layout.

## Scope

As in the requirement: native editable linker files (`SDOS.cmd` per core and
runtime, module fragments such as `fir_memory.cmd`), the whole firmware
covered, startup/cache/MMU and load/run copy support, one manifest-based
memory contract with generated or validated definitions, generalized ELF and
map validation with overlay awareness, maps and utilization reports, and
documentation of every mandatory reservation and platform limitation.

## Out of scope

Changing Linux, the kernel, the device tree, or Linux memory reservations; a
placement API or configuration language in place of native linker files.

## Maintainer checkpoint

None for the policy, which the requirement authorizes. `hardware-ready` for
the ownership audit and the board proof.

## Acceptance criteria

- All 12 firmware builds link from editable native TI linker files with a
  main entry point and module fragments, through `make build`.
- At least two materially different valid layouts build and pass validation,
  including a DDR repartition and local-SRAM placement, with examples of module
  placement, alignment, linker symbols, and load/run initialization.
- Invalid ownership, range, arithmetic, alignment, overlap, and missing
  boot/IPC metadata cases are rejected with the exact contract named;
  intentional overlays are accepted.
- Every mandatory reservation has a recorded hardware, loader, ownership, or
  interoperability reason; hardware facts live only in the manifest.
- Maps and reports show regions, section addresses, sizes, and utilization.
- Board evidence, kept separate from static evidence, shows loading,
  initialization, correctness, and restart of the default and an alternative
  layout on each core and runtime, or records the check as not run.

## Verification and evidence

Run the container builds for all 12 presets and each example layout, the
validator tests with positive and negative fixtures, and archive maps,
reports, and board results under `release/evidence/task-041/`.
