# Trace2D Project Status

Last repository-state update: **2026-08-07**

This document is the operational snapshot for the next contributor or coding agent. It should stay concise and current. Long-term intent belongs in the architecture/roadmap documents.

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

**P0 — Project foundation**

Current active work:

- Draft PR **#1 — Bootstrap Trace2D project foundation**
- Branch: `agent/project-bootstrap`

P0 currently includes:

- C++20 root CMake project
- MSVC warning policy
- CMake Presets
- pinned vcpkg baseline
- platform-independent `Trace2D::Core`
- `trace2d` CLI bootstrap
- machine-readable `doctor --json`
- GoogleTest / CTest
- Windows CI
- coding style/editor settings
- architecture, roadmap, agent-first principles
- repository agent operating guide

## Current validation status

The first CI attempt exposed an environment assumption: GitHub `windows-latest` now uses a Visual Studio 2026 runner while the local preset intentionally targets Visual Studio 2022.

The bootstrap branch now separates:

- local `windows-msvc`: Visual Studio 2022
- CI `ci-windows-msvc`: Visual Studio 2026 generator with `v143` toolset

The active PR must not be merged until its latest CI run is green.

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
- [ ] latest clean-checkout CI passes configure/build/test
- [ ] PR #1 merged to `main`

## Next execution order

A fresh agent should work in this order unless a blocking dependency requires a documented change:

1. **Finish PR #1** and make CI green.
2. **#2 — P1: Add SDL3 platform boundary and startup modes**
3. **#3 — P1: Implement deterministic fixed-step runtime control**
4. **#4 — P2: Define stable entity identity and scene registry**
5. **#5 — P2: Add text-first scene format and deterministic serialization**
6. **#6 — P3: Build protocol-independent runtime inspection API**
7. **#7 — P3: Add semantic selectors and runtime queries**
8. **#8 — P4: Implement virtual input with frame scheduling**
9. **#9 — P4: Add deterministic gameplay test runner and assertions**
10. **#10 — P5: Implement minimal SDL3 GPU 2D renderer and capture path**
11. Complete the minimal Public Alpha vertical-slice tracker before expanding broader P6 systems.
12. **#13 — P6: Add practical 2D engine slice for authored games** after the public-alpha minimum is stable.
13. **#11 — P7: Add JSON-RPC transport and MCP adapter over agent facade** after the protocol-independent loop is already proven.
14. **#12 — P8: Build end-to-end agent-authored sample game and portfolio demo** evolves into the polished public portfolio demonstration after the alpha loop works.

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
- authored scene/project state is text-first.
- structured state beats pixel inference for gameplay QA.
- semantic selectors beat coordinate-based targeting where identity exists.
- optimization complexity follows measurement.

## Known decisions still open

These must be resolved when their implementation phase arrives, not prematurely:

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
