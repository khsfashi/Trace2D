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

P0 project foundation is complete in PR **#1 — Bootstrap Trace2D project foundation** and is ready for squash merge after the final CI run for this status update is green.

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

## Current validation status

The bootstrap CI is validated on a clean GitHub-hosted Windows runner.

An initial CI failure exposed a toolchain assumption: GitHub `windows-latest` uses a Visual Studio 2026 runner while the local developer preset intentionally targets Visual Studio 2022. The project now separates:

- local `windows-msvc`: Visual Studio 2022
- CI `ci-windows-msvc`: Visual Studio 2026 generator with `v143` toolset

CI run **#11** completed successfully through vcpkg install, configure, build, and test. Any later commit to PR #1 must also finish with green CI before merge.

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
- [x] PR #1 ready for squash merge to `main`

## Next execution order

A fresh agent should work in this order unless a blocking dependency requires a documented change:

1. **#2 — P1: Add SDL3 platform boundary and startup modes**
2. **#3 — P1: Implement deterministic fixed-step runtime control**
3. **#4 — P2: Define stable entity identity and scene registry**
4. **#5 — P2: Add text-first scene format and deterministic serialization**
5. **#6 — P3: Build protocol-independent runtime inspection API**
6. **#7 — P3: Add semantic selectors and runtime queries**
7. **#8 — P4: Implement virtual input with frame scheduling**
8. **#9 — P4: Add deterministic gameplay test runner and assertions**
9. **#10 — P5: Implement minimal SDL3 GPU 2D renderer and capture path**
10. Complete the minimal Public Alpha vertical-slice tracker before expanding broader P6 systems.
11. **#13 — P6: Add practical 2D engine slice for authored games** after the public-alpha minimum is stable.
12. **#11 — P7: Add JSON-RPC transport and MCP adapter over agent facade** after the protocol-independent loop is already proven.
13. **#12 — P8: Build end-to-end agent-authored sample game and portfolio demo** evolves into the polished public portfolio demonstration after the alpha loop works.

## Immediate next task

**Issue #2 — Add SDL3 platform boundary and startup modes.**

The goal is to introduce SDL3 without leaking SDL types or lifetime rules into `engine/core`, while establishing both interactive/windowed and headless startup paths that share authoritative runtime logic.

Do not begin Issue #3 until #2 has a green merged PR unless the two tasks are deliberately combined and the reason is documented.

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
