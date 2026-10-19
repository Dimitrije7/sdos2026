# 011 — Resolve DSP identity and service ownership

## Status

done

## Handoff

- Updated: 2026-10-19, claude.
- State: done.
- Done:
  - Identities resolve by device-tree unit address: `4d80800000.dsp` (C66x_0,
    `j7-c66_0-fw`), `4d81800000.dsp` (C66x_1), `64800000.dsp` (C71x_0).
    `remoteproc0`, `1`, and `2` held across about 15 reboots on 2026-10-19,
    but nothing relies on the numbering.
  - Ownership: no package, unit, or firmware file on the image claims a DSP;
    all three are offline after boot, and `board-report` checks this. The
    only rpmsg devices present at boot belong to the MCU R5F running TI's
    device-manager firmware. Course-image policy: ship no DSP firmware under
    the kernel's default names and start no DSP at boot. Nothing needs
    disabling on this image.
  - Evidence: [`release/evidence/task-011/20261019T094327Z/`](../release/evidence/task-011/20261019T094327Z/).
- Next: none.
- Decisions: none.
- Verification, 2026-10-19: the inventory and three board reports on
  separate boots.

## Depends on

009.

## Plan references

Phase 2 and the remoteproc risks in Section 8.

## Objective

Map both C66x cores and the C71x core to stable Linux identities and establish
exclusive, observable ownership for course use.

## Scope

- Inventory remoteproc platform devices, firmware names, sysfs links, traces,
  RPMsg endpoints, and aliases from the effective system.
- Define stable logical names `C66x_0`, `C66x_1`, and `C71x_0` without relying
  on `remoteprocN` enumeration.
- Identify preinstalled services or Edge AI components that claim the DSPs.
- Add read-only ownership diagnostics and a reversible course-image policy for
  disabling conflicts.

## Out of scope

Loading replacement firmware or permanently removing vendor packages.

## Maintainer checkpoint

`hardware-ready`: the agent inspects the connected board and performs all
commands.

## Acceptance criteria

- All three logical DSP identities resolve deterministically after reboot.
- Expected firmware and trace paths are recorded.
- Conflicting services have a reversible, evidence-backed handling decision.

## Verification and evidence

Compare mappings across multiple reboots and archive the resolution report and
service-ownership diagnostics.
