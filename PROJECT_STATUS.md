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

**P3 — Structured observability foundation**

P0 project foundation is complete. PR **#1 — Bootstrap Trace2D project foundation** was squash-merged to `main` after green CI.

P1 deterministic runtime foundation is complete:

- Issue **#2 — Add SDL3 platform boundary and startup modes** completed in PR **#16**
- Issue **#3 — Implement deterministic fixed-step runtime control** completed in PR **#17**

P2 scene and authored-data foundation is complete:

- Issue **#4 — Define stable entity identity and scene registry** completed in PR **#18**
- Issue **#5 — Add text-first scene format and deterministic serialization** completed in PR **#19**

The next executable task is **Issue #6 — Build protocol-independent runtime inspection API**.

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

P1 established:

- SDL3 pinned through the project vcpkg manifest
- SDL3 hidden behind `Trace2D::Platform`
- RAII SDL subsystem/window ownership with final SDL cleanup
- explicit headless and windowed startup modes
- engine-owned quit event translation
- deterministic `Trace2D::Runtime` boundary
- explicit `Step(count)` simulation-frame control without sleeping
- runtime-owned frame, fixed timestep, simulation time, and deterministic seed/reset state
- wall-clock accumulation separated from explicit stepping
- monotonic `steady_clock` wrapper for later interactive-loop integration
- `trace2d run --frames N --seed N` machine-controlled runtime smoke path
- unit coverage for zero/one/multi-frame stepping, exact 120-frame advancement, reset determinism, wall-clock remainder, and monotonic clock behavior

P2 established:

- `Trace2D::Scene` module with scene-owned entity lifetime
- generation-safe `EntityId` runtime handles using slot index + generation
- stale-handle invalidation across destruction and slot reuse
- unique non-empty semantic IDs separated from runtime handles
- authored scene semantic ID and human-readable scene name
- human-readable entity names and normalized semantic tags
- mutable `Transform2D` state
- deterministic observable runtime iteration in ascending slot-index order
- allocation-free runtime entity iteration without a temporary result collection
- TOML version-1 authored scene format using `*.trace2d.toml`
- `LoadSceneToml` / `SaveSceneToml` without exposing toml++ types in public headers
- strict schema validation for required IDs, types, unknown fields, duplicate semantic IDs, tags, and transforms
- actionable diagnostics with semantic field paths and source line/column when available
- canonical serialization with fixed field order and entities sorted lexicographically by semantic ID
- locale-independent float formatting sufficient for 32-bit float round trips
- runtime-only entities without semantic identity rejected from authored serialization
- lifecycle, identity, tag, transform, stale-handle, reuse, deterministic-order, schema-validation, and round-trip tests
- scene-format and architecture documentation for identity, iteration, and serialization rules

## Current validation status

The project is validated on clean GitHub-hosted Windows runners using the repository's pinned vcpkg baseline and MSVC toolchain configuration.

The project separates:

- local `windows-msvc`: Visual Studio 2022
- CI `ci-windows-msvc`: Visual Studio 2026 generator with `v143` toolset

Validated milestones:

- PR **#1** final CI run **#12**: configure, build, test successful before merge
- PR **#16** final-head CI run **#19**: SDL3 configure, Windows MSVC build, and all tests successful before merge
- PR **#17** final-head CI run **#24**: configure, build, runtime tests, pre-existing tests, and 120-frame headless CLI smoke all successful before squash merge
- PR **#18** final-head CI run **#28**: pinned dependencies, configure, Windows MSVC build, and full CTest suite including scene lifecycle/determinism tests all successful before squash merge
- PR **#19** final-head CI run **#35**: pinned dependencies including toml++, configure, Windows MSVC build, and full CTest suite including text-scene validation/deterministic round-trip tests all successful before squash merge

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

## P1 exit criteria

- [x] **#2 — SDL3 platform boundary and startup modes** — PR #16 merged after green CI
- [x] **#3 — deterministic fixed-step runtime control** — PR #17 merged after green CI
- [x] headless execution does not require a visible window
- [x] explicit fixed-frame stepping does not sleep or read wall-clock time
- [x] deterministic seed/reset ownership is in the runtime layer
- [x] wall-clock accumulation is separate from explicit stepping
- [x] runtime code has no SDL, CLI, JSON, or MCP dependency

## P2 exit criteria

