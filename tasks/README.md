# SDOS bring-up task graph

These work orders turn [`docs/bringup-plan.md`](../docs/bringup-plan.md) into
agent-executable increments. The plan remains the product and architecture
source of truth; task files define execution order and completion evidence.

## Responsibility model

Agents own research, implementation, workstation and board commands, builds,
flashing, deployment, tests, evidence capture, and documentation. The
maintainer only:

1. connects the requested board, serial adapter, network, or removable media;
2. approves destructive access after the agent identifies the exact target;
3. decides an explicit architecture, licensing, course-policy, or release gate
   from a recommendation prepared by the agent; and
4. oversees the resulting evidence.

An agent must not give the maintainer a command checklist for work the agent can
perform. A missing board is a hardware-ready pause, not a request for the
maintainer to diagnose it.

## Status model

The first `## Status` value in each task file is authoritative:

- `ready`: every dependency is `done`; an agent may start it.
- `pending`: a dependency is incomplete or a future checkpoint has not arrived.
- `in-progress`: one agent owns the task and is actively working it.
- `blocked`: progress is impossible after safe alternatives were exhausted;
  record the concrete cause and unblock condition.
- `done`: every acceptance criterion has evidence in the repository.

When completing a task, set newly unblocked tasks to `ready` in the same change.
Do not mark a task `done` for producing a plan when its acceptance criteria
require a working or measured result.

## Selecting work

When no task is named, choose the lowest-numbered `ready` task on the critical
path. Tasks with satisfied dependencies may proceed in parallel when they do
not edit the same contracts or files. Never start a dependent task from an
assumption that its predecessor will pass.

At the start of a task:

1. read `AGENTS.md`, this file, the task, its dependency reports, and its cited
   plan sections;
2. change the task to `in-progress` and write its handoff record;
3. state the acceptance conditions and applicable skills;
4. inspect the worktree before editing.

At completion:

1. run every verification named by the task;
2. archive generated evidence at the path required by the task;
3. self-review the diff against the task and `AGENTS.md`;
4. update documents or manifests invalidated by the result;
5. replace the handoff record with `## Completion evidence`, carrying over
   every maintainer decision;
6. change the task to `done` and unblock dependent tasks only when every
   acceptance criterion holds; and
