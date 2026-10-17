# 021 — Create the course firmware templates

## Status

pending

## Depends on

020.

## Plan references

Phase 3 and the firmware matrix in Sections 6.1 and 6.4.

## Objective

Provide minimal, maintainable FreeRTOS and bare-metal starting points for both
C66x cores and the C71x core.

## Scope

- Define a shared platform/runtime interface and thin core-specific adapters.
- Implement startup plus a FreeRTOS task or bare-metal event loop.
- Include the validated resource table, RPMsg endpoint, linker script, trace,
  exception/crash record, and deterministic debug/release configuration.
- Isolate generated/vendor code from course-owned code and analysis results.
- Build all 12 core/profile/configuration combinations.

## Out of scope

Bulk shared buffers, final student algorithms, or changing the Gate 2 memory
contract without a new decision.

## Maintainer checkpoint

None unless the public student/runtime boundary changes the accepted course
policy.

## Acceptance criteria

- The complete C66x_0/C66x_1/C71x_0 × FreeRTOS/bare-metal × debug/release matrix
  builds through one command model.
- Each firmware exposes identity, version, echo, trace, and crash information.
- Common behavior has one source; target differences are explicit adapters.

## Verification and evidence

Run clean matrix builds and portable tests; archive artifacts, maps, manifests,
and build logs.
