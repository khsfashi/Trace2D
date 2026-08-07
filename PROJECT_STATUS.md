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

**P4 — Deterministic interaction and gameplay-test foundation**

Completed phase milestones:

- P0 project foundation — PR **#1**
- P1 SDL3 platform boundary — Issue **#2**, PR **#16**
- P1 deterministic fixed-step runtime — Issue **#3**, PR **#17**
- P2 stable entity identity / scene registry — Issue **#4**, PR **#18**
- P2 text-first deterministic scene format — Issue **#5**, PR **#19**
- P3 protocol-independent runtime inspection — Issue **#6**, PR **#20**
- P3 semantic selectors and runtime queries — Issue **#7**, PR **#21**

P3 is complete. The next executable task is **Issue #8 — P4: Implement virtual input with frame scheduling**.

Do not begin Issue #9 gameplay assertions before #8 is merged and green unless a blocking dependency requires a documented change.

## Foundation currently established

### Project / build

- C++20 root CMake project
- shared local and CI CMake Presets
- pinned vcpkg baseline
- strict warning policy
- GoogleTest / CTest
- Windows GitHub Actions CI
- coding style/editor configuration
- architecture, roadmap, public-release, ADR, and agent handoff documentation

### Platform / deterministic runtime

- SDL3 isolated behind `Trace2D::Platform`
- explicit headless and windowed startup modes
- engine-owned quit-event translation
- deterministic `Trace2D::Runtime`
- explicit `Step(count)` simulation-frame control without sleeping
- runtime-owned frame, fixed timestep, simulation time, deterministic seed, and reset state
- wall-clock accumulation separated from explicit simulation stepping
- monotonic `steady_clock` wrapper for later interactive-loop integration
- `trace2d run --frames N --seed N` machine-controlled smoke path

### Scene / authored data

- `Trace2D::Scene` owns entity lifetime
- generation-safe `EntityId` runtime handles using slot index + generation
- stale-handle invalidation across destruction and slot reuse
- unique non-empty authored semantic IDs separated from runtime handles
- authored scene semantic ID and human-readable scene name
- human-readable entity names and normalized semantic tags
- mutable `Transform2D` state
- deterministic observable runtime iteration in ascending slot-index order
- allocation-free runtime entity iteration without a temporary result collection
- versioned TOML `*.trace2d.toml` authored scene format
- strict schema validation with actionable diagnostics
- canonical serialization with fixed field order and semantic-ID entity ordering
- locale-independent float formatting sufficient for 32-bit float round trips
- load -> save -> load semantic round-trip coverage

### Agent observability / semantic queries

- `Trace2D::Agent` protocol-independent facade over runtime and scene state
- non-owning active runtime/scene binding without reversing dependency direction
- owned runtime, scene, entity, transform, nullable-bounds, and generic component-field snapshots
- stable engine-level inspection and query error codes independent of JSON/MCP
- deterministic entity/tag/component/field ordering in inspection snapshots
- `Transform2D` exposed as a direct snapshot and initial typed generic component fields
- bounds explicitly nullable until renderer/physics state can provide authoritative values
- `trace2d inspect --scene PATH --frames N --seed N [--json]`
- exact, case-sensitive semantic selector grammar:
  - `#<semantic-id>`
  - `name:<entity-name>`
  - `tag:<tag>`
  - `type:<component-type>`
- `AgentFacade::Query` multi-result API with deterministic scene-order results
- zero-match multi-query is a successful empty result
- `AgentFacade::QueryOne` requires exactly one match and returns `no_match` / `ambiguous_match` instead of choosing arbitrarily
- `QueryOne` scans deterministically and materializes at most one entity snapshot, avoiding full ambiguous-result snapshot allocation
- invalid selector syntax returns stable structured `invalid_selector` diagnostics
- `type:Transform2D` is the initial authoritative component-type selector; spatial queries remain deferred until authoritative bounds exist
- `trace2d query --scene PATH --selector SELECTOR [--one] [--frames N] [--seed N] [--json]`
- query and inspection JSON serialization remain CLI/tool-boundary concerns, not engine contracts
- inspection/query allocations happen only when explicitly requested; no per-frame observation work was introduced
- initial query implementation intentionally uses a deterministic linear live-entity scan rather than maintaining an unmeasured index/cache