7. commit the result as described in [Agent handoff](#agent-handoff).

## Agent handoff

Claude Code and Codex work this repository in turns on one working tree. A
session can end at any moment through a usage limit, crash, or agent switch, so
the repository carries all continuity. Chat history and agent-private memory
are scratch space; project state belongs in task files, documents, manifests,
and commits.

### Handoff record

Every `in-progress` task has a `## Handoff` section directly after `## Status`.
It is a current snapshot, rewritten in place:

- `Updated:` date and agent (`claude` or `codex`).
- `State:` where the task stands, in one or two sentences.
- `Done:` completed sub-steps, each naming the file or check that proves it.
- `Next:` ordered remaining steps, concrete enough to resume from the record
  alone.
- `Decisions:` each question put to the maintainer, its answer as given, and
  the date; `none` otherwise.
- `Verification:` checks last run and their observed results.

Rewrite the record after each completed sub-step and before any long research,
build, or board operation. A record written only at the end of a session is
lost when the session ends early.

### Maintainer decisions

Record a maintainer answer in the task file in the same turn it arrives. Quote
the answer and give its date, then update the authoritative document the
decision changes.

### Resuming

1. Run `git status` and `git log --oneline -10`, then find active work with
   `grep -A2 '^## Status' tasks/0*.md | grep -E 'in-progress|blocked'`.
2. Read each active task's handoff record and reconcile it with the worktree.
   A change the record does not explain was made after the last checkpoint;
   inspect it before building on it.
3. When the record is missing or stale, read the other agent's latest session
   transcript as evidence: Codex keeps them under `~/.codex/sessions/`, and
   Claude Code under `~/.claude/projects/<working-directory-slug>/`. Write the
   recovered state into the handoff record before continuing.
4. Re-run the record's verification checks and confirm they still pass.

### Commits

Commit locally at task completion and at the end of each turn that leaves the
work coherent, with the touched validators passing. Use a `task-NNN:` subject
prefix and an `Agent: claude` or `Agent: codex` trailer. Commit only paths the
session changed or adopted from the handoff, and push only when the maintainer
asks.

## Checkpoints

`hardware-ready` means the agent first checks whether the device is already
available. If absent, it asks only for the physical connection described by the
task, then resumes discovery and execution itself.

Before flashing or overwriting media, the agent records the exact device path,
stable device identity, model, capacity, mount state, and why it is the intended
target. It then requests approval for that exact destructive operation. A glob,
an unresolved variable, or an assumed `/dev/sdX` is never an acceptable target.

`maintainer-decision` means the agent prepares one recommended choice, compact
alternatives, supporting evidence, consequences, and reversibility. The
maintainer chooses; the agent records the decision and performs the resulting
work.

## Evidence

- Portable build/test/quality results belong in normal test reports and CI
  artifacts.
- Connected-board runs use timestamped, machine-readable records under
  `release/evidence/` with board identity, image/firmware hashes, commands,
  results, temperatures, and relevant logs.
- Secrets, credentials, licensed installers, student data, and unapproved
  binary dumps never enter evidence.
- Mocked evidence and real-board evidence are labeled separately.

## Task index

| Task | Outcome | Depends on | Checkpoint |
|---|---|---|---|
| [001](001-architecture-course-policy.md) | Architecture and course policy | — | maintainer-decision |
| [002](002-reference-hardware-and-lab-bom.md) | Reference hardware and lab BOM | — | maintainer-decision if procurement changes scope |
| [003](003-licensing-and-redistribution.md) | License/redistribution audit | — | maintainer-decision |
| [004](004-version-manifest.md) | Candidate version manifest | 001, 003 | — |
| [005](005-github-hosted-feasibility.md) | GitHub-hosted TI/image feasibility | 003, 004 | maintainer-decision on blocker |
| [006](006-gate-0-decision.md) | Gate 0 evidence and approval | 001–005 | maintainer-decision |
| [007](007-safe-board-access.md) | Safe board discovery/control | 004 | hardware-ready for final proof |
| [008](008-flash-and-first-boot.md) | Candidate image and first boot | 006, 007 | hardware-ready, destructive approval |
| [009](009-linux-baseline-and-board-report.md) | Linux baseline and diagnostics | 008 | hardware-ready |
| [010](010-linux-stability-gate-1.md) | Linux stability and Gate 1 | 009 | hardware-ready, maintainer-decision if cold power control is absent |
| [011](011-dsp-identity-and-service-ownership.md) | Stable DSP identities and ownership | 009 | hardware-ready |
| [012](012-device-tree-memory-audit.md) | Device-tree/reserved-memory audit | 004, 009 | hardware-ready |
| [013](013-single-core-ti-ipc.md) | Single-core TI IPC proof | 011, 012 | hardware-ready |
| [014](014-three-core-lifecycle.md) | Concurrent DSP lifecycle/recovery | 013 | hardware-ready |
| [015](015-gate-2-architecture.md) | Gate 2 soak and architecture decision | 014 | maintainer-decision |
| [016](016-command-facade-and-cmake.md) | Command facade and build skeleton | 006 | — |
| [017](017-oss-containers-and-devcontainer.md) | OSS containers and dev container | 004, 016 | — |
| [018](018-github-pr-gateway.md) | Minimal required PR gateway | 016, 017 | maintainer approval for repository settings |
| [019](019-ti-toolchain-layer.md) | Licensed TI toolchain layer | 005, 006, 017 | — |
| [020](020-ti-reference-firmware-matrix.md) | TI reference firmware matrix | 015, 019 | — |
| [021](021-course-firmware-templates.md) | Course firmware templates | 020 | — |
| [022](022-firmware-artifact-validation.md) | ELF/linker/resource validation | 020, 021 | — |
| [023](023-unit-mocks-coverage.md) | Unit tests, mocks, sanitizers, coverage | 016, 021 | — |
| [024](024-quality-security-misra.md) | Quality, security, MISRA-oriented gates | 018, 021, 023 | maintainer-decision on enforced MISRA subset |
| [025](025-complete-ci-lanes.md) | PR/main/nightly/release CI lanes | 018, 022–024 | — |
| [026](026-gate-3-reproducibility.md) | Clean build/deploy Gate 3 | 015, 019–025 | hardware-ready |
| [027](027-protocol-data-path-decision.md) | Protocol/data-path contract | 015, 023 | maintainer-decision |
| [028](028-host-library-and-mocks.md) | Linux host library and transport mock | 027 | — |
| [029](029-dsp-runtime-shared-buffers.md) | DSP runtime and shared buffers | 021, 027 | hardware-ready for final proof |
| [030](030-dspctl-cli.md) | Student/maintainer DSP CLI | 028 | hardware-ready for final proof |
| [031](031-data-path-gate-4.md) | Fault, stress, performance, Gate 4 | 026, 028–030 | hardware-ready |
| [032](032-student-sdk-workflow.md) | Student SDK command workflow | 026, 030 | — |
| [033](033-dsp-examples-and-golden-vectors.md) | DSP examples and golden vectors | 029, 032 | hardware-ready for target results |
| [034](034-labs-docs-pilot-gate-5.md) | Labs, documentation, clean-room pilot | 031–033 | hardware-ready, maintainer release decision |
| [035](035-reproducible-image-builder.md) | Deterministic image builder | 009, 015, 017 | — |
| [036](036-course-image-content.md) | Course image content and lifecycle | 030, 035 | hardware-ready for validation |
| [037](037-image-release-gate-6.md) | Image CI, SBOM, recovery, Gate 6 | 025, 036 | hardware-ready, destructive approval |
| [038](038-connected-board-qualification.md) | Qualification harness and evidence | 031, 037 | hardware-ready |
| [039](039-fleet-pilot-gate-7.md) | Fleet qualification and Gate 7 | 034, 037, 038 | hardware-ready, maintainer release decision |
| [040](040-semester-release-gate-8.md) | Semester release and maintenance | 039 | maintainer-decision |
| [041](041-dsp-linker-control.md) | Full, modular DSP linker-script control | 021, 022; ownership needs 012; proof needs 013 | hardware-ready for ownership and proof |
