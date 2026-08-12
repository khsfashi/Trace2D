# Sprite Pipeline Contract

Status: **S0/S1/SR0-SR8/SA0-SA4/SPP0 complete. SPP1 — deterministic alpha/background/frame extraction is active via #156 / draft PR #157.**

Operational umbrella: GitHub Issue #59.  
Frozen S0 architecture: [`SPRITE_ARCHITECTURE.md`](SPRITE_ARCHITECTURE.md).  
Machine-readable S0 invariants: [`contracts/sprite-s0.json`](contracts/sprite-s0.json).  
Canonical S1 format: [`SPRITE_ASSET_FORMAT.md`](SPRITE_ASSET_FORMAT.md).  
SR0 render seam: [`SPRITE_RENDER_CONTRACT.md`](SPRITE_RENDER_CONTRACT.md).  
SR1 transform seam: [`SPRITE_TRANSFORM_PRESENTATION.md`](SPRITE_TRANSFORM_PRESENTATION.md).  
SR2 atlas/trim/UV seam: [`SPRITE_ATLAS_GEOMETRY.md`](SPRITE_ATLAS_GEOMETRY.md).  
SR3 color/sampling seam: [`SPRITE_COLOR_SAMPLING.md`](SPRITE_COLOR_SAMPLING.md).  
SR4 order/masking seam: [`SPRITE_ORDER_MASKING.md`](SPRITE_ORDER_MASKING.md).  
SR5 9-slice/tiled seam: [`SPRITE_PRIMITIVES.md`](SPRITE_PRIMITIVES.md).  
SR6 pixel-perfect seam: [`SPRITE_PIXEL_PERFECT.md`](SPRITE_PIXEL_PERFECT.md).  
SR7 batching/hot-path seam: [`SPRITE_BATCHING_SR7.md`](SPRITE_BATCHING_SR7.md).  
SR8 conformance/workload seam: [`SPRITE_RENDERER_CONFORMANCE_SR8.md`](SPRITE_RENDERER_CONFORMANCE_SR8.md).  
SA0 timing seam: [`SPRITE_ANIMATION_TIMING_SA0.md`](SPRITE_ANIMATION_TIMING_SA0.md).  
Machine-readable SA0 invariants: [`contracts/sprite-animation-sa0.json`](contracts/sprite-animation-sa0.json).  
SA1 state seam: [`SPRITE_ANIMATOR_STATE_SA1.md`](SPRITE_ANIMATOR_STATE_SA1.md).  
SA2 playback seam: [`SPRITE_ANIMATOR_PLAYBACK_SA2.md`](SPRITE_ANIMATOR_PLAYBACK_SA2.md).  
SA3 Agent/MCP seam: [`SPRITE_ANIMATION_AGENT_SA3.md`](SPRITE_ANIMATION_AGENT_SA3.md).  
SA4 conformance/workload seam: [`SPRITE_ANIMATION_CONFORMANCE_SA4.md`](SPRITE_ANIMATION_CONFORMANCE_SA4.md).  
SPP0 processing/QA seam: [`SPRITE_PROCESSING_QA_SPP0.md`](SPRITE_PROCESSING_QA_SPP0.md).  
Active SPP1 extraction seam: [`SPRITE_EXTRACTION_SPP1.md`](SPRITE_EXTRACTION_SPP1.md).

This document owns the fixed Sprite stage order and capability target. Stage-local documents refine implementation details but cannot silently replace canonical authored/runtime truth with renderer, Agent, workload, processing-report, extraction-result, timing, or capture state.

## 1. Product goal

Trace2D targets an Agent-verifiable Sprite pipeline rather than a minimal quad renderer:

```text
sprite request / source image / external sheet
 -> optional generation
 -> deterministic offline processing / normalization / QA
 -> canonical SpriteAsset
 -> authoritative Sprite runtime/animation state
 -> backend-independent render extraction
 -> production renderer
 -> exact-frame + perceptual QA
 -> reproducible performance evidence
```

Generated pixels and external formats are inputs. Canonical Trace2D data is runtime truth.

