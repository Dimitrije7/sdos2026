# 015 — Run the Gate 2 soak and decide the platform architecture

## Status

pending

## Depends on

014.

## Plan references

Phase 2, Gate 2, and its fallback sequence.

## Objective

Determine whether the candidate Beagle Linux and TI SDK combination is the
course baseline before investing in the student SDK and final image.

## Scope

- Run the multi-hour concurrent IPC soak and repeated three-core lifecycle test.
- Analyze all warnings, resets, timeouts, corruption, and thermal observations.
- Compare results with Gate 2 acceptance and the memory audit.
- Recommend freezing the stack or taking the smallest documented fallback.
- If a fallback is needed, isolate and validate only the minimum candidate
  kernel/DT change before recommending a larger platform switch.

## Out of scope

Silently accepting instability or beginning final SDK/image work before the
decision.

## Maintainer checkpoint

`maintainer-decision`: accept the recommended baseline or a documented fallback
based on the indexed evidence.

## Acceptance criteria

- Every Gate 2 statement is supported by connected-board evidence.
- The exact frozen or rejected stack is identified by hashes.
- Any fallback has scope, maintenance cost, reversibility, and new validation
  requirements recorded.

## Verification and evidence

Archive soak/lifecycle results and a signed Gate 2 go/no-go architecture record.
