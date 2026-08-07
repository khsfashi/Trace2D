# Trace2D Project Status

Last repository-state update: **2026-08-07**

This document is the operational snapshot for the next contributor or coding agent. Live repository state wins over stale prose.

## Current mission

Reach **v0.1.0-alpha.1 Public Alpha** with a complete minimal agent-first development loop:

```text
text-authored scene
  -> build
  -> deterministic headless run
  -> explicit frame step
  -> semantic state query
  -> virtual input
  -> gameplay assertion
  -> 2D render
  -> frame-specific visual capture
```

The first public release proves this loop. It is not intended to be a complete general-purpose game engine.

## Current phase

**P1 — Deterministic runtime foundation**

P0 project foundation is complete. PR **#1 — Bootstrap Trace2D project foundation** was squash-merged to `main` after green CI.

Issue **#2 — Add SDL3 platform boundary and startup modes** is complete. PR **#16** passed CI and was squash-merged to `main`.

The next executable task is Issue **#3 — Implement deterministic fixed-step runtime control**.

P0 established:

- C++20 root CMake project
- MSVC warning policy
- shared CMake Presets
- pinned vcpkg baseline
- platform-independent `Trace2D::Core`
- `trace2d` CLI bootstrap
- machine-readable `doctor --json`
- GoogleTest / CTest
- Windows CI
- coding style/editor settings
- architecture, roadmap, public-release, ADR, and agent handoff documentation

P1 now additionally has:

- SDL3 pinned through the project vcpkg manifest
- SDL3 hidden behind `Trace2D::Platform`
- RAII SDL subsystem/window ownership with final SDL cleanup
- explicit headless and windowed startup modes
- engine-owned quit event translation
- `trace2d run --headless|--windowed [--json]` startup smoke path
- CI-safe headless platform and CLI tests

## Current validation status

The P0 bootstrap is validated on a clean GitHub-hosted Windows runner.

An initial CI failure exposed a toolchain assumption: GitHub `windows-latest` uses a Visual Studio 2026 runner while the local developer preset intentionally targets Visual Studio 2022. The project now separates:

- local `windows-msvc`: Visual Studio 2022
- CI `ci-windows-msvc`: Visual Studio 2026 generator with `v143` toolset

The final PR #1 CI run (**#12**) completed successfully through dependency install, configure, build, and test before merge.

PR **#16** latest-head CI run (**#19**) also completed successfully through pinned vcpkg install, SDL3 configure, Windows MSVC build, and all GoogleTest/CTest checks before squash merge.

## P0 exit criteria

- [x] C++20 project structure
- [x] CMake Presets
- [x] vcpkg manifest/baseline
- [x] warning policy
- [x] core library bootstrap
- [x] CLI bootstrap
- [x] unit-test integration
- [x] CI workflow created
- [x] architecture and roadmap documented
- [x] agent operating/handoff structure documented
- [x] clean-checkout CI passes configure/build/test
- [x] PR #1 squash-merged to `main`

## P1 progress

- [x] **#2 — SDL3 platform boundary and startup modes** — PR #16 merged after green CI
- [ ] **#3 — deterministic fixed-step runtime control** — next executable task

## Next execution order

A fresh agent should work in this order unless a blocking dependency requires a documented change:

1. **#3 — P1: Implement deterministic fixed-step runtime control**
2. **#4 — P2: Define stable entity identity and scene registry**
3. **#5 — P2: Add text-first scene format and deterministic serialization**
4. **#6 — P3: Build protocol-independent runtime inspection API**
5. **#7 — P3: Add semantic selectors and runtime queries**
6. **#8 — P4: Implement virtual input with frame scheduling**
7. **#9 — P4: Add deterministic gameplay test runner and assertions**
8. **#10 — P5: Implement minimal SDL3 GPU 2D renderer and capture path**
9. Complete the minimal Public Alpha vertical-slice tracker before expanding broader P6 systems.
10. **#13 — P6: Add practical 2D engine slice for authored games** after the public-alpha minimum is stable.
11. **#11 — P7: Add JSON-RPC transport and MCP adapter over agent facade** after the protocol-independent loop is already proven.
12. **#12 — P8: Build end-to-end agent-authored sample game and portfolio demo** evolves into the polished public portfolio demonstration after the alpha loop works.

## Immediate next task

**Issue #3 — Implement deterministic fixed-step runtime control.**

Build `engine/runtime` on top of the platform boundary without moving simulation ownership into SDL or the CLI.

Required outcomes from Issue #3:

- fixed simulation timestep configuration
- monotonic wall-clock abstraction for interactive/windowed mode
- explicit simulation frame counter
- `Step(count)` API that advances without sleeping
- deterministic seed ownership/reset point
- windowed loop may accumulate wall-clock time while tests advance exact frames directly
- tests for zero, one, and multi-frame stepping
- repeated runs from the same initial state/seed report identical frame/state results

Do not begin scene/entity work in Issue #4 until #3 has a green merged PR unless a blocking dependency requires a documented change.

## Public Alpha blockers

The following capabilities are release blockers for `v0.1.0-alpha.1`:

- deterministic headless execution
- explicit frame stepping
- stable text-authored scene/entity identity
- structured runtime inspection
- semantic selectors
- virtual input
- gameplay assertions
- minimal sprite renderer
- capture at a known simulation frame
- one tiny end-to-end sample proving the workflow
- clean Windows build/test documentation
- green CI
- repository license and third-party license review before visibility changes to Public
- documentation that clearly distinguishes implemented features from planned features

See `docs/PUBLIC_RELEASE.md` for exact release gates.

## Explicit non-blockers for first public release

Do **not** delay Public Alpha for these unless release scope is intentionally changed:

- MCP adapter
- full editor
- Box2D feature completeness
- semantic UI tree completeness
- networking/audio
- job system
- custom allocator framework
- advanced renderer/lighting
- Linux/macOS support

## Architecture invariants currently in force

- `engine/core` has no SDL dependency.
- SDL-specific ownership and types remain behind `engine/platform`.
- MCP is never the source of truth for engine behavior.
- Headless and windowed execution share runtime logic.
- Automated tests own simulation time through fixed-step control.
- Authored scene/project state is text-first.
- Structured state beats pixel inference for gameplay QA.
- Semantic selectors beat coordinate-based targeting where identity exists.
- Optimization complexity follows measurement.

## Known decisions still open

Resolve these only when their implementation phase arrives:

- exact authored scene syntax/serialization library
- exact protocol/transport used before MCP adapter
- exact minimal sample game used for Public Alpha
- project license before the repository becomes Public
- whether public alpha uses Box2D or a simpler engine-owned collision slice

When one is decided, record the rationale in architecture documentation or an ADR and remove it from this list.

## Handoff rule

Every PR that materially advances a phase should update this file so these sections remain true:

- Current phase
- Current validation status
- Exit criteria
- Next execution order
- Public Alpha blockers
- Known decisions still open

A future conversation should be able to continue from this repository without relying on previous chat context.
