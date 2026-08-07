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

Do not silently reinterpret a deliberate architecture constraint. If a constraint must change, document why in the same PR.

## How to choose the next task

1. If `PROJECT_STATUS.md` lists an active PR, finish or repair that PR first.
2. Never start a new feature while the active bootstrap/foundation PR has failing CI unless the failure is unrelated and explicitly documented.
3. Otherwise select the first unblocked issue in the "Next execution order" section of `PROJECT_STATUS.md`.
4. Work on one coherent issue/PR at a time unless two issues are inseparable.
5. Prefer the smallest vertical slice that leaves the repository runnable and testable.

Do not jump ahead to attractive later features such as MCP, an editor, advanced rendering, custom allocators, or lock-free infrastructure while earlier release gates are incomplete.

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

## Scope control before first public release

The first public release is a proof of the agent-first loop, not a Godot replacement.

Do not make these release blockers unless `docs/PUBLIC_RELEASE.md` is intentionally revised:

- full editor
- scripting language
- networking
- audio engine
- advanced lighting/PBR
- full-featured ECS
- custom allocator framework
- work-stealing job system
- MCP integration
- broad platform support beyond the tested Windows baseline

## Documentation responsibilities

Update `PROJECT_STATUS.md` in the same PR whenever work changes any of these:

- current phase
- active PR
- completed release gate
- next execution order
- known blocker
- CI/build assumptions
- major architecture decision

Update `docs/PUBLIC_RELEASE.md` only when the public release scope/gates change, not after every normal task.

Update architecture/design documents when a change affects module boundaries, determinism guarantees, authored formats, or the agent contract.

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
