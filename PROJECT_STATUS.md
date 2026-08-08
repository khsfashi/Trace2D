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

That loop is now implemented by the committed Public Alpha sample. The remaining release work is a narrow measured renderer optimization decision plus repository-quality/release gates. The first public release is not intended to be a complete Godot-like general-purpose engine.

## Current phase

**Public Alpha release preparation — Issue #14**

P0-P5 are complete. The first end-to-end Public Alpha sample is complete through PR **#32**.

Recent completed milestones:

- P5 renderer / GPU presentation foundation — PR **#24**
- P5 orthographic camera / CPU sprite render-data contract — PR **#25**
- P5 textured sprite GPU submission baseline — PR **#26**
- P5 ordered unbatched multi-sprite baseline — PR **#27**
- P5 actual submission culling + metrics — PR **#28**
- P5 allocation-free contiguous-texture batching opportunity measurement — PR **#29**
- P5 persistent offscreen color target + swapchain presentation copy — PR **#30**
- P5 explicit-frame GPU readback + deterministic BMP artifact — PR **#31**
- Public Alpha vertical sample / full automation loop — PR **#32**

## Public Alpha vertical sample — complete

Committed sample:

```text
samples/public_alpha/public_alpha.trace2d.toml
```

Documented workflow:

```text
docs/PUBLIC_ALPHA_SAMPLE.md
```

The sample proves the complete implemented loop without a graphical editor:

- text-authored TOML scene
- stable semantic controlled entity `#player`
- deterministic `KeyD` virtual input scheduled against simulation frames
- deterministic movement from authoritative input state
- exact-frame semantic assertion on `Transform2D.position.x`
- headless CTest path with no GPU initialization
- windowed rendering of the same final authoritative scene state
- explicit simulation-frame BMP capture
- structured inspect/query workflow before gameplay execution

Default sample contract:

```text
frames:                     8
seed:                       42
KeyD press frame:           2
KeyD release frame:         6
#player.position.x:          4.0
visible sprites:             7
contiguous texture runs:     2
candidate draw-call saving:  5
```

PR **#32** CI **#93** passed Configure, Build, and full CTest on the clean GitHub-hosted Windows/MSVC runner.

## Immediate next task — contiguous same-texture instancing

The previous one-sprite workload did not justify batching complexity. PR #32 changed the evidence:

```text
unbatched visible draws:     7
contiguous texture runs:     2
measured candidate saving:   5
```

This is material enough to implement the already-documented first batching mechanism: **contiguous same-texture instancing that preserves painter order**.

Keep this slice narrow. Do not introduce a render graph, global texture sorting, generic material system, frame allocator, ECS rewrite, or broad asset pipeline.

Required constraints:

- preserve the caller-provided visible painter sequence
- only combine sprites that are already contiguous and share the same texture
- culled sprites emit no draw and must not change visible ordering semantics
- keep texture validation semantics unchanged
- use persistent/reused GPU and transfer/upload resources; grow capacity only when required
- do not allocate/copy/grow a renderer-owned visible-sprite list every frame
- do not sort by texture
- ordinary steady-capacity frames must not recreate buffers
- renderer metrics must report actual successful submitted draw calls and sprites
- headless/runtime/scene/input/agent/testing layers remain renderer-independent
- add CPU/backend-independent coverage where possible and GPU-facing tests only where the existing CI environment can validate them reliably
- re-run the committed Public Alpha workload and record before/after draw-call evidence

If implementation complexity becomes disproportionate to the measured five-draw saving, document the blocker and defer it; actual instancing remains a Public Alpha non-blocker.

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
- P5 ordered multi-sprite submission — Issue **#10**, PR **#27**
- P5 submission culling / culling metrics — Issue **#10**, PR **#28**
- P5 batching opportunity measurement / decision — Issue **#10**, PR **#29**
- P5 persistent offscreen color target / presentation copy — Issue **#10**, PR **#30**
- P5 explicit-frame GPU readback / deterministic artifact — Issue **#10**, PR **#31**
- Public Alpha end-to-end vertical sample — Issue **#14**, PR **#32**

## Foundation currently established

### Project / build

- C++20 root CMake project
- shared local and CI CMake Presets
- pinned vcpkg baseline
- strict warning policy
- GoogleTest / CTest
- Windows GitHub Actions CI

### Platform / deterministic runtime

- SDL3 isolated behind `Trace2D::Platform`
- explicit headless and windowed startup modes
- deterministic fixed-step runtime
- explicit `Step(count)` simulation-frame control without sleeping
- observable frame, fixed timestep, simulation time, deterministic seed, and reset state
- wall-clock accumulation separated from explicit simulation stepping

### Scene / authored data

- generation-safe runtime entity handles
- stable non-empty authored semantic IDs
- deterministic observable entity iteration
- text-first versioned TOML scene format
- strict schema validation with actionable diagnostics
- canonical deterministic serialization for stable Git diffs

### Agent observability / gameplay automation

