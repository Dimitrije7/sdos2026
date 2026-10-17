# 032 — Build the student SDK workflow

## Status

pending

## Depends on

026 and 030.

## Plan references

Phase 5 and Section 6.10.

## Objective

Make configure, build, test, deploy, run, debug, and recover a coherent
one-command student experience for both runtime profiles and all DSPs.

## Scope

- Define stable student algorithm and host-application extension points.
- Implement target/profile selection through the command facade and container.
- Add artifact selection, compatibility checks, deployment, result capture,
  and support-bundle flow.
- Separate student-editable files from protected platform contracts.
- Version the student API and document compatibility/deprecation rules.

## Out of scope

Linux bring-up, student linker/memory-map edits, JTAG as a requirement, or
duplicated per-lab build systems.

## Maintainer checkpoint

None unless the public student API changes accepted course scope.

## Acceptance criteria

- One documented workflow covers C66x_0, C66x_1, C71x_0 and both runtime modes.
- A new exercise can add an algorithm without copying platform internals.
- Local failures map directly to the same CI checks and reports.

## Verification and evidence

Run clean-container workflow tests against mocks and available hardware; verify
protected/student-editable boundaries and semantic-version metadata.
