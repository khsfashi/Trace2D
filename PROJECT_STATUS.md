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

The first public release proves this loop. It is not intended to be a complete Godot-like general-purpose engine.

## Current phase

**P5 — Minimal SDL3 GPU 2D renderer and deterministic capture path**

P0-P4 are complete. P5 has these merged slices:

- renderer / GPU presentation foundation — PR **#24**
- orthographic camera / CPU sprite render-data contract — PR **#25**
- textured sprite GPU submission baseline — PR **#26**
- ordered unbatched multi-sprite submission baseline — PR **#27**
- actual submission culling + submitted/culled metrics — PR **#28**
- allocation-free contiguous-texture batching opportunity measurement — PR **#29**

### Batching decision after PR #29

Issue **#10** required batching complexity to follow measurement. PR #29 added `MeasureContiguousTextureBatching`, which counts visible sprites, culled sprites, and contiguous visible texture runs without allocation, sorting, or painter-order changes.

The current executable windowed sample has one visible sprite:

```text
unbatched draw calls:       1
contiguous texture runs:    1
measured draw-call saving:  0
```

Therefore actual GPU instancing is **intentionally deferred** until the Public Alpha vertical sample contains a representative multi-sprite workload that demonstrates a real reduction. Do not add an instance-buffer/upload path merely to satisfy a checklist when the repository sample cannot measure a benefit.

The documented future batching candidate is contiguous same-texture instancing only, preserving caller order and using persistent/reused GPU + transfer buffers. See `docs/BATCHING.md`.

## Immediate next task — deterministic visual capture

Remain inside **Issue #10**.

Implement the smallest offscreen/readback path that can produce a deterministic artifact at an explicitly requested simulation frame.

Required constraints:

- simulation frame number, never wall-clock timing, selects the captured state
- rendering remains presentation/QA state and never becomes authoritative gameplay state
- preserve the current headless runtime/testing path without GPU initialization
- reuse persistent download/readback resources where practical; do not create expensive transfer resources every frame
- capture work occurs only when explicitly requested, not on every frame
- preserve existing sprite ordering, texture validation, culling, and render metrics semantics
- no hidden renderer-side frame-list copy or per-frame container growth
- use a deterministic image format/encoding contract suitable for CI and agent inspection
- keep the first implementation deliberately narrow; no render graph, async capture framework, video recording, or broad asset pipeline

Current SDL3 GPU direction already verified against the official API:

- render to an `SDL_GPU_TEXTUREUSAGE_COLOR_TARGET` texture
- use `SDL_DownloadFromGPUTexture` into a download `SDL_GPUTransferBuffer`
- submit with a fence when CPU readback must be consumed
- wait/query the fence before mapping/reading downloaded bytes
- reuse download buffers because SDL documents them as expensive to create repeatedly

The exact image artifact contract is still open. Prefer the least complex deterministic format that keeps byte layout and row pitch explicit.

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
- P5 textured sprite GPU submission — Issue **#10**, PR **#26**
- P5 ordered unbatched multi-sprite submission — Issue **#10**, PR **#27**
- P5 submission culling / culling metrics — Issue **#10**, PR **#28**
- P5 batching opportunity measurement / decision — Issue **#10**, PR **#29**

## Foundation currently established

### Project / build

- C++20 root CMake project
- shared local and CI CMake Presets
- pinned vcpkg baseline
- strict warning policy
- GoogleTest / CTest
- Windows GitHub Actions CI
- architecture, roadmap, public-release, and agent handoff documentation

### Platform / deterministic runtime

- SDL3 isolated behind `Trace2D::Platform`
- explicit headless and windowed startup modes
- SDL-free numeric window identity for renderer handoff
- deterministic fixed-step runtime
- explicit `Step(count)` simulation-frame control without sleeping
- runtime-owned frame, fixed timestep, simulation time, deterministic seed, and reset state
- wall-clock accumulation separated from explicit simulation stepping
- headless and windowed execution share simulation/runtime logic

### Scene / authored data

- `Trace2D::Scene` owns entity lifetime
- generation-safe runtime entity handles
- unique non-empty authored semantic IDs
- deterministic observable entity iteration
- text-first versioned TOML scene format
- strict schema validation with actionable diagnostics
- canonical deterministic serialization for stable Git diffs
- load -> save -> load semantic round-trip coverage

### Agent observability / gameplay automation