- protocol-independent `Trace2D::Agent` facade
- deterministic runtime/scene/entity/component snapshots
- semantic selectors by authored ID, name, tag, and authoritative component type
- deterministic query / single-result ambiguity semantics
- physical and virtual input converge on the same engine-owned input path
- frame-indexed virtual input scheduling
- deterministic gameplay scenario runner and exact-frame assertions
- structured reproducible failure reports

### Renderer / visual QA

- SDL3 GPU renderer isolated behind `Trace2D::Render`
- CPU-only orthographic camera and sprite render-data contracts
- ordered multi-sprite textured submission
- fused allocation-free visibility/culling filter
- actual submitted/culled/draw metrics
- allocation-free contiguous-texture batching measurement
- persistent offscreen render target used for presentation and capture source
- explicit simulation-frame GPU readback and deterministic BMP artifact
- persistent/reused capture download transfer buffer

## Current validation status

Validation uses clean GitHub-hosted Windows runners with the repository-pinned vcpkg baseline and MSVC configuration.

Validated recent milestones:

- PR **#24** — CI **#64** green
- PR **#25** — CI **#68** green
- PR **#26** — CI **#71** green
- PR **#27** — CI **#74** green
- PR **#28** — CI **#78** green
- PR **#29** — CI **#81** green
- PR **#30** — CI **#84** green
- PR **#31** — CI **#90** green
- PR **#32** — CI **#93** green; Configure, Build, and full CTest successful

Recent squash merges:

- PR **#24** -> `637d8c2ab7839720f7b43e11e554f078b6a5c548`
- PR **#25** -> `6c54a64210fc0a8e28544c3e70b4e6c6575833c0`
- PR **#26** -> `e5a260577e6aa2d6666e13c00845940ba82e4c76`
- PR **#27** -> `897a03b76553a9bc5b674cc703d5efaf61047434`
- PR **#28** -> `9094d790ae99d6de3936ce044f9d930d8f614693`
- PR **#29** -> `3ff5a1164ff2e1b770552c2b4ab08d85cbef66ef`
- PR **#30** -> `0bc95bb907e79e6fc9af81c23856b854fc99ec76`
- PR **#31** -> `a8b674894d418bd6fa478a2d30bfa362076c8a4a`
- PR **#32** -> `a144802aee4211927aaa43c7c89baf118fb251c9`

Issue **#10** is closed. Issue **#14** remains the Public Alpha release tracker.

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
- [x] minimal SDL3 GPU 2D renderer and capture path
- [x] ordered multi-sprite submission and culling metrics
- [x] batching opportunity measurement
- [x] offscreen presentation/capture source
- [x] explicit simulation-frame capture artifact

### Public Alpha — in progress

- [x] tiny end-to-end sample proving the full automation loop
- [x] documented edit -> build -> run -> inspect -> input -> assert -> capture workflow
- [ ] optional measured contiguous same-texture instancing decision/implementation
- [ ] repository license selected and added
- [ ] third-party license review completed
- [ ] repository/history secret and private-path review completed
- [ ] README quick start verified from a clean clone
- [ ] implemented/planned features clearly distinguished
- [ ] Public Alpha limitations documented
- [ ] release-candidate `main` CI green
- [ ] create `v0.1.0-alpha.1`
- [ ] change repository visibility to Public
- [ ] verify README/release from public view

## Next execution order

A fresh agent should work in this order unless a blocking dependency requires a documented change:

1. Implement or explicitly defer contiguous same-texture instancing using the PR #32 measurement as evidence.
2. Re-run the committed Public Alpha sample and record actual before/after draw-call behavior without changing painter order.
3. Perform repository-quality gates: license, third-party licenses, secrets/private paths, clean-clone quick start, implemented-vs-planned wording, and limitations.
4. Run release-candidate CI and mark this file release-ready.
5. Create `v0.1.0-alpha.1`, change visibility to Public, verify the public view, then open follow-up issues for known limitations.

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
- [x] renderer culling baseline
- [x] batching opportunity measurable without changing painter order
- [x] offscreen render target suitable for visual readback
- [x] deterministic capture at a known simulation frame
- [x] one tiny end-to-end sample proving the workflow
- [ ] clean Windows build/test documentation verified for the release candidate
- [ ] green release-candidate `main` CI
- [ ] repository license and third-party license review before visibility changes to Public
- [ ] documentation clearly distinguishes implemented features from planned features

See `docs/PUBLIC_RELEASE.md` for exact release gates.

## Explicit non-blockers for first public release

Do **not** delay Public Alpha for these unless release scope is intentionally changed:

- contiguous same-texture instancing if implementation cost is disproportionate to the measured benefit
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

- whether contiguous same-texture instancing is worth landing before Public Alpha after measuring implementation complexity against the observed 5-draw saving
- whether construction-time shadercross should later be replaced by offline precompiled shader artifacts; change only if measured startup/distribution/CI cost justifies it
- project license before repository visibility changes to Public
- exact protocol/transport used before the later MCP adapter

The Public Alpha sample content, capture source, canonical pixel layout, first artifact format, and synchronous first capture API are no longer open decisions.

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