## 2. Frozen authority and coordinates

```text
external source/generation
 -> deterministic processing/import evidence
 -> canonical SpriteAsset CPU truth
 -> authoritative SpriteRenderer2D / SpriteAnimator2D / transform semantics
 -> explicit Agent inspection / deterministic workload evidence
 -> backend-independent extraction
 -> derived presentation
 -> renderer/backend resources
```

Hard invariants:

- canonical assets and authoritative runtime state require no renderer/GPU initialization,
- authored image metadata is exact source-pixel truth; normalized UVs/GPU handles/batch IDs are derived,
- source origin is untrimmed top-left, +x right, +y down, integer half-open rectangles,
- pivot is exact untrimmed source-space metadata and may intentionally lie outside source bounds,
- trim and packed rotation are storage semantics only,
- rendering never owns transform/gameplay/animation truth,
- `current_fixed` is authoritative; `previous_fixed` is presentation history,
- exact-frame presentation uses authoritative current state,
- semantic painter order cannot be changed by resource/material sorting,
- #71/#86/#88/#89 attach through typed seams without replacing Sprite semantics,
- import/generation/repair/report/capture/workload/timing are explicit work outside ordinary frame hot paths.

Animation authority is the completed SA0-SA4 chain: integer fixed-step animation time/frame/event crossings are authoritative runtime state; SA1 materializes renderer-independent prepared clip/state; SA2 executes exact retained-rational playback and typed emissions; SA3 exposes that existing authority to agents without duplicating it; SA4 validates/replays/measures it explicitly. MCP serialization, workload digests, timing samples, GPU resources and pixels never become a second animation state machine.

Offline processing authority begins with SPP0. Decoded pixels and explicit metadata are inputs; deterministic measurements and findings are derived evidence. SPP0 reports do not mutate source pixels or become canonical Sprite state. SPP1 may create derived cleaned/extracted pixels only from explicit deterministic rules, preserves exact source rectangles, requires expected frame count, and feeds those outputs back through SPP0 rather than creating a second QA vocabulary.

## 3. Fixed implementation order