- protocol-independent `Trace2D::Agent` facade
- deterministic owned runtime/scene/entity/component snapshots
- semantic selectors for authored ID, name, tag, and authoritative component type
- deterministic query / single-result ambiguity semantics
- physical and virtual input converge on the same engine-owned input path
- deterministic held / pressed / released transitions
- frame-indexed virtual input scheduling
- deterministic gameplay scenario runner and exact frame assertions
- structured reproducible failure reports

See `docs/INSPECTION.md`, `docs/QUERY.md`, `docs/INPUT.md`, and `docs/GAMEPLAY_TESTING.md`.

## P5 renderer state

### PR #24 — renderer foundation

- dedicated `Trace2D::Render` module
- SDL3 GPU device and swapchain ownership isolated from simulation
- clear/store render pass and presentation path
- renderer metrics and driver name
- headless runtime/tests remain GPU-independent

### PR #25 — camera / render-data contract

- CPU-only `OrthographicCamera`, `OrthographicView`, and `SpriteRenderData`
- cached target-aspect / world-to-clip scale
- deterministic layer + stable-order comparator
- allocation-free inclusive AABB `IsSpriteVisible`
- no SDL/backend objects in CPU render-data contracts

### PR #26 — textured sprite GPU baseline

- Trace2D-owned 32-bit `TextureHandle`
- one-time RGBA8 upload and persistent renderer texture table
- persistent unit-quad vertex buffer, sampler, and graphics pipeline
- construction-time embedded-HLSL shader compilation through pinned `sdl3-shadercross`
- actual GPU draws increment `drawCalls` and `submittedSprites`

### PR #27 — ordered multi-sprite baseline

- `Renderer::RenderFrame(camera, std::span<const SpriteRenderData>)`
- non-owning input; no renderer-side frame-list copy
- one `OrthographicView` per non-empty presented frame
- caller-supplied order preserved; renderer does not sort
- explicit unbatched relationship: one visible sprite = one draw

### PR #28 — actual culling integration

- `IsSpriteVisible` fused into real submission
- culled sprites skip uniforms, texture/sampler bind, and draw
- relative visible order preserved
- pipeline/unit-quad binding skipped for fully culled frames
- cumulative `culledSprites` metric
- all supplied texture handles validated before command-buffer encoding
- no transient visible list

### PR #29 — batching measurement

- CPU-only `SpriteBatchMeasurement`
- `MeasureContiguousTextureBatching(view, sprites)` performs one O(N) allocation-free scan
- culling uses the same inclusive AABB helper as renderer submission
- candidate batches are contiguous texture runs in the post-culling visible sequence
- culled sprites do not split a visible run because they emit no presentation output
- no texture sorting, copying, GPU dependency, or container growth
- deterministic tests cover repeated textures, texture changes, culled gaps, and all-culled input
- current one-sprite executable sample measures no draw-call benefit, so actual instancing is deferred

## Current validation status

Validation uses clean GitHub-hosted Windows runners with the repository-pinned vcpkg baseline and MSVC configuration.

Validated P5 milestones:

- PR **#24** — CI **#64** green
- PR **#25** — CI **#68** green
- PR **#26** — CI **#71** green
- PR **#27** — CI **#74** green
- PR **#28** — CI **#78** green
- PR **#29** — CI **#81** green; Configure, Build, and full CTest all successful

Recent P5 squash merges:

- PR **#24** -> `637d8c2ab7839720f7b43e11e554f078b6a5c548`
- PR **#25** -> `6c54a64210fc0a8e28544c3e70b4e6c6575833c0`
- PR **#26** -> `e5a260577e6aa2d6666e13c00845940ba82e4c76`
- PR **#27** -> `897a03b76553a9bc5b674cc703d5efaf61047434`
- PR **#28** -> `9094d790ae99d6de3936ce044f9d930d8f614693`
- PR **#29** -> `3ff5a1164ff2e1b770552c2b4ab08d85cbef66ef`

Issue **#10** remains open because offscreen rendering and deterministic capture are still required. Actual GPU batching is measured/deferred rather than silently over-engineered.

