# Trace2D Project Status

Last repository-state update: **2026-08-08**

This document is the operational handoff for the next contributor or coding agent. Live repository state wins over stale prose.

## Current mission

Reach **v0.1.0-alpha.1 Public Alpha** with one complete minimal agent-first 2D development loop:

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

P0-P4 are complete. P5 currently has the following merged slices:

- renderer/GPU presentation foundation — Issue **#10**, PR **#24**
- orthographic camera / CPU sprite render-data contract — Issue **#10**, PR **#25**
- textured sprite GPU submission baseline — Issue **#10**, PR **#26**
- ordered unbatched multi-sprite submission baseline — Issue **#10**, PR **#27**
- actual submission culling + culled/submitted metrics — Issue **#10**, PR **#28**

The next executable task remains inside Issue **#10**: introduce the **smallest sprite batching change justified by the measured unbatched/culling contract**. After batching, finish P5 with an offscreen color target and deterministic explicit-frame capture.

## Completed phase milestones

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
- P5 orthographic camera / sprite render-data contract — Issue **#10**, PR **#25**
- P5 textured sprite GPU submission baseline — Issue **#10**, PR **#26**
- P5 ordered unbatched multi-sprite submission baseline — Issue **#10**, PR **#27**
- P5 submission culling / culling metrics — Issue **#10**, PR **#28**

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
- deterministic fixed-step runtime
- explicit `Step(count)` simulation-frame control without sleeping
- runtime-owned frame, fixed timestep, simulation time, deterministic seed, and reset state
- wall-clock accumulation separated from explicit simulation stepping
- headless and windowed execution share the same simulation/runtime logic

### Scene / authored data

- `Trace2D::Scene` owns entity lifetime
- generation-safe runtime entity handles
- unique non-empty authored semantic IDs separated from runtime handles
- deterministic observable entity iteration
- text-first versioned TOML scene format
- strict schema validation with actionable diagnostics
- canonical deterministic serialization for stable Git diffs
- load -> save -> load semantic round-trip coverage

### Agent observability / semantic queries

- protocol-independent `Trace2D::Agent` facade over runtime and scene state
- deterministic owned runtime/scene/entity/component snapshots
- semantic selectors for authored ID, name, tag, and authoritative component type
- deterministic multi-result queries
- strict single-result no-match / ambiguity semantics
- inspection/query allocation only when explicitly requested
- JSON remains at CLI/tool boundaries rather than runtime contracts

See `docs/INSPECTION.md` and `docs/QUERY.md`.

### Deterministic input / gameplay testing

- engine-owned gameplay input state independent of SDL event objects
- physical and virtual input converge on the same `InputEvent` / `InputSystem` path
- deterministic held / pressed / released transitions
- frame-indexed scheduled virtual input
- no allocation in normal per-frame input consumption
- protocol-independent gameplay scenario runner
- exact frame execution and semantic component-field assertions
- structured deterministic failure reports with seed/frame/input/runtime/entity context
- repeated failing scenarios reproduce identical reports

See `docs/INPUT.md` and `docs/GAMEPLAY_TESTING.md`.

## P5 renderer state

### PR #24 — renderer foundation

- dedicated `Trace2D::Render` module
- SDL3 GPU device and swapchain ownership isolated from simulation
- renderer claims/releases the platform window through an SDL-free public `WindowId`
- clear/store render pass and presentation path
- renderer metrics and driver name
- headless runtime/tests remain GPU-independent

### PR #25 — camera / render-data contract

- CPU-only `OrthographicCamera`, `OrthographicView`, and `SpriteRenderData`
- camera view computes target aspect once and caches reciprocal clip scales
- `WorldToClip` uses subtraction/multiplication only per position
- deterministic layer + stable-order comparator
- allocation-free inclusive AABB `IsSpriteVisible`
- no SDL/backend objects in CPU render-data contracts

### PR #26 — textured sprite GPU baseline

- Trace2D-owned 32-bit `TextureHandle`
- one-time RGBA8 texture upload and persistent renderer texture table
- persistent unit-quad vertex buffer, sampler, and graphics pipeline
- construction-time embedded-HLSL shader compilation through pinned `sdl3-shadercross`
- one visible sprite = one direct six-vertex draw
- actual GPU draws increment `drawCalls` and `submittedSprites`
- no persistent GPU resource creation or texture upload in `RenderFrame`

### PR #27 — ordered unbatched multi-sprite baseline

- `Renderer::RenderFrame(camera, std::span<const SpriteRenderData>)`
- single-sprite overload delegates to the span path
- non-owning input; no renderer-side frame-list copy
- one `OrthographicView` per non-empty presented frame
- caller-supplied sprite order is preserved; renderer does not sort
- persistent graphics pipeline and unit-quad vertex buffer bound once for the unculled baseline
- every supplied sprite draws once: `drawCalls == submittedSprites == N`
- no batching or culling in this baseline by design

