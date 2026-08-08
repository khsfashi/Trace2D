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

**Public Alpha vertical slice and release-quality repository work — Issue #14**

P0-P5 are complete. Issue **#10** is closed after PR **#31** completed the explicit simulation-frame GPU capture path.

P5 merged slices:

- renderer / GPU presentation foundation — PR **#24**
- orthographic camera / CPU sprite render-data contract — PR **#25**
- textured sprite GPU submission baseline — PR **#26**
- ordered unbatched multi-sprite submission baseline — PR **#27**
- actual submission culling + submitted/culled metrics — PR **#28**
- allocation-free contiguous-texture batching opportunity measurement — PR **#29**
- persistent offscreen color target + swapchain presentation copy — PR **#30**
- explicit-frame GPU readback + deterministic BMP artifact — PR **#31**

## Immediate next task — tiny Public Alpha vertical sample

Work inside **Issue #14**.

Build the smallest sample that proves the already-implemented loop end to end. Do not start broad P6 engine work yet.

Required sample properties:

- text-authored sample scene committed to the repository
- one semantic controlled entity such as `#player`
- deterministic movement or similarly obvious gameplay state change
- headless automated gameplay test using the existing virtual-input and assertion path
- windowed rendering of the same sample state
- explicit frame-specific capture using the P5 capture API/CLI
- one documented agent workflow covering edit -> build -> run -> inspect -> input -> assert -> capture
- enough visible sprites to make the renderer sample representative enough to re-run the existing batching measurement

Keep the sample intentionally tiny. Prefer proving composition of existing systems over introducing new engine abstractions.

After the sample exists, run `MeasureContiguousTextureBatching` on that representative workload. Add contiguous same-texture instancing only if the measurement demonstrates a material draw-call saving. Actual instancing remains a non-blocker for Public Alpha.

## P5 decisions now closed

### Batching decision

PR #29 added `MeasureContiguousTextureBatching`, which counts visible sprites, culled sprites, and contiguous visible texture runs without allocation, sorting, or painter-order changes.

The previous executable sample had one visible sprite:

```text
unbatched draw calls:       1
contiguous texture runs:    1
measured draw-call saving:  0
```

Therefore actual GPU instancing remains **intentionally deferred** until the Public Alpha sample provides representative evidence. If later justified, the preferred first mechanism is contiguous same-texture instancing that preserves painter order and uses persistent/reused GPU and transfer resources. See `docs/BATCHING.md`.

### Offscreen presentation/capture source

PR #30 moved rendering off the swapchain and onto a renderer-owned color target:

- target format matches the claimed swapchain format
- dimensions use the actual acquired presentation target
- target is lazily created and reused at steady size
- resize replacement is create-before-release
- render pass uses SDL GPU color-target cycling
- completed output is copied to the same-format swapchain texture
- swapchain remains presentation-only and is never the readback source

### Deterministic capture contract

PR #31 completed the narrow Public Alpha capture path:

- `CaptureRequest` carries explicit `simulationFrame`, artifact path, and image format
- `trace2d run --windowed --frames N --capture PATH` steps simulation explicitly and captures the resulting `RuntimeState.frame`
- wall-clock timing never selects the captured gameplay frame
- the renderer reuses one download transfer buffer and grows it only when required
- readback rows use a 256-byte aligned pitch
- requested capture work is encoded from the completed offscreen target
- capture submission acquires a GPU fence; CPU mapping happens only after fence completion
- RGBA/BGRA SDR target bytes normalize into packed top-down canonical RGBA8
- the first artifact format is a deterministic dependency-free 32-bit top-down BMP
- capture bytes remain presentation/QA output, never authoritative gameplay state
- ordinary non-capture frames perform no download, capture fence wait, transfer-buffer mapping, image normalization, or capture file I/O

Deterministic capture means deterministic simulation-frame selection, canonical byte layout, and deterministic artifact encoding. It does not claim bit-identical floating-point rasterization across unrelated GPU vendors/backends.

See `docs/RENDERING.md` for the full contract.

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
- P5 persistent offscreen color target / presentation copy — Issue **#10**, PR **#30**
- P5 explicit-frame GPU readback / deterministic artifact — Issue **#10**, PR **#31**

## Foundation currently established

### Project / build

- C++20 root CMake project
- shared local and CI CMake Presets
- pinned vcpkg baseline
- strict warning policy
- GoogleTest / CTest
- Windows GitHub Actions CI
- architecture, roadmap, public-release, rendering, and agent handoff documentation

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
- deterministic gameplay scenario runner and exact-frame assertions
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
- one visible sprite = one draw in the current unbatched path

