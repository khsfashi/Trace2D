# Trace2D Project Status

Last repository-state update: **2026-08-08**

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

**P5 — Minimal SDL3 GPU 2D renderer and capture path**

Completed phase milestones:

- P0 project foundation — PR **#1**
- P1 SDL3 platform boundary — Issue **#2**, PR **#16**
- P1 deterministic fixed-step runtime — Issue **#3**, PR **#17**
- P2 stable entity identity / scene registry — Issue **#4**, PR **#18**
- P2 text-first deterministic scene format — Issue **#5**, PR **#19**
- P3 protocol-independent runtime inspection — Issue **#6**, PR **#20**
- P3 semantic selectors and runtime queries — Issue **#7**, PR **#21**
- P4 deterministic virtual input / frame scheduling — Issue **#8**, PR **#22**
- P4 deterministic gameplay test runner / assertions — Issue **#9**, PR **#23**
- P5 renderer/GPU presentation foundation — Issue **#10**, PR **#24**

P0-P4 are complete. PR **#24** is merged and establishes the first P5 vertical slice: renderer module ownership, SDL3 GPU device/swapchain integration, clear/present frame submission, baseline metrics, and explicit headless isolation. The next executable task remains inside Issue **#10**: implement the orthographic camera and minimum sprite render-data slice.

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
- SDL-free numeric window identity for renderer handoff; `SDL_Window*` remains private to SDL-backed implementation code
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

### Deterministic input

- `Trace2D::Input` owns gameplay-facing input state independently of SDL event objects
- fixed-size direct-indexed control state for A-Z, arrows, Space/Enter/Escape, and initial mouse buttons
- deterministic held / pressed / released transitions
- transient press/release flags clear on frame advancement while held state persists
- frame-indexed scheduled input with insertion-order stability for same-frame events
- `VirtualInputSource` supports immediate and scheduled test/agent injection
- physical SDL keyboard/mouse events translate into the same engine-owned `InputEvent` type
- gameplay code consumes one `InputSystem` state regardless of physical versus virtual origin
- reset clears control state, frame, scheduler cursor, and pending schedule while vector capacity remains reusable
- per-frame `ApplyEvent`, state lookup, transient clearing, and schedule consumption allocate no memory
- scheduling is setup work and may allocate/move vector elements
- tests cover immediate transitions, scheduled press/held/release, same-frame ordering, out-of-order schedule authoring, reset, invalid frame/control rejection, repeat determinism, and exact Runtime lockstep

See `docs/INPUT.md` for frame semantics and the physical/virtual input contract.

### Deterministic gameplay testing

- protocol-independent `Trace2D::Testing` module composes existing scene, runtime, input, and semantic-query systems
- scenario lifecycle supports authored-scene load, baseline reset, immediate/scheduled input, exact frame execution, semantic assertion, and structured report
- `RunFrames` advances input and runtime one frame at a time before invoking the gameplay update callback, preserving transient input semantics
- runner rejects pre-existing input/runtime frame divergence
- exact float-field assertion surface is intentionally limited to currently authoritative component state rather than adding reflection or guessed spatial state
- assertions reuse `AgentFacade::QueryOne` no-match/ambiguity behavior
- structured failures include stable code, selector, component/field, expected/observed values, frame, deterministic seed, detail, runtime snapshot, relevant input state, and resolved entity snapshot when available
- detailed failure/entity snapshots are materialized only at assertion/failure time; scenario execution does not add per-frame inspection allocation
- scenario report records immediate and scheduled input metadata for reproduction
- reset restores the baseline scene and clears runtime/input/report state
- repeated failing scenarios are covered by complete report equality checks
- `trace2d_gameplay_tests` is discovered by CTest and remains separate from the engine-level testing contract

See `docs/GAMEPLAY_TESTING.md` for the scenario, assertion, determinism, and failure-report contract.

### P5 renderer foundation — PR #24