### PR #28 — actual culling integration

- the existing CPU `IsSpriteVisible` AABB test is fused into real sprite submission
- one visibility decision per supplied sprite
- culled sprites perform no uniform push, texture/sampler bind, or draw
- relative order of visible sprites is preserved
- pipeline + unit-quad vertex buffer bind lazily on the first visible sprite and are skipped when every sprite is culled
- cumulative `RenderMetrics::culledSprites` added beside `drawCalls` and `submittedSprites`
- CLI human/JSON output exposes the culling metric
- all supplied texture handles are validated before command-buffer encoding so invalid-input behavior does not depend on camera visibility
- culling builds no transient visible list and adds no renderer-side frame allocation

Current successful-frame measurement contract before batching:

```text
visible sprite count = submittedSprites delta = drawCalls delta
culled sprite count  = culledSprites delta
supplied sprite count = visible + culled
```

Once batching is introduced, `submittedSprites` must continue to describe encoded visible sprites while `drawCalls` may become smaller.

See `docs/RENDERING.md` and `docs/ROADMAP.md` for the current renderer contract and order of work.

## Current validation status

Validation uses clean GitHub-hosted Windows runners with the repository-pinned vcpkg baseline and MSVC configuration.

Validated milestones:

- PR **#1** — CI **#12** green
- PR **#16** — CI **#19** green
- PR **#17** — CI **#24** green
- PR **#18** — CI **#28** green
- PR **#19** — CI **#35** green
- PR **#20** — CI **#40** green
- PR **#21** — CI **#44** green
- PR **#22** — CI **#47** green
- PR **#23** — CI **#54** green; full gameplay-test suite successful
- PR **#24** — CI **#64** green; renderer/platform/runtime/CLI build and full CTest successful
- PR **#25** — CI **#68** green; CPU camera/order/culling tests successful
- PR **#26** — CI **#71** green; shadercross dependency configure, warning-clean MSVC build, full CTest successful
- PR **#27** — CI **#74** green; ordered multi-sprite baseline build and full CTest successful
- PR **#28** — CI **#78** green; pinned dependency configure, warning-clean MSVC build, and full CTest successful

Recent P5 squash merges:

- PR **#24** -> `637d8c2ab7839720f7b43e11e554f078b6a5c548`
- PR **#25** -> `6c54a64210fc0a8e28544c3e70b4e6c6575833c0`
- PR **#26** -> `e5a260577e6aa2d6666e13c00845940ba82e4c76`
- PR **#27** -> `897a03b76553a9bc5b674cc703d5efaf61047434`
- PR **#28** -> `9094d790ae99d6de3936ce044f9d930d8f614693`

Issue **#10** remains open because batching, offscreen rendering, and deterministic capture are still P5 work.

Dependency note: `sdl3-shadercross` expands configure-time dependencies through DXC/spirv-cross. No shader compilation occurs in `RenderFrame`; switch to offline artifacts only if measured startup, distribution, or CI cost justifies the packaging complexity.

## Phase exit criteria

### P0-P4 — complete

- [x] project/build foundation
- [x] SDL3 platform boundary
- [x] deterministic fixed-step runtime
- [x] stable entity identity / deterministic scene registry
- [x] text-first deterministic scene format
- [x] structured inspection / semantic queries
- [x] deterministic virtual input
- [x] deterministic gameplay scenario runner / assertions

### P5 — in progress

- [ ] **#10 — minimal SDL3 GPU 2D renderer and capture path**
- [x] SDL3 GPU device / swapchain integration — PR **#24**
- [x] orthographic camera / CPU sprite render data — PR **#25**
- [x] textured sprite GPU submission — PR **#26**
- [x] ordered unbatched multi-sprite baseline — PR **#27**
- [x] visibility/culling integrated into actual submission — PR **#28**
- [x] draw/submitted/culled renderer metrics suitable for baseline measurement — PRs **#24**, **#26**, **#27**, **#28**
- [x] headless gameplay tests remain independent of GPU presentation
- [ ] smallest justified sprite batching implementation
- [ ] offscreen render target where supported
- [ ] deterministic screenshot capture at an explicitly requested simulation frame

## Next execution order

A fresh agent should work in this order unless a blocking dependency requires a documented change:

1. Continue **#10** with the smallest measurable sprite batching implementation justified by the PR #27/#28 baseline.
2. Measure the post-batching relationship between `submittedSprites` and `drawCalls`; preserve ordering and culling semantics.
3. Add an offscreen color target suitable for visual QA.
4. Add deterministic frame-selected readback/capture and complete **#10**.
5. Complete the tiny Public Alpha vertical sample and release-quality repository checks tracked by **#14**.
6. Move into broader P6 systems only after the Public Alpha loop is stable.