See `docs/INSPECTION.md` and `docs/QUERY.md` for the current public contracts.

## Current validation status

The project is validated on clean GitHub-hosted Windows runners using the repository's pinned vcpkg baseline and MSVC toolchain configuration.

The project separates:

- local `windows-msvc`: Visual Studio 2022
- CI `ci-windows-msvc`: Visual Studio 2026 generator with `v143` toolset

Validated milestones:

- PR **#1** final CI run **#12**: configure, build, test successful before merge
- PR **#16** final-head CI run **#19**: SDL3 configure, Windows MSVC build, and all tests successful before merge
- PR **#17** final-head CI run **#24**: configure, build, runtime tests, pre-existing tests, and 120-frame headless CLI smoke successful before squash merge
- PR **#18** final-head CI run **#28**: configure, Windows MSVC build, and full CTest suite including scene lifecycle/determinism tests successful before squash merge
- PR **#19** final-head CI run **#35**: configure, Windows MSVC build, and full CTest suite including text-scene validation/deterministic round-trip tests successful before squash merge
- PR **#20** final-head CI run **#40**: configure, Windows MSVC build, full CTest suite, agent inspection unit tests, and deterministic CLI inspect fixture successful before squash merge
- PR **#21** final-head CI run **#44**: configure, Windows MSVC build, full CTest suite, semantic selector/query unit tests, and deterministic CLI query coverage successful before squash merge

## Phase exit criteria

### P0 — complete

- [x] C++20 project structure
- [x] CMake Presets
- [x] pinned vcpkg manifest/baseline
- [x] warning policy
- [x] core library and CLI bootstrap
- [x] unit-test integration
- [x] Windows CI
- [x] architecture / roadmap / agent handoff documentation

### P1 — complete

- [x] **#2 — SDL3 platform boundary and startup modes**
- [x] **#3 — deterministic fixed-step runtime control**
- [x] headless execution requires no visible window
- [x] explicit frame stepping does not sleep or read wall-clock time
- [x] deterministic seed/reset ownership lives in runtime
- [x] runtime has no SDL, CLI, JSON, or MCP dependency

### P2 — complete

- [x] **#4 — stable entity identity / scene registry**
- [x] generation-safe stale-handle rejection
- [x] semantic authored identity separated from runtime handles
- [x] deterministic allocation-free observable entity iteration
- [x] **#5 — text-first scene format / deterministic serialization**
- [x] actionable scene validation diagnostics
- [x] semantic round-trip preservation
- [x] stable serialization ordering for Git diffs

### P3 — complete

- [x] **#6 — protocol-independent runtime inspection API**
- [x] structured runtime/scene/entity state inspection without parsing logs or pixels
- [x] stable protocol-independent inspection schema/errors
- [x] deterministic CLI `inspect --json`
- [x] **#7 — semantic selectors / runtime queries** — PR **#21**
- [x] authored ID, name, tag, and initial component-type selectors
- [x] deterministic single-result and multi-result query APIs
- [x] clear no-match, ambiguity, invalid-syntax, and unavailable-scene errors
- [x] deterministic CLI `query --json`
- [x] query tests cover ID, name, tag, type, no-match, multi-match, ambiguity, invalid syntax, and deterministic ordering

### P4 — in progress

- [ ] **#8 — virtual input with frame scheduling**
- [ ] engine-level input state independent of SDL event objects
- [ ] physical SDL and virtual test/agent sources feed the same gameplay-facing input state
- [ ] deterministic press / release / held transitions
- [ ] frame-indexed input scheduling
- [ ] predictable reset between test scenarios
- [ ] **#9 — deterministic gameplay test runner and assertions**

