# Trace2D Agent Operating Guide

This file is the entry point for any coding agent working in Trace2D.

The goal is that a fresh agent can open the repository, recover the project's current state, select the correct next task, implement it, validate it, and leave the repository in a state that another agent can continue from without relying on chat history.

## Project identity

Trace2D is a deterministic and observable C++20 2D engine designed for AI coding agents.

Its defining capability is not "AI generates game code." The engine itself must make development automatable:

```text
edit -> build -> run -> step -> inspect -> input -> assert -> capture
```

Authoritative gameplay state should be available as structured data. Pixels are for visual QA, not the only source of truth.

## Required reading order

Before changing code, read these files in order:

1. `AGENTS.md`
2. `PROJECT_STATUS.md`
3. `docs/PUBLIC_RELEASE.md`
4. `docs/ROADMAP.md`
5. `docs/ARCHITECTURE.md`
6. `docs/AGENT_FIRST_PRINCIPLES.md`

Then inspect the live GitHub state:

1. open pull requests
2. CI/check status on the active PR
3. open issues relevant to the current phase
4. recent commits if repository state differs from `PROJECT_STATUS.md`

If the active task is particle umbrella/children **#46-#53**, also read `docs/PARTICLES.md` and the exact active child issue before editing code.

Live repository state wins over stale prose. If `PROJECT_STATUS.md` is stale, update it as part of the work.

## Source-of-truth hierarchy

When documents disagree, use this order:

1. compiling code and automated tests
2. active PR and CI results
3. explicit architecture decisions / constraints
4. `PROJECT_STATUS.md`
5. `docs/PUBLIC_RELEASE.md`
6. `docs/ROADMAP.md`
7. older issue descriptions and discussion

For particle work, `docs/PARTICLES.md` plus the active #47-#53 issue records the owner-approved particle architecture. If those disagree with live compiling code/tests because implementation has advanced, reconcile the documentation in the same PR rather than silently choosing one interpretation.

Do not silently reinterpret a deliberate architecture constraint. If a constraint must change, document why in the same PR.

## How to choose the next task

1. If `PROJECT_STATUS.md` lists an active PR, finish or repair that PR first.
2. Never start a new feature while the active bootstrap/foundation PR has failing CI unless the failure is unrelated and explicitly documented.
3. Otherwise select the first unblocked issue in the "Next execution order" section of `PROJECT_STATUS.md`.
4. Work on one coherent issue/PR at a time unless two issues are inseparable.
5. Prefer the smallest vertical slice that leaves the repository runnable and testable.

Do not jump ahead to attractive later features such as an editor, advanced rendering, custom allocators, lock-free infrastructure, GPU particles, physics, or animation while earlier owner-fixed gates are incomplete.

Within the particle sequence, complete exactly one of #47 -> #48 -> #49 -> #50 -> #51 -> #52 -> #53 at a time.

## Development workflow

Use this flow for normal changes:

```text
Issue
  -> branch
  -> implementation
  -> tests
  -> local/CI validation
  -> documentation/status update
  -> draft PR
  -> green CI
  -> ready/merge
```

Branch naming for agent-created branches:

```text
agent/<short-description>
```

Main uses squash merges. Keep PR scope understandable from one squash commit.

## Before editing

For each task:

- read the issue acceptance criteria
- inspect the modules that will be affected
- confirm dependency direction in `docs/ARCHITECTURE.md`
- identify the relevant automated test level
- avoid adding a dependency unless the current phase actually needs it

## Validation requirements

Every implementation must run the most relevant available checks.