## Immediate next task — measured sprite batching

Remain inside **Issue #10**.

Implement the smallest batching change that reduces draw calls without invalidating the deterministic ordering contract.

Required constraints:

- preserve caller-provided painter order; batching must never reorder across sprites when that could change alpha-blended output
- keep existing CPU culling before per-sprite GPU submission work
- no renderer-side per-frame heap allocation or container growth
- do not create/destroy persistent GPU resources per frame
- retain `submittedSprites` as visible sprite count and make `drawCalls` represent actual encoded draw calls
- retain `culledSprites` semantics unchanged
- keep headless runtime/tests GPU-independent
- add CPU-testable batching/grouping contract coverage where practical
- record before/after metrics using a deterministic sample workload

Prefer a deliberately narrow batching model. A strong first candidate is **contiguous same-texture runs only**, because it preserves painter order without a texture sort. If SDL GPU instancing or a small persistent/reused instance buffer is used, establish explicit capacity/reuse rules and avoid frame-time growth. Do not introduce atlas packing, bindless descriptors, global texture sorting, render graphs, custom allocator frameworks, or a broad material system for this slice.

Before implementation, inspect the current SDL3 GPU API and existing renderer data path and choose the least complex approach that produces a measurable draw-call reduction while keeping ownership and allocation rules intact.

## Public Alpha blockers

The following capabilities are release blockers for `v0.1.0-alpha.1`:

- [x] deterministic headless execution
- [x] explicit frame stepping
- [x] stable text-authored scene/entity identity
- [x] structured runtime inspection
- [x] semantic selectors
- [x] virtual input
- [x] gameplay assertions
- [x] minimal textured sprite renderer
- [x] ordered multi-sprite render submission
- [x] actual renderer culling baseline
- [ ] deterministic capture at a known simulation frame
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
- SDL-specific ownership/types stay behind platform/rendering boundaries.
- `engine/platform` owns SDL initialization/window lifetime; public renderer handoff uses a Trace2D-owned numeric window ID rather than `SDL_Window*`.
- `engine/render` may depend on platform/SDL3, but runtime/scene/input/agent/testing do not depend on render presentation state.
- renderer GPU state is presentation state and never authoritative simulation state.
- CPU camera/sprite render data is Trace2D-owned, trivially copyable presentation input with scalar texture identity rather than backend pointers.
- persistent renderer resource creation/upload is explicit setup work; frame submission does not recreate persistent resources.
- multi-sprite submission consumes non-owning caller storage and must not copy/grow a renderer frame list.
- renderer submission preserves caller-provided order unless a future ordering contract is explicitly documented and proven equivalent.
- visibility/culling is a fused allocation-free O(N) submission filter; it does not build a transient visible list.
- renderer metrics are committed from actual successful GPU submission, not speculative work before a failed submit.
- texture validation semantics must not silently depend on camera visibility.
- input hot-path state uses direct indexed storage; scenario scheduling may allocate, frame consumption does not.
- runtime has no SDL, renderer, CLI, JSON, or MCP dependency.
- agent/testing layers compose lower-level systems without reversing dependency direction.
- JSON/MCP remain adapter concerns, never engine truth.
- headless and windowed execution share simulation/runtime logic.
- automated tests own simulation time through fixed-step control.
- authored scene/project state is text-first and deterministic.
- structured state beats pixel inference for gameplay QA.
- semantic selectors beat coordinate targeting where identity exists.
- rendering is presentation/QA state, not authoritative gameplay state.
- optimization complexity follows measurement.

## Known decisions still open

Resolve these only when their implementation phase arrives:

- exact batching mechanism after inspecting the current SDL3 GPU instancing/buffer tradeoffs; preserve painter order and no-growth hot-path rules
- whether construction-time shadercross should be replaced by offline precompiled shader artifacts before Public Alpha; change only if measured cost justifies it
- exact offscreen/readback image format and capture artifact contract for P5
- exact minimal sample game used for Public Alpha
- project license before the repository becomes Public
- exact protocol/transport used before the later MCP adapter

When one is decided, record the rationale in architecture documentation or an ADR and remove it from this list.

## Handoff rule

Every PR that materially advances a phase must keep this file aligned with live repository state. If a phase PR is merged before the handoff edit, follow it immediately with a status-only commit.

At minimum keep these sections true:

- Current phase
- P5 renderer state
- Current validation status
- Phase exit criteria
- Next execution order
- Immediate next task
- Public Alpha blockers
- Architecture invariants
- Known decisions still open

A future conversation should be able to continue from this repository without relying on previous chat context.
