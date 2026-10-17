# SDOS 2026 — agent working agreement

This is the working agreement for human contributors and coding agents in this
repository. Read it before changing source, build infrastructure, workflows,
images, documentation, or generated release metadata.

Updated: 2026-10-18

## Source of truth and instruction priority

Apply repository guidance in this order:

1. The user's latest explicit request.
2. [`docs/bringup-plan.md`](docs/bringup-plan.md), the engineering and course
   delivery source of truth.
3. This file and established repository conventions.
4. The closest component-level `README.md` or design document.

The repository is still in its planning and scaffolding phase. A path, command,
test, or capability described in the plan is a target until it exists in the
working tree and has been verified. Never report planned behavior as
implemented behavior.

## Outcome and boundaries

The project supplies a reproducible BeagleBone AI-64 teaching platform. Linux
runs on the TDA4VM A72 cores and controls student firmware on both C66x DSPs and
the C71x DSP. Students work on DSP applications, not Linux bring-up.

These boundaries apply to every change:

- Support all three DSP cores: `C66x_0`, `C66x_1`, and `C71x_0`.
- Treat FreeRTOS and bare-metal firmware as equal, maintained targets.
- Keep image construction, host support, firmware, labs, tests, CI, and
  documentation in this monorepo.
- Use a pinned, containerized development/build environment. Local and CI
  entry points must invoke the same underlying commands.
- Use GitHub-hosted runners only. Do not add a self-hosted runner dependency.
- Do not add automated HIL. Physical-board validation belongs in repeatable,
  agent-run connected-board suites under `tests/target/` and release evidence.
- A PR gateway must remain executable without a board: portable tests, mocks,
  static checks, firmware artifact inspection, and image validation.
- Students must not need to modify U-Boot, the kernel, or the device tree for a
  normal lab exercise.
- Prefer free and open tooling. Treat proprietary TI compilers, SDK content,
  firmware, and redistribution rights as explicit licensed inputs rather than
  repository assets unless their licenses permit distribution.
- Never describe a free MISRA-oriented check as formal MISRA compliance.

When a request conflicts with one of these boundaries, preserve the
non-conflicting scope and report the conflict.

## Working workflow

1. Read the request completely and select any applicable project skill.
2. Read the sections of `docs/bringup-plan.md` that define the affected
   architecture, acceptance gate, or policy.
3. Inspect the working tree and relevant implementation before editing.
   Preserve unrelated user changes.
4. State concrete acceptance conditions. For cross-layer work, trace the path
   from developer command through artifact, host/DSP boundary, and test.
5. Implement the smallest coherent change that satisfies those conditions.
6. Run the narrowest relevant checks first, then the documented aggregate
   check when it exists. Prefer the containerized project entry points used by
   CI.
7. Review the actual diff for scope, reproducibility, security, licensing,
   target coverage, and accidental generated or secret material.
8. Report results using the completion format below.

Make routine, reversible implementation decisions from the plan and existing
patterns. Ask before a decision that changes supported hardware, licensing,
repository visibility, runner ownership, course policy, or release scope.

## Task execution

[`tasks/README.md`](tasks/README.md) defines the bring-up task graph, status
transitions, evidence rules, and the division of responsibility between agents
and the maintainer. Read it when selecting, starting, completing, or unblocking
a planned task.

Agents own the work: research, implementation, workstation commands, board
commands, flashing, deployment, tests, evidence capture, and documentation.
The maintainer connects requested hardware/media, approves destructive access
to a verified device, and decides explicit architecture or release gates.
Never turn an agent-executable procedure into a list of commands for the
maintainer to perform.

When the user asks to continue the project without naming a task, choose a
`ready` task whose dependencies are `done`, preferring the lowest task number
on the critical path. A `hardware-ready` task pauses only when the required
device is absent. A `maintainer-decision` task must present a recommendation
and compact alternatives before requesting the decision.

## Skills

Project skills are vendored verbatim in `.opencode/skills/`. That directory is
canonical; `.codex/skills`, `.claude/skills`, and `.github/skills` are symlinks
to it so supported agent harnesses see one shared copy.

Before acting, inspect the available skills and use the smallest applicable
set. State which skill is being used and why. If none applies, use this working
agreement directly.

| Skill | Use it for |
|---|---|
| `code-review` | Reviewing a diff against repository standards and the originating request or plan |
| `codebase-design` | Designing or reshaping a module interface and its seams |
| `improve-codebase-architecture` | Surveying an established codebase for architectural improvements |
| `domain-modeling` | Stabilizing DSP/platform terminology and recording durable decisions |
| `diagnosing-bugs` | Hard, intermittent, or regression bugs with a tight feedback loop |
| `research` | Investigating hardware, SDK, compiler, kernel, or platform questions from primary sources |
| `writing-for-agents` | Editing this file, agent profiles, prompts, or skills |

Repository-specific adaptations override a skill's defaults:

- For `code-review`, use the user's request and `docs/bringup-plan.md` as the
  Spec axis. No issue tracker setup is required.
- A skill may recommend a method, but it cannot weaken repository safety,
  licensing, verification, or hardware-access constraints.
- Target tests may be run only when a compatible board is actually available.
  Otherwise validate their scripts statically and report target execution as
  not run.
- Add, update, or remove vendored skills only with maintainer approval. Update
  `.opencode/skills/.provenance.json` and the applicable third-party notice at
  the same time.

## Architecture and interfaces

The intended top-level layout is documented in the bring-up plan. Keep these
responsibility boundaries as implementation appears:

