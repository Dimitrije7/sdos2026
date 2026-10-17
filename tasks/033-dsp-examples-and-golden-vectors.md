# 033 — Implement DSP examples and golden-vector regression

## Status

pending

## Depends on

029 and 032.

## Plan references

Phase 5 and the numerical/performance rows of Section 7.

## Objective

Create the reference progression students will optimize while preserving
portable correctness oracles and comparable measurements.

## Scope

- Implement hello/echo plus cycle counter, vector gain, scalar/optimized FIR,
  block FFT/magnitude, streaming overlap-save or multirate processing, and an
  optional three-DSP split.
- Define deterministic input vectors, fixed/floating types, scaling,
  saturation, tolerance, and expected output metadata.
- Provide portable A72/host references and target-specific optimized adapters.
- Capture compiler/vectorization reports, memory placement, cycle counts, and
  correctness before/after optimization.
- Add numerical and code-size regression suites across supported variants.

## Out of scope

Unvalidated benchmark marketing, mandatory NEON, or hiding vendor-library
license constraints.

## Maintainer checkpoint

`hardware-ready` for target results; example learning objectives already follow
the accepted course policy.

## Acceptance criteria

- Every example passes host golden tests and applicable DSP target comparisons.
- Tolerances and scaling are explicit, justified, and regression-protected.
- Measurements are reproducible and identify core/profile/build/frequency.

## Verification and evidence

Run host and connected-board numerical matrices, tolerance-edge fixtures,
compiler report capture, and performance comparison reports.
