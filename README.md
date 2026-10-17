# SDOS 2026 — BeagleBone AI-64 DSP Course Platform

This repository will contain the reproducible Linux image, DSP firmware
examples, host-side support, validation tests, and laboratory material for the
Systems for Digital Signal Processing course.

The complete engineering, CI, testing, and course-delivery roadmap is in
[`docs/bringup-plan.md`](docs/bringup-plan.md).

Execution is split into dependency-ordered agent work orders in
[`tasks/README.md`](tasks/README.md). The maintainer oversees gates and makes
architectural decisions; agents perform the implementation and connected-board
work.

AI-assisted contributors must read [`AGENTS.md`](AGENTS.md). The repository
uses one canonical, provenance-tracked skill set under `.opencode/skills/`,
shared with supported agent harnesses through repository symlinks.