- [x] **#4 — stable entity identity / scene registry** — PR #18 merged after green CI
- [x] scene owns entity creation/destruction and runtime handle validity
- [x] stale handles are rejected after destruction and slot reuse
- [x] authored semantic IDs are distinct from generation-safe runtime handles
- [x] observable runtime entity iteration order is deterministic and allocation-free
- [x] **#5 — text-first scene format / deterministic serialization** — PR #19 merged after green CI
- [x] minimal authored scene can be loaded entirely from text
- [x] invalid scene input produces actionable diagnostics
- [x] load -> save -> load preserves semantic state
- [x] serialization output ordering is stable for Git diffs
- [x] authored scene syntax and serialization dependency are documented

## P3 progress

- [ ] **#6 — protocol-independent runtime inspection API**
- [ ] active runtime frame and scene state can be inspected through Trace2D-owned types
- [ ] entity identity, name, tags, transform, and initial component fields are inspectable without parsing logs or pixels
- [ ] inspection result/error schema is stable and independent of JSON/MCP
- [ ] initial CLI `inspect` boundary can emit deterministic structured JSON
- [ ] **#7 — semantic selectors / runtime queries**

## Next execution order

A fresh agent should work in this order unless a blocking dependency requires a documented change:

1. **#6 — P3: Build protocol-independent runtime inspection API**
2. **#7 — P3: Add semantic selectors and runtime queries**
3. **#8 — P4: Implement virtual input with frame scheduling**
4. **#9 — P4: Add deterministic gameplay test runner and assertions**
5. **#10 — P5: Implement minimal SDL3 GPU 2D renderer and capture path**
6. Complete the minimal Public Alpha vertical-slice tracker before expanding broader P6 systems.
7. **#13 — P6: Add practical 2D engine slice for authored games** after the public-alpha minimum is stable.
8. **#11 — P7: Add JSON-RPC transport and MCP adapter over agent facade** after the protocol-independent loop is already proven.
9. **#12 — P8: Build end-to-end agent-authored sample game and portfolio demo** evolves into the polished public portfolio demonstration after the alpha loop works.

## Immediate next task

**Issue #6 — Build protocol-independent runtime inspection API.**

Required outcomes from Issue #6:

- add an `engine/agent` facade over authoritative runtime/scene state
- inspect the active scene and runtime frame without coupling engine internals to CLI, JSON, MCP, or a specific LLM
- expose entity identity, transform, tags, bounds, and initial component fields through stable Trace2D-owned result types
- define stable result/error structures suitable for later adapters
- add JSON serialization only at the tool boundary
- expose the initial CLI `inspect` command with deterministic output
- return stable non-zero exit behavior and actionable diagnostics for inspection failures

Preserve the dependency direction: the protocol-independent agent facade may depend on runtime/scene, but runtime/scene must not depend on JSON, CLI, or MCP. Do not make MCP the source of truth for inspection behavior.

Do not begin Issue #7 selector work until #6 has a green merged PR unless a blocking dependency requires a documented change.

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
- `engine/runtime` has no SDL, CLI, JSON, or MCP dependency.
- MCP is never the source of truth for engine behavior.
- Headless and windowed execution share runtime logic.
- Automated tests own simulation time through fixed-step control.
- Authored scene/project state is text-first.
- Authored scene files use versioned TOML and serialize into a canonical deterministic representation.
- toml++ remains private to the scene text implementation boundary.
- Runtime entity identity uses generation-safe handles; automation-facing identity must not be a raw pointer.
- Non-empty semantic IDs are unique within a scene and distinct from runtime handles.
- Observable runtime entity iteration uses deterministic ascending slot-index order.
- Authored serialization ordering is independent of runtime slot order and uses semantic IDs.
- Structured state beats pixel inference for gameplay QA.
- Semantic selectors beat coordinate-based targeting where identity exists.
- Optimization complexity follows measurement.

## Known decisions still open

Resolve these only when their implementation phase arrives:

- exact protocol/transport used before MCP adapter
- exact minimal sample game used for Public Alpha
- project license before the repository becomes Public
- whether public alpha uses Box2D or a simpler engine-owned collision slice

When one is decided, record the rationale in architecture documentation or an ADR and remove it from this list.

## Handoff rule

Every PR that materially advances a phase should update this file so these sections remain true:

- Current phase
- Current validation status
- Exit criteria/progress
- Next execution order
- Public Alpha blockers
- Known decisions still open

A future conversation should be able to continue from this repository without relying on previous chat context.