- dedicated `Trace2D::Render` module with SDL3 linked privately
- renderer public API contains Trace2D-owned config/metrics types rather than SDL GPU handles
- platform retains SDL window ownership and exposes only a numeric window identifier
- renderer owns SDL3 GPU device lifetime and claims/releases the platform window swapchain
- current frame path acquires one command buffer, waits for a swapchain texture, clears/stores it in one render pass, and submits for presentation
- minimized/unavailable swapchain textures are valid no-presentation frames rather than simulation failures
- windowed `trace2d run` creates a renderer and submits one clear frame after explicit simulation stepping
- renderer exposes backend name plus submitted/presented-frame, render-pass, draw-call, sprite-count, and last-target-size metrics
- current `drawCalls` / `submittedSprites` remain zero until the sprite slice is implemented
- renderer rejects headless `Platform` before GPU initialization
- render-module tests cover headless window-ID behavior and GPU-independent rejection
- runtime/scene/input/agent/testing modules do not depend on `Trace2D::Render`

See `docs/RENDERING.md` for the ownership, frame-path, metrics, and P5 scope contract.

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
- PR **#22** final-head CI run **#47**: configure, Windows MSVC build, full CTest suite, deterministic virtual-input transition/scheduling tests, and runtime-lockstep input coverage successful before squash merge
- PR **#23** final-head CI run **#54**: configure, Windows MSVC build, and **55/55 CTest tests** successful, including scheduled/immediate gameplay input, structured assertion failure snapshots, query ambiguity propagation, reset restoration, and repeated-failure report determinism before squash merge
- PR **#24** final-head CI run **#64**: pinned SDL3 3.4.14 configure, Windows MSVC build, render/platform/runtime/CLI targets, render headless-isolation tests, and full CTest suite successful before squash merge

PR **#23** was squash-merged to `main` as commit `423c74ce7c3cdaacfc3333dacdba826ee5730abb` and Issue **#9** closed as completed.

PR **#24** was squash-merged to `main` as commit `637d8c2ab7839720f7b43e11e554f078b6a5c548`. Issue **#10** remains open because orthographic camera, textured sprites, batching/culling, offscreen rendering, and deterministic capture are still P5 work.

CI note for PR #24: run #63 initially failed while downloading `vcpkg.exe` with WinHTTP `0x00002F78`; rerunning the failed job passed bootstrap/configure. The subsequent build exposed only an `EXPECT_THROW` macro parsing issue in the new renderer test, which was fixed before final green run #64. Renderer and CLI targets had already compiled/linked successfully before that test-only fix.

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

### P4 — complete

- [x] **#8 — virtual input with frame scheduling** — PR **#22**
- [x] engine-level input state independent of SDL event objects
- [x] physical SDL and virtual test/agent sources feed the same gameplay-facing input state
- [x] deterministic press / release / held transitions
- [x] frame-indexed input scheduling
- [x] predictable reset between test scenarios
- [x] **#9 — deterministic gameplay test runner and assertions** — PR **#23**
- [x] scenario lifecycle: load / reset / run / report
- [x] semantic-selector assertions over authoritative component state
- [x] deterministic seed/input/frame metadata in reports
- [x] structured failure output with relevant runtime/entity/input snapshot
- [x] CI-visible gameplay scenario target
- [x] repeated failing scenarios reproduce identical reports

### P5 — in progress

- [ ] **#10 — minimal SDL3 GPU 2D renderer and capture path**
- [x] SDL3 GPU device and swapchain integration isolated from simulation ownership — PR **#24**
- [ ] orthographic camera
- [ ] textured sprite rendering
- [ ] measured sprite batching baseline
- [ ] visibility/culling baseline
- [ ] offscreen render target where supported
- [ ] deterministic screenshot capture at an explicitly requested simulation frame
- [x] basic renderer metrics suitable for profiling — PR **#24**
- [x] headless gameplay tests remain independent of GPU presentation — PR **#24**

## Next execution order

A fresh agent should work in this order unless a blocking dependency requires a documented change:

1. Continue **#10** with the orthographic camera + minimal sprite render-data vertical slice.
2. Add textured sprite rendering, then measure the simple batching/culling baseline before introducing renderer complexity.
3. Add offscreen rendering and deterministic frame-selected screenshot capture to complete **#10**.
4. Complete the minimal Public Alpha vertical-slice tracker before expanding broader P6 systems.
5. **#13 — P6: Add practical 2D engine slice for authored games** after the public-alpha minimum is stable.
6. **#11 — P7: Add JSON-RPC transport and MCP adapter over agent facade** after the protocol-independent loop is already proven.
7. **#12 — P8: Build end-to-end agent-authored sample game and portfolio demo** evolves into the polished public portfolio demonstration after the alpha loop works.

## Immediate next task

Remain inside **Issue #10** and implement the next smallest vertical slice:

- define a Trace2D-owned orthographic camera API with deterministic world-to-clip math
- define the minimum sprite render-data contract needed for one sample without making renderer state authoritative
- keep SDL/shader/resource handles private to `engine/render`
- add pure CPU unit tests for camera math and sprite ordering/culling decisions where practical
- do not begin texture-cache sophistication or batching optimizations until one sprite path exists and metrics can measure it

The following #10 outcomes remain after that slice:

- textured sprites in windowed mode
- measured batching baseline
- visibility/culling baseline
- offscreen render target
- deterministic frame-selected capture artifact

## Public Alpha blockers

The following capabilities are release blockers for `v0.1.0-alpha.1`:

- [x] deterministic headless execution
- [x] explicit frame stepping
- [x] stable text-authored scene/entity identity
- [x] structured runtime inspection
- [x] semantic selectors
- [x] virtual input
- [x] gameplay assertions
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
- SDL-specific ownership and types remain behind platform/rendering boundaries rather than entering core simulation contracts.
- `engine/platform` owns SDL initialization/window lifetime; renderer integration receives a Trace2D-owned numeric window ID rather than `SDL_Window*` in public APIs.
- `engine/render` may depend on `engine/platform` and SDL3, but runtime/scene/input/agent/testing do not depend on render presentation state.
- renderer-owned GPU device/swapchain/command-buffer state is presentation state and never authoritative simulation state.
- `engine/input` gameplay-facing state contains no SDL, CLI, JSON, or MCP types.
- Physical platform input and virtual test/agent input converge on the same `trace2d::input::InputEvent` / `InputSystem` path.
- Input hot-path state uses direct indexed storage; scheduled scenario authoring may allocate, frame consumption does not.
- `engine/runtime` has no SDL, renderer, CLI, JSON, or MCP dependency.
- `engine/scene` has no agent, testing, renderer, CLI, JSON, or MCP dependency unless a future authored render component is deliberately introduced at the scene/data layer with documented ownership.
- `engine/agent` may depend on runtime/scene, but runtime/scene never depend on `engine/agent`.
- `engine/testing` may compose agent/input/runtime/scene but those lower-level modules do not depend on testing.
- Gameplay-scenario execution does not request per-frame inspection snapshots; detailed failure context is materialized only at assertions/failures.
- Agent facade and gameplay-test result/error types contain no JSON, MCP, SDL, or LLM-specific protocol types.
- JSON serialization is an adapter/tool-boundary concern, not an engine/runtime concern.
- Inspection and semantic-query snapshot allocation happens only when explicitly requested; no per-frame copying is introduced.
- Semantic query result order follows deterministic scene observable order.
- Single-result semantic queries fail on ambiguity rather than choosing an arbitrary entity.
- MCP is never the source of truth for engine behavior.
- Headless and windowed execution share simulation/runtime logic.
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
- Rendering is presentation/QA state, not authoritative gameplay state.
- Optimization complexity follows measurement.

## Known decisions still open

Resolve these only when their implementation phase arrives:

- exact minimal sprite resource/data model and shader packaging strategy for P5
- exact offscreen/readback image format and capture artifact contract for P5
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