- `image/`: deterministic Linux image customization and packaging.
- `toolchain/` and `.devcontainer/`: pinned development/build environments.
- `firmware/common/`: shared protocol, resource tables, memory contracts, and
  runtime support.
- `firmware/c66x/` and `firmware/c71x/`: core-specific FreeRTOS and bare-metal
  targets.
- `host/libsdos_dsp/`: Linux-side lifecycle, IPC, and buffer abstractions.
- `host/dspctl/`: operator/student command-line interface.
- `labs/` and `examples/`: student-facing contracts and reference exercises.
- `tests/`: portable unit, smoke, system, regression, and connected-board suites.
- `ci/` and `.github/`: shared orchestration and GitHub policy, not a second
  implementation of the build.
- `manifest/` and `release/`: pinned inputs, hashes, SBOM inputs, and release
  evidence rather than mutable downloads.

Keep hardware facts and policy out of duplicated shell fragments. Put them in
versioned manifests, CMake/toolchain modules, schemas, or shared scripts with a
small documented interface.

## DSP and host invariants

- Give each remote processor a stable logical identity independent of
  enumeration order.
- Treat the remoteproc firmware name, resource table, linker map, reserved
  memory, RPMsg endpoints, and shared-buffer layout as one versioned contract.
- Validate address ranges, alignment, integer overflow, message lengths, and
  cache ownership at every Linux/DSP boundary.
- Make cache maintenance and buffer ownership explicit. Never rely on
  incidental coherence.
- Keep control messages bounded and versioned. Use shared memory for bulk
  samples rather than embedding large payloads in RPMsg.
- Make DSP start, stop, crash, timeout, and restart behavior observable and
  recoverable.
- Avoid hidden floating-point or target-specific assumptions in reference
  algorithms. Golden vectors must specify types, scaling, tolerance, and
  saturation behavior.
- Keep core-specific optimization behind portable interfaces with a host
  reference implementation suitable for tests.
- Linker and memory-map changes require review of all three DSPs and both
  runtime profiles, even when only one target exposed the problem.

## Build, test, and CI policy

The final project command surface is `make bootstrap`, `make configure`,
`make build`, `make test`, `make quality`, `make package`, `make image`,
`make deploy`, and `make target-smoke`. Introduce these incrementally; until a
target exists, use the repository's actual documented command and state the
gap.

Required verification is proportional to the change:

- Host-only logic: unit tests, sanitizer-enabled tests where supported,
  formatting, lint, and static analysis.
- DSP algorithm logic: host reference/golden-vector tests plus every affected
  firmware build variant.
- Protocol or memory contract: serialization/validation tests, mocked
  remoteproc/RPMsg system tests, ELF/resource-table inspection, and all target
  builds.
- Container/toolchain changes: build the affected image and execute a minimal
  compiler/configuration smoke test inside it.
- Image changes: static filesystem/package/configuration tests; full image
  construction where the workflow tier permits.
- Workflow changes: syntax/security lint plus inspection that PR `opened` and
  `synchronize` events still reach the aggregate required check.
- Documentation changes: link, spelling/style, and command-snippet validation
  where available.

GitHub Actions must be least-privilege, pin third-party actions by full commit
SHA, avoid secrets in untrusted PR jobs, and keep the aggregate required check
stable. Do not make a PR check depend on connected-board access.

Never claim a check passed unless it ran and its result was observed. Report
skipped checks and environmental limitations plainly. A mocked or static test
does not prove physical DSP behavior.

## Reproducibility, licensing, and safety

- Pin base images, SDKs, compilers, packages, actions, source revisions, and
  downloads. Record SHA-256 hashes and upstream licenses.
- Keep credentials, access tokens, accepted-license state, student data, and
  proprietary installer payloads out of Git and build logs.
- Verify TI license and redistribution terms before publishing a container,
  cache, image, firmware bundle, or release artifact.
- Prefer deterministic archives and record enough metadata to reproduce a
  release. Generate SBOMs and provenance through the release pipeline.
- Do not edit generated build products. Change their source and regenerate.
- Do not weaken warnings, analysis, tests, branch policy, or type/range checks
  to obtain a passing result.
- Preserve recoverability when manipulating removable media or board storage.
  Resolve the exact device first and require explicit maintainer authorization
  before flashing or overwriting it.
- Never perform destructive Git, filesystem, device, or board operations
  without explicit authorization and a verified target.

## Documentation policy

Write for students who know C/C++ and DSP fundamentals but are not responsible
for Linux bring-up. Every lab-facing workflow should state prerequisites,
commands, expected output, common failure modes, and recovery steps.

Keep one authoritative explanation per concept and link to it elsewhere.
Update the plan when a gate, supported target, policy, or architectural decision
changes. Record a durable and hard-to-reverse design decision as an ADR when an
ADR directory is introduced.

## Self-review quality bar

Before reporting completion:

1. Inspect the actual diff and status.
2. Map every requested condition to a changed file or verified behavior.
3. Check all affected DSP/runtime/build variants rather than the convenient
   host path only.
4. Check failure paths, bounds, timeouts, cleanup, restart behavior, and
   diagnostics.
5. Check CI/local command parity, deterministic inputs, licenses, secrets, and
   generated artifacts.
6. Remove placeholders, dead alternatives, debug output, broad suppressions,
   and unrelated formatting churn.

## Completion report

Keep the report concise and evidence-based:

```markdown
- Scope: requested outcome and implemented result
- Skills used: selected project skills, or `none`
- Files changed: path and purpose
- Verification: commands/checks run and their results
- Target coverage: host/mocked checks and any physical-board checks, clearly separated
- Assumptions or deviations: exact detail, or `none`
- Blockers or follow-up: exact detail, or `none`
```