### PR #28 — actual culling integration

- `IsSpriteVisible` fused into real submission
- culled sprites skip uniforms, texture/sampler bind, and draw
- relative visible order preserved
- persistent pipeline/unit-quad binding skipped for fully culled frames
- cumulative `culledSprites` metric
- all supplied texture handles validated before command-buffer encoding
- no transient visible list

### PR #29 — batching measurement

- CPU-only `SpriteBatchMeasurement`
- one O(N), allocation-free scan
- culling uses the same visibility helper as renderer submission
- candidate batches are contiguous texture runs in the post-culling visible sequence
- no texture sorting, copying, GPU dependency, or container growth

### PR #30 — offscreen render / presentation copy

- persistent renderer-owned offscreen `SDL_GPUTexture`
- size-aware lazy creation/replacement
- completed offscreen image copied to the swapchain
- swapchain remains presentation-only
- presentation transfer does not alter sprite draw metrics

### PR #31 — explicit deterministic capture

- public capture request/frame result contract
- persistent capacity-reused download transfer buffer
- aligned row-pitch calculation with overflow guards
- SDL GPU texture download + fence synchronization
- canonical packed RGBA8 CPU result
- deterministic 32-bit BMP writer
- exact simulation-frame CLI capture
- CPU tests for layout, artifact bytes, frame identity, and overflow rejection
- normal render path remains free of capture-only synchronization and I/O

## Current validation status

Validation uses clean GitHub-hosted Windows runners with the repository-pinned vcpkg baseline and MSVC configuration.

Validated P5 milestones:

- PR **#24** — CI **#64** green
- PR **#25** — CI **#68** green
- PR **#26** — CI **#71** green
- PR **#27** — CI **#74** green
- PR **#28** — CI **#78** green
- PR **#29** — CI **#81** green
- PR **#30** — CI **#84** green
- PR **#31** — final CI **#90** green; Configure, Build, and full CTest all successful

Recent P5 squash merges:

- PR **#24** -> `637d8c2ab7839720f7b43e11e554f078b6a5c548`
- PR **#25** -> `6c54a64210fc0a8e28544c3e70b4e6c6575833c0`
- PR **#26** -> `e5a260577e6aa2d6666e13c00845940ba82e4c76`
- PR **#27** -> `897a03b76553a9bc5b674cc703d5efaf61047434`
- PR **#28** -> `9094d790ae99d6de3936ce044f9d930d8f614693`
- PR **#29** -> `3ff5a1164ff2e1b770552c2b4ab08d85cbef66ef`
- PR **#30** -> `0bc95bb907e79e6fc9af81c23856b854fc99ec76`
- PR **#31** -> `a8b674894d418bd6fa478a2d30bfa362076c8a4a`

Issue **#10** is closed as completed. Actual GPU batching remains measured/deferred rather than silently over-engineered.

Dependency note: `sdl3-shadercross` expands configure-time dependencies through DXC/spirv-cross. No shader compilation occurs in `RenderFrame`; change packaging only if measured startup, distribution, or CI cost justifies it.

## Phase exit criteria

### P0-P5 — complete

- [x] project/build foundation
- [x] SDL3 platform boundary
- [x] deterministic fixed-step runtime
- [x] stable entity identity / deterministic scene registry
- [x] text-first deterministic scene format
- [x] structured inspection / semantic queries
- [x] deterministic virtual input
- [x] deterministic gameplay scenario runner / assertions
- [x] **#10 — minimal SDL3 GPU 2D renderer and capture path**
- [x] SDL3 GPU device / swapchain integration
- [x] orthographic camera / CPU sprite render data
- [x] textured ordered multi-sprite submission
- [x] visibility/culling + renderer metrics
- [x] batching opportunity measured and actual instancing explicitly deferred pending evidence
- [x] offscreen color target suitable for presentation/capture
- [x] explicit capture request bound to a simulation frame
- [x] GPU download/readback with reusable transfer resource + fence synchronization
- [x] deterministic screenshot artifact generation/validation
- [x] headless gameplay tests remain independent of GPU presentation

### Public Alpha — in progress