## Next execution order

A fresh agent should work in this order unless a blocking dependency requires a documented change:

1. **#8 — P4: Implement virtual input with frame scheduling**
2. **#9 — P4: Add deterministic gameplay test runner and assertions**
3. **#10 — P5: Implement minimal SDL3 GPU 2D renderer and capture path**
4. Complete the minimal Public Alpha vertical-slice tracker before expanding broader P6 systems.
5. **#13 — P6: Add practical 2D engine slice for authored games** after the public-alpha minimum is stable.
6. **#11 — P7: Add JSON-RPC transport and MCP adapter over agent facade** after the protocol-independent loop is already proven.
7. **#12 — P8: Build end-to-end agent-authored sample game and portfolio demo** evolves into the polished public portfolio demonstration after the alpha loop works.

## Immediate next task

**Issue #8 — P4: Implement virtual input with frame scheduling.**

Goal: allow tests and agents to inject input independently of physical devices and schedule it against deterministic simulation frames.

Required outcomes:

- introduce gameplay-facing engine input state that contains no SDL event types
- provide a physical SDL adapter and a virtual input source that converge on the same engine state
- support key/button press, release, and held state
- support frame-indexed scheduling against deterministic simulation frames
- expose a minimal test API or CLI boundary for injecting virtual input
- make reset semantics explicit and deterministic between scenarios
- test exact press -> held -> release transitions across explicitly stepped frames
- test scheduled events at known frame indices
- preserve current runtime determinism and dependency direction

Implementation guidance:

- keep physical-device translation at the platform/input adapter boundary; gameplay code must not branch on physical versus virtual origin
- define frame semantics precisely before implementation (for example, when an event scheduled for frame N becomes visible relative to `Step`)
- avoid per-frame heap allocation for steady input-state updates; scheduling may allocate when a scenario is authored, but consumption should use stable prebuilt state where practical
- use compact state representations and direct indexed lookup for the small initial key/button domain rather than maps or strings in the simulation hot path
- do not make SDL scancodes, JSON values, CLI argument structures, or future MCP types the engine input contract
- do not begin #9 assertion-runner work until #8 is green and merged unless a blocking dependency requires a documented exception

## Public Alpha blockers

The following capabilities are release blockers for `v0.1.0-alpha.1`:

- [x] deterministic headless execution
- [x] explicit frame stepping
- [x] stable text-authored scene/entity identity
- [x] structured runtime inspection
- [x] semantic selectors
- [ ] virtual input
- [ ] gameplay assertions
- [ ] minimal sprite renderer
- [ ] capture at a known simulation frame
- [ ] one tiny end-to-end sample proving the workflow
- [ ] clean Windows build/test documentation for the release candidate
- [ ] green release-candidate CI
- [ ] repository license and third-party license review before visibility changes to Public
- [ ] documentation that clearly distinguishes implemented features from planned features

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
- `engine/scene` has no agent, CLI, JSON, or MCP dependency.
- `engine/agent` may depend on runtime/scene, but runtime/scene never depend on `engine/agent`.
- Agent facade result/error types contain no JSON, MCP, SDL, or LLM-specific protocol types.
- JSON serialization is an adapter/tool-boundary concern, not an engine/runtime concern.
- Inspection and semantic-query snapshot allocation happens only when explicitly requested; no per-frame copying is introduced.
- Semantic query result order follows deterministic scene observable order.
- Single-result semantic queries fail on ambiguity rather than choosing an arbitrary entity.
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

Every PR that materially advances a phase should keep this file aligned with live repository state. If a phase PR is merged before the handoff edit, follow it immediately with a status-only commit as done for previous milestones.

At minimum keep these sections true:

- Current phase
- Current validation status
- Phase exit criteria
- Next execution order
- Immediate next task
- Public Alpha blockers
- Architecture invariants
- Known decisions still open

A future conversation should be able to continue from this repository without relying on previous chat context.
