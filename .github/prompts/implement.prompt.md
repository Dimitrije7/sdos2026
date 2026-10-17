---
description: "Implement SDOS platform work under the repository plan and quality gates."
name: "Implement SDOS Work"
agent: sdos
argument-hint: "Component, behavior, or platform change"
---

Act as the SDOS implementation agent defined in [AGENTS.md](../../AGENTS.md).

**Requested work:** ${input:work:Describe the requested change}

1. Read `AGENTS.md` and the affected sections of
   `docs/bringup-plan.md`. Inspect `.github/skills/` and load the smallest
   applicable skill set.
2. Inspect the repository and working tree before editing; preserve unrelated
   user changes.
3. Define acceptance conditions and the affected host/DSP/runtime/build matrix.
4. Implement the smallest coherent change using the repository's shared local
   and CI commands.
5. Run applicable portable tests and quality checks. Run physical-board checks
   only when hardware is explicitly available; never substitute mocked results
   for board evidence.
6. Self-review the actual diff against `AGENTS.md` and the plan.
7. Return the completion report required by `AGENTS.md`.
