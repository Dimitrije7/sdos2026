# 011 — Resolve DSP identity and service ownership

## Status

pending

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
