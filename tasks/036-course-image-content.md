# 036 — Integrate and harden the course image content

## Status

pending

## Depends on

030 and 035.

## Plan references

Phase 6 and the image/system tests in Section 6.6.

## Objective

Make a freshly built image boot into a secure, diagnosable, course-ready state
with the platform services and student tools installed.

## Scope

- Install pinned kernel/DTB/firmware packages, host library, `dspctl`, health
  tools, examples, service units, udev/permission policy, and local docs.
- Define harmless DSP startup/stopped policy and deterministic service ordering.
- Add `/etc/sdos-release`, machine-readable manifests, and first-boot per-board
  SSH identity generation.
- Remove conflicting/unneeded services and default credentials.
- Implement update and rollback packages while preserving evidence/recovery.

## Out of scope

Shared credentials, broad root access for students, or unrecorded package drift.

## Maintainer checkpoint

`hardware-ready` for boot/service/permission validation.

## Acceptance criteria

- The image identifies its exact source, platform, and installed component
  versions.
- Student operations work with least privilege and no default secret.
- Service ordering, DSP ownership, update, and rollback pass static and real
  board tests.

## Verification and evidence

Run filesystem/package/service/permission tests, first-boot validation, update
and rollback scenarios, and archive board reports/logs.