Current Windows baseline:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug
```

CI may use a separate preset appropriate for the current GitHub runner image.

New behavior should have automated tests when practical. A machine-facing feature without tests is considered incomplete unless the PR explains why it cannot be tested yet.

## C++ engineering rules

Prioritize predictable code over clever code.

- C++20 is the project baseline.
- Use RAII and explicit ownership.
- Prefer `std::unique_ptr` for owning heap relationships.
- Raw pointers/references are non-owning unless clearly documented otherwise.
- Avoid unnecessary shared ownership.
- Keep platform/library types behind module boundaries where practical.
- Do not add allocation to known per-frame hot paths without a reason.
- Do not build custom allocators, lock-free queues, ECS machinery, or bespoke containers before measurement/requirements justify them.
- No benchmark claim may be documented as fact without measured data.
- Stable entity identity exposed to automation must never be a raw pointer.
- Deterministic observable behavior must not depend on unspecified container iteration order.

## Agent-first rules

The following are hard architectural constraints unless deliberately revised with documentation:

- MCP is an adapter, not the engine API.
- The automation facade must remain protocol independent.
- Headless execution shares authoritative runtime logic with windowed execution.
- Simulation time must be explicitly controllable by tests/agents.
- Authored project/scene data should be text-first and diffable.
- Automation prefers semantic identity/selectors over screen coordinates.
- Structured runtime state is preferred over visual inference.
- Machine-facing commands use stable exit behavior and structured diagnostics.
- CLI/JSON/MCP adapters should compose a small vocabulary of operations instead of mirroring every engine function.

Target vocabulary:

```text
build
run
inspect
query
input
step
assert
capture
test
```

## Particle-specific hard rules

When working on #46-#53:

- CPU particle execution is the deterministic semantic reference and must be fully inspectable headlessly.
- Rich supported CPU particle properties are allowed; per-particle heap objects, strings, maps, callbacks, renderer handles, and arbitrary script/module state are not.
- Particle capacity is explicit and validated before simulation.
- Ordinary stepping must not build JSON, detailed snapshots, screenshots, or fingerprints unless explicitly requested.
- Keyed randomness must keep unrelated emitters and random channels isolated.
- The CPU cost report separates deterministic structural metrics from machine-dependent timing evidence.
- Never invent a portable "CPU percentage" from semantic operation counts.
- A coding agent may recommend CPU or GPU using documented measurements, but **must never change the backend automatically**.
- CPU -> GPU is a human decision represented by explicit reviewable authored/build configuration.
- `backend=cpu` remains CPU even if analysis recommends considering GPU.
- `backend=gpu` must fail clearly when unsupported; never silently fall back to CPU.
- GPU compilation minimizes runtime attributes from the verified ParticleProgram rather than copying the complete rich CPU reference layout.
- Normal GPU mode must not also run the full CPU reference simulation unless explicit conformance/debug mode requests dual execution.
- CPU is the exact semantic oracle; do not claim universal bit-identical floating-point GPU behavior across vendors/drivers without a separately proven numeric contract.
- Particle pixels are visual QA evidence, not the only correctness oracle.

## Scope control

Trace2D grows through narrow measured vertical slices, not by attempting to become a full general-purpose engine at once.

Do not make these implicit requirements unless `PROJECT_STATUS.md` and the active issue intentionally introduce them:

- full editor
- scripting language
- networking
- audio engine
- advanced lighting/PBR
- full-featured ECS
- custom allocator framework
- work-stealing job system
- broad platform support beyond tested baselines
- generic particle graph/editor
- gameplay-authoritative particle collision
- particle trails/sub-emitter recursion

## Documentation responsibilities

Update `PROJECT_STATUS.md` in the same PR whenever work changes any of these:

- current phase
- active PR
- completed release gate
- next execution order
- known blocker
- CI/build assumptions
- major architecture decision

Update `docs/PUBLIC_RELEASE.md` only when public release scope/gates change, not after every normal task.

Update architecture/design documents when a change affects module boundaries, determinism guarantees, authored formats, or the agent contract.

Particle child PRs must update `docs/PARTICLES.md` whenever they finalize or change particle semantic, cost-analysis, compiler, backend, or conformance contracts.

## Finishing a task

Before considering a PR complete:

- acceptance criteria are satisfied or explicitly called out
- tests pass
- CI is green or an external blocker is documented
- no generated/build artifacts are accidentally committed
- machine-readable output remains stable/deterministic where applicable
- `PROJECT_STATUS.md` reflects the new state
- next work is obvious to a fresh agent

The handoff standard is simple: another agent should not need the previous chat to know what to do next.