Exactly one child issue/PR is active at a time:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete]
 -> SR4 [complete] -> SR5 [complete] -> SR6 [complete] -> SR7 [complete]
 -> SR8 [complete]
 -> SA0 [complete] -> SA1 [complete] -> SA2 [complete] -> SA3 [complete]
 -> SA4 [complete]
 -> SPP0 [complete]
 -> SPP1 [active #156/#157] -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Completed stages through SPP0 are frozen. **Do not create or begin SPP2 while #156/PR #157 remains open or while its hosted CI/audit/documentation gates are pending.**

## 4. Completed renderer foundation

### S0 — architecture / authority — complete

#119/#120 froze authority/ownership, exact coordinates, pivot/trim/rotation semantics, fixed-step presentation history, exact-frame capture, typed future resource/view/material seams, painter-order invariants and hot-path/offline-tool separation.

### S1 — canonical Sprite asset/import — complete

#121/#122 established canonical `.sprite.toml` v1 with ordered pages/regions, exact source/trim/packed rectangles, reduced rational pivot, `none|cw90` packing, color/alpha/sampling intent and source-pixel border metadata. Normalized UVs and GPU handles are never canonical asset state.

### SR0 — asset/render separation — complete

#123/#124 made setup-time region resolution produce fixed-size renderer-facing data; steady-state extraction is O(1) and allocation-free.

### SR1 — transform/history/geometry — complete

#125/#126 reused `scene::Transform2D`, preserved exact rational pivot/untrimmed source geometry, and separated authoritative fixed state from presentation history.

### SR2 — atlas/trim/pivot/rotated packing — complete

#127/#128 kept trim in untrimmed logical source space and made `cw90` a storage/UV mapping concern only.

### SR3 — color/alpha/blend/sampling — complete

#130/#131 froze straight canonical source alpha, nearest/linear atlas-safe sampling, explicit sRGB/linear page intent, premultiplied output immediately before blending, and persistent sampler/pipeline/vertex-resource reuse. Required owner GPU evidence passed.

### SR4 — painter order/sorting groups/masking — complete

#132/#133 froze stable semantic painter order independent of resource identity, bounded one-level sorting groups, bounded stencil masks and persistent stencil-compatible resources.

### SR5 — 9-slice and tiled/repeated primitives — complete

#134/#135 added exact source-pixel borders, deterministic sliced/tiled expansion, bounded caller-owned patch output, trim-gap/`cw90` subrect mapping and atlas-safe partial-tile sampling while preserving SR4 semantic atomicity.

### SR6 — pixel-perfect runtime presentation — complete

#136/#137 froze integer logical viewport mapping, letterbox/pillarbox, exact source-grid snap, current-vs-interpolated presentation selection and nearest exact pixel presentation without an intermediate upscale texture.

### SR7 — production batching/resource reuse/hot-path metrics — complete

#138/#139 preserved painter order, culled against the resolved presentation view, compacted visible geometry only, merged only contiguous compatible work and exposed structural metrics while reusing retained capacities/resources.

### SR8 — renderer conformance / capture QA / reproducible workloads — complete

#142/#143 added backend-independent composition coverage, the fixed 1,024-submission structural workload, owner real-GPU fixtures/evidence, deterministic CPU facts vs bounded GPU pixel tolerances and exact-head final evidence. Captured pixels remain derived evidence and shared-runner wall-clock time is not a correctness threshold.

## 5. Deterministic Sprite animation — complete

### SA0 — timing/frame/event contract

#144/#145 froze integer nanoseconds, checked positive frame durations, half-open frame ownership, explicit terminal completion, stable equal-time event ordinals, exact forward/reverse crossing rules, lossless large advances, loop/ping-pong traversal and the no-wall-clock/no-per-tick-reporting contract.

### SA1 — `SpriteAnimator2D` authoritative state

#146/#147 added prepared renderer-independent clip data and fixed-size typed state, cached integer boundaries, O(log frame-count) arbitrary lookup, O(1) current observation, canonical exact rational speed and transactional restore.

### SA2 — playback/events/loops/transitions

#148/#149 added prepared numeric authored events, retained rational speed remainder, explicit controls, deterministic `Once|Loop|PingPong` traversal and caller-owned bounded `AuthoredEvent|Loop|Bounce|Completed` output. Capacity failure is explicit and transactional.

### SA3 — Agent/MCP verification

#150/#151 exposed existing SA0-SA2 authority through explicit non-owning entity bindings, exact scalar inspection, direct runtime actions and finite typed assertions. Agent advance materializes ordered emissions only on request; MCP remains serialization only.

### SA4 — conformance, determinism and workloads

#152/#153, squash `c5952c0e905c46816b0a182b7d91143bf54b188b`, completed the runtime phase with three committed structural workloads and focused long-run/boundary/rational/ping-pong/capacity/restart tests. Structural evidence is deterministic; optional Release timing is environment-labelled local evidence only. SA4 introduced no renderer/GPU behavior and no runtime reporting/hash/timing work.

## 6. Offline Sprite processing / generation

### SPP0 — deterministic processing / QA report — complete

Completed via #154 / PR #155 / squash `54d13db3c0547311afdbab25854212edc8226116`. Concrete contract: [`SPRITE_PROCESSING_QA_SPP0.md`](SPRITE_PROCESSING_QA_SPP0.md).

SPP0 defines the first protocol-independent offline evidence surface over immutable decoded RGBA8 frame views and explicit frame/page metadata.

Required raw evidence includes:

- dimensions / exact pixel count,
- transparent/partial/opaque counts,
- non-zero-alpha visible bounds,
- empty state and per-edge visible contact counts,
- transparent-RGB residue,
- exact color counts,
- explicit pivot/dimension histograms,
- exact byte-identical duplicate groups,
- adjacent equal-dimension changed-pixel count and visible-bounds origin displacement,
- explicit grid evidence without automatic segmentation,
- explicit atlas area/utilization/out-of-bounds/overlap facts.

Findings are stable typed records separate from raw facts. Threshold-based findings require explicit options; artistic style/readability/identity/motion quality is not silently presented as deterministic truth.

Determinism rules:

- same bytes/metadata/order/options -> field-identical and byte-identical schema-versioned JSON,
- no pointer or unordered iteration appears in observable ordering,
- exact integer ratios remain authority over convenience floats,
- duplicate identity requires full dimension + RGBA8 equality rather than a probabilistic hash match.

Performance boundary:

- per-frame scan is linear in pixel count,
- adjacent diff is linear per comparable pair,
- initial duplicate grouping and atlas overlap may use bounded explicit offline pairwise comparison,
- source pixel buffers are viewed instead of copied merely for reporting,
- no SPP0 work enters animation/runtime/render frame paths.

`trace2d_sprite_process` is an explicit CLI adapter using `TextureAssetCache`; the core analyzer remains filesystem/GPU independent.

Current reference decisions: W3C PNG alpha semantics are adopted/adapted over decoded RGBA8; Godot `Image.get_used_rect()` is adapted as a useful non-zero-alpha-bounds precedent; Aseprite frame/palette/grid metadata is adopted/adapted as explicit authoring precedent; Aseprite importing remains SPP3 and silent grid/frame inference is rejected.

No new presentation/GPU behavior was introduced, so SPP0 required no new real-GPU acceptance gate.

### SPP1 — deterministic alpha/background/frame extraction — active #156 / draft PR #157

Concrete contract: [`SPRITE_EXTRACTION_SPP1.md`](SPRITE_EXTRACTION_SPP1.md).

SPP1 consumes decoded RGBA8 sheet pixels plus an explicit extraction specification and produces owned derived frame pixels with exact source rectangles. It supports only deterministic caller-selected cleanup: exact RGB color key, explicit alpha cutoff, optional transparent-RGB zeroing, and optional non-zero-alpha trim.

Extraction geometry is one of:

- caller-ordered explicit rectangles,
- fully explicit uniform grid with origin/cell/rows/columns/spacing/order,
- deterministic 4-connected post-cleanup alpha components with row-major component seeds.

Every mode requires `expectedFrameCount > 0`. Count mismatch fails without partial output; SPP1 does not silently merge, split, drop or invent frames. Alpha components are geometry evidence only, not a semantic claim that every connected region is an authored frame.

Successful outputs are passed to the existing SPP0 `AnalyzeSpriteProcessing` API, so SPP0 remains the alpha/bounds/color/identity/motion/finding vocabulary. SPP1 adds no parallel QA truth model.

Fuzzy color matching, learned/VLM background removal and perceptual segmentation are rejected from SPP1. They may only appear in a later explicitly heuristic/perceptual stage with reviewable evidence.

SPP1 is explicit offline work: component discovery is linear in source pixels, output copy is proportional to extracted pixels, visitation storage is bounded to the source operation, and no runtime/animation/render/GPU path changes.

### SPP2 — pixel-grid/palette/pivot/identity/motion QA and repair

Add offline deterministic or explicitly labelled heuristic analysis/repair with reviewable raw evidence. Deterministic facts stay distinct from perceptual/advisory judgments.

### SPP3 — Aseprite/generic importers

Convert supported external formats into canonical Trace2D Sprite assets; never dispatch source-tool runtime code from the engine.

### SPP4 — sprite-gen / PerfectPixel-style interoperability

Consume useful external manifests through conversion/validation without turning those tools into runtime dependencies.

### SPP5 — provider-neutral generation orchestration

```text
sprite request
 -> replaceable external generation provider
 -> raw output
 -> deterministic Trace2D processing + QA
 -> canonical asset after validation
```

Live provider calls are not deterministic CI dependencies.

## 7. End-to-end proof

### SE2E

Prove request/import -> raw/generated pixels -> deterministic QA -> canonical asset -> animation -> headless exact-frame verification -> renderer/capture -> perceptual/human review.

### SPERF

Publish visible/animated counts, atlas pages, compatibility transitions, draws, culling, animation/extraction CPU time, upload bytes, retained capacities, texture/page memory/utilization and capture cost with reproducible environment metadata.

## 8. Frozen production integration requirements

### Same future world/component semantics

`SpriteRenderer2D` and `SpriteAnimator2D` are finite typed semantic components compatible with #71. Sprite must not own a private competing entity graph.

### Fixed-state vs presentation

Moving Sprite presentation remains:

```text
previous_fixed
current_fixed
presentation(previous, current, alpha)
```

Gameplay/Agent reads `current_fixed`; interactive rendering may interpolate; reset/load/teleport synchronizes history; exact-frame capture defaults to authoritative current state; no transient per-Sprite heap selector work is required.

### Material-ready seam

The built-in Sprite material already resolves through a compatibility identity so #89 may extend the same path. Only compatible contiguous work may merge; material identity never authorizes global painter-order sorting.

### Camera/Viewport-ready seam

Rendering consumes backend-independent resolved presentation/view state. #88 later owns Camera2D/Viewport2D selection and world/screen mapping without replacing Sprite semantics.

### Resource-ready seam

Canonical Sprite assets retain project-relative CPU identity. UVs/GPU handles/samplers/pipelines/buffers remain derived presentation state so #86 can own common resource lifecycle later.

## 9. Verification / review authority

Reuse #97-#99:

```text
deterministic Sprite fact
 -> verify
 -> diagnose
 -> Agent/user repair
 -> re-verify
 -> multimodal review only when genuinely perceptual
 -> human creative approval
```

A screenshot cannot override deterministic failure. A processing warning cannot silently mutate canonical state. Sprite does not create a second review database.

## 10. Production texture decisions / handoffs

Sprite owns or hands off explicitly:

- canonical color-space intent and renderer conversion expectations,
- alpha convention/conversion boundary,
- atlas page metadata compatible with future compression/mip/package policy,
- exact authored pixel metadata remaining canonical even when packaged GPU representations differ,
- memory/workload evidence distinguishing Sprite metadata, retained CPU pixels and renderer-owned GPU pages.

#70 owns package format policy and #86 owns common runtime resource residency/lifetime. Sprite must not permanently assume all production pages are uncompressed single-mip RGBA8 GPU resources.

## 11. Explicit non-goals

#59 does not silently absorb generic Material2D/Shader2D (#89), Camera2D/Viewport2D ownership (#88), general resource lifecycle (#86), scene/component hierarchy (#71), arbitrary Mesh2D (#60), Spine/skeletal runtime before #61/#101 decision, PBR/deferred/render-graph/bindless architecture, or an ECS/reflection/custom-allocator/job-system detour without evidence.

Offline processing/generation must not pollute runtime hot paths. External tools/providers remain replaceable inputs/adapters rather than engine gameplay authority.

## 12. Completion definition

The complete #59 program is done only when an agent can author/import/generate a Sprite animation, receive deterministic machine-readable processing/QA, produce a canonical SpriteAsset, run it headlessly, inspect/assert exact animation state/events, render through the production Sprite path, capture authoritative exact-frame visual evidence, obtain reproducible performance evidence, emit #98-compatible review evidence and hand genuine perceptual questions to multimodal/human review without confusing those layers.

On completion of all SPP stages, SE2E and SPERF, advance to **#103 Benchmark B1** before #69 game-production work.

## 13. Handoff rule

Every Sprite child PR must:

1. implement only the first incomplete stage,
2. include tests/fixtures/evidence,
3. update relevant stage contracts, this roadmap and `PROJECT_STATUS.md`,
4. preserve enough structured evidence to continue without chat history,
5. avoid beginning the next child until the current PR merges green.

SPP1 / #156 / draft PR #157 is the only active Sprite child. Keep it scoped to explicit deterministic cleanup/extraction, expected-frame gates, exact source geometry, SPP0 QA reuse and deterministic structural evidence. Do not create or implement SPP2 until PR #157 merges green.