- [ ] tiny end-to-end sample proving the full automation loop
- [ ] documented edit -> build -> run -> inspect -> input -> assert -> capture workflow
- [ ] repository license selected and added
- [ ] third-party license review completed
- [ ] repository/history secret and private-path review completed
- [ ] README quick start verified from a clean clone
- [ ] implemented/planned features clearly distinguished
- [ ] Public Alpha limitations documented
- [ ] release-candidate main CI green
- [ ] create `v0.1.0-alpha.1`
- [ ] change repository visibility to Public
- [ ] verify README/release from public view

## Next execution order

A fresh agent should work in this order unless a blocking dependency requires a documented change:

1. Continue **#14** by choosing and committing the tiny Public Alpha sample scene.
2. Give the controlled entity a stable semantic selector such as `#player`.
3. Add the smallest deterministic gameplay behavior needed to make a visible state change; reuse existing runtime/input/scene/testing surfaces rather than introducing a broad gameplay framework.
4. Add a headless automated scenario proving exact-frame input and state assertion for the sample.
5. Render the same sample state in windowed mode and capture an explicitly selected simulation frame.
6. Document the complete agent workflow: edit -> build -> run -> inspect -> input -> assert -> capture.
7. Re-run `MeasureContiguousTextureBatching` against the representative sample. Implement contiguous same-texture instancing only if measured savings are material.
8. Perform repository-quality gates: license, third-party licenses, secrets/private paths, clean-clone quick start, implemented-vs-planned wording, limitations.
9. Run release-candidate CI and mark `PROJECT_STATUS.md` release-ready.
10. Create `v0.1.0-alpha.1`, change visibility to Public, verify public view, then open follow-up issues for known limitations.

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
- [x] offscreen render target suitable for visual readback
- [x] deterministic capture at a known simulation frame
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
- Linux/macOS/mobile support

## Architecture invariants currently in force

- `engine/core` has no SDL dependency.
- SDL-specific ownership/types stay behind platform/rendering boundaries.
- `engine/platform` owns SDL initialization/window lifetime; renderer receives a Trace2D-owned numeric window ID.
- `engine/render` may depend on platform/SDL3, but runtime/scene/input/agent/testing do not depend on render presentation state.
- renderer GPU state is presentation state and never authoritative simulation state.
- CPU camera/sprite render data is Trace2D-owned presentation input.
- size-independent persistent renderer resources are setup work; size/capacity-dependent offscreen/readback resources are reused and replaced only when required.
- steady-size normal frame submission does not recreate persistent GPU resources.
- normal non-capture frames perform no capture download, fence wait, mapping, normalization, or file I/O.
- multi-sprite submission consumes non-owning caller storage and does not copy/grow a renderer frame list.
- renderer submission preserves caller-provided painter order.
- texture identity never participates in global draw-order sorting.
- visibility/culling is a fused allocation-free O(N) submission filter.
- batching may only combine sprites already contiguous in the visible painter sequence unless equivalence is proven.
- renderer metrics are committed from actual successful GPU submission, not speculative work before a failed submit.
- texture validation semantics do not depend on camera visibility.
- swapchain is presentation-only; capture/readback sources the renderer-owned offscreen target.
- capture frame selection uses simulation frame identity, never wall-clock timing.
- canonical captured CPU pixels are packed top-down RGBA8; BMP is an artifact adapter rather than engine truth.
- runtime has no SDL, renderer, CLI, JSON, or MCP dependency.
- agent/testing layers compose lower-level systems without reversing dependency direction.
- JSON/MCP remain adapter concerns, never engine truth.
- automated tests own simulation time through fixed-step control.
- authored scene/project state is text-first and deterministic.
- structured state beats pixel inference for gameplay QA.
- semantic selectors beat coordinate targeting where identity exists.
- optimization complexity follows measurement.

## Known decisions still open

Resolve these only when their implementation/release phase arrives:

- exact tiny sample game/content used for Public Alpha
- whether representative sample measurements justify contiguous same-texture instancing before or after Public Alpha
- whether construction-time shadercross should later be replaced by offline precompiled shader artifacts; change only if measured startup/distribution/CI cost justifies it
- project license before repository visibility changes to Public
- exact protocol/transport used before the later MCP adapter

The capture source, canonical pixel layout, first artifact format, and synchronous first capture API are no longer open decisions; PR #31 records those contracts in `docs/RENDERING.md`.

## Handoff rule

Every PR that materially advances a phase must keep this file aligned with live repository state. At minimum keep these sections true:

- Current phase
- Immediate next task
- Current validation status
- Phase exit criteria
- Next execution order
- Public Alpha blockers
- Architecture invariants
- Known decisions still open

A future conversation should be able to continue from this repository without relying on previous chat context.