Dependency note: `sdl3-shadercross` expands configure-time dependencies through DXC/spirv-cross. No shader compilation occurs in `RenderFrame`; change packaging only if measured startup, distribution, or CI cost justifies it.

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
- [x] ordered multi-sprite submission — PR **#27**
- [x] visibility/culling integrated into actual submission — PR **#28**
- [x] draw/submitted/culled metrics suitable for measurement
- [x] contiguous same-texture batching opportunity measured — PR **#29**
- [x] actual batching explicitly deferred until a representative workload shows savings
- [x] headless gameplay tests remain independent of GPU presentation
- [ ] offscreen color target suitable for readback
- [ ] deterministic screenshot capture at an explicitly requested simulation frame

## Next execution order

A fresh agent should work in this order unless a blocking dependency requires a documented change:

1. Continue **#10** with an offscreen color target suitable for deterministic readback.
2. Add explicit capture request data including the target simulation frame and artifact path/format contract.
3. Download the requested rendered frame through a reusable SDL3 GPU download transfer buffer and fence synchronization.
4. Add deterministic artifact validation that does not require gameplay state to be inferred from pixels.
5. Close **#10** once the explicit-frame capture acceptance criterion is met.
6. Complete the tiny Public Alpha vertical sample and release-quality repository checks tracked by **#14**.
7. Re-run `MeasureContiguousTextureBatching` on that sample; add contiguous same-texture instancing only if it demonstrates material savings.
8. Move into broader P6 systems only after the Public Alpha loop is stable.

## Public Alpha blockers

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
- [x] batching opportunity is measurable without changing painter order
- [ ] deterministic capture at a known simulation frame
- [ ] one tiny end-to-end sample proving the workflow
- [ ] clean Windows build/test documentation for the release candidate
- [ ] green release-candidate CI
- [ ] repository license and third-party license review before visibility changes to Public
- [ ] documentation that clearly distinguishes implemented features from planned features

See `docs/PUBLIC_RELEASE.md` for exact release gates.

## Explicit non-blockers for first public release

Do **not** delay Public Alpha for these unless release scope is intentionally changed:

- actual sprite instancing before a representative workload demonstrates savings
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
- `engine/platform` owns SDL initialization/window lifetime; renderer receives a Trace2D-owned numeric window ID.
- `engine/render` may depend on platform/SDL3, but runtime/scene/input/agent/testing do not depend on render presentation state.
- renderer GPU state is presentation state and never authoritative simulation state.
- CPU camera/sprite render data is Trace2D-owned, trivially copyable presentation input.
- persistent renderer resource creation/upload is explicit setup work; normal frame submission does not recreate persistent resources.
- multi-sprite submission consumes non-owning caller storage and does not copy/grow a renderer frame list.
- renderer submission preserves caller-provided painter order.
- texture identity never participates in global draw-order sorting.
- visibility/culling is a fused allocation-free O(N) submission filter.
- batching, when added, may only combine sprites already contiguous in the visible painter sequence unless equivalence is proven.
- renderer metrics are committed from actual successful GPU submission, not speculative work before a failed submit.
- texture validation semantics do not depend on camera visibility.
- capture must be explicit/requested work, not a per-frame tax.
- runtime has no SDL, renderer, CLI, JSON, or MCP dependency.
- agent/testing layers compose lower-level systems without reversing dependency direction.
- JSON/MCP remain adapter concerns, never engine truth.
- automated tests own simulation time through fixed-step control.
- authored scene/project state is text-first and deterministic.
- structured state beats pixel inference for gameplay QA.
- semantic selectors beat coordinate targeting where identity exists.
- optimization complexity follows measurement.

## Known decisions still open

Resolve these only when their implementation phase arrives:

- exact deterministic capture image format and row-pitch normalization contract
- whether capture renders directly into a fixed offscreen target or renders/copies from a target shared with presentation
- whether construction-time shadercross should be replaced by offline precompiled shader artifacts before Public Alpha; change only if measured cost justifies it
- exact minimal sample game used for Public Alpha
- project license before repository visibility changes to Public
- exact protocol/transport used before the later MCP adapter

The future batching mechanism is no longer an immediate open decision: if later measurement justifies it, use contiguous same-texture instancing with explicit persistent capacity/reuse rules unless new evidence supports a simpler equivalent.

## Handoff rule

Every PR that materially advances a phase must keep this file aligned with live repository state. At minimum keep these sections true:

- Current phase
- Immediate next task
- P5 renderer state
- Current validation status
- Phase exit criteria
- Next execution order
- Public Alpha blockers
- Architecture invariants
- Known decisions still open

A future conversation should be able to continue from this repository without relying on previous chat context.
