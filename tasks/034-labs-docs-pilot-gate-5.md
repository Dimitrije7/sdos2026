# 034 — Create labs, validate documentation, and decide Gate 5

## Status

pending

## Depends on

031, 032, and 033.

## Plan references

Phase 5, Gate 5, and Section 6.10.

## Objective

Turn the SDK and examples into self-contained lab material proven from a clean
student perspective.

## Scope

- Write student quick start, workflow, target/profile selection, CI diagnosis,
  optimization, debugging, support, and recovery material.
- Create starter code, protected contracts, exercises, expected observations,
  and assessment-facing outputs for the example progression.
- Execute safe documentation snippets in CI.
- Run a clean-room agent session and an independent pilot participant through
  FIR or FFT on every DSP without platform-author intervention.
- Convert observed friction into fixes and regression checks.

## Out of scope

Making students perform Linux bring-up or requiring the maintainer to run the
pilot commands.

## Maintainer checkpoint

The maintainer oversees the pilot and makes the Gate 5 release decision; the
agent prepares and operates the environment and evidence capture.

## Acceptance criteria

- A clean supported workstation reaches verified FIR/FFT results from the
  student guide alone on each DSP.
- FreeRTOS and bare-metal paths are documented and tested.
- CI detects broken safe-to-run snippets and starter projects.
- Gate 5 has an indexed pass/fail record and resolved pilot findings.

## Verification and evidence

Archive clean-room logs, pilot observations, command/snippet test reports,
support bundles, timing, and the Gate 5 decision.
