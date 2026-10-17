# 001 — Resolve architecture and course policy

## Status

ready

## Depends on

None.

## Plan references

Sections 1–4, Phase 0, Sections 6.2, 6.8–6.10.

## Objective

Turn the remaining course-policy choices into recorded architectural decisions
without asking the maintainer to research implementation details.

## Scope

- Research and recommend supported student host platforms and architectures.
- Recommend whether students replace complete DSP firmware or link algorithms
  into a supplied runtime.
- Recommend public/private repository and branch/fork submission topology.
- Define the C/C++ language versions, warning policy, checked MISRA subset,
  deviation process, and honest compliance wording.
- Propose measurable gain, FIR, and FFT performance objectives.
- Present each material choice with consequences and reversibility.

## Out of scope

Tool installation, hardware access, implementation, and Gate 0 approval.

## Maintainer checkpoint

`maintainer-decision`: present one compact decision packet with recommendations.
Record accepted choices as ADRs or an authoritative project-policy document.

## Acceptance criteria

- Every Phase 0 policy question has an accepted answer or a named Gate 0 blocker.
- The bring-up plan and task assumptions agree with the recorded decisions.
- No implementation choice is presented to the maintainer without an agent
  recommendation.

## Verification and evidence

Review the decision records against every Phase 0 policy bullet and link them
from the plan or manifest.
