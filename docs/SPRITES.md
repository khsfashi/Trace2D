# Sprite Pipeline Contract

Status: **S0/S1/SR0/SR1/SR2/SR3/SR4/SR5/SR6 complete. SR7 — production batching/resource reuse/hot-path metrics is active via #138 / draft PR #139. SR8 must not begin before SR7 merges green.**

Operational umbrella: GitHub Issue #59.  
Frozen S0 architecture: [`SPRITE_ARCHITECTURE.md`](SPRITE_ARCHITECTURE.md).  
Machine-readable S0 invariants: [`contracts/sprite-s0.json`](contracts/sprite-s0.json).  
Canonical S1 format: [`SPRITE_ASSET_FORMAT.md`](SPRITE_ASSET_FORMAT.md).  
SR0 render seam: [`SPRITE_RENDER_CONTRACT.md`](SPRITE_RENDER_CONTRACT.md).  
SR1 transform seam: [`SPRITE_TRANSFORM_PRESENTATION.md`](SPRITE_TRANSFORM_PRESENTATION.md).  
SR2 atlas/trim/UV seam: [`SPRITE_ATLAS_GEOMETRY.md`](SPRITE_ATLAS_GEOMETRY.md).  
SR3 color/alpha/blend/sampling seam: [`SPRITE_COLOR_SAMPLING.md`](SPRITE_COLOR_SAMPLING.md).  
SR4 painter-order/group/masking seam: [`SPRITE_ORDER_MASKING.md`](SPRITE_ORDER_MASKING.md).  
SR5 9-slice/tiled seam: [`SPRITE_PRIMITIVES.md`](SPRITE_PRIMITIVES.md).  
SR6 pixel-perfect seam: [`SPRITE_PIXEL_PERFECT.md`](SPRITE_PIXEL_PERFECT.md).  
SR7 production batching/hot-path seam: [`SPRITE_BATCHING_SR7.md`](SPRITE_BATCHING_SR7.md).

This document owns the fixed Sprite stage order and capability target. Stage-local documents refine implementation details but cannot silently replace canonical authored/runtime truth with renderer or tooling state.

## 1. Product goal

Trace2D targets an Agent-verifiable Sprite pipeline rather than a minimal quad renderer:

```text
sprite request / source image / external sheet
 -> optional generation
 -> deterministic import / normalization / QA
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
 -> deterministic import
 -> canonical SpriteAsset CPU truth
 -> authoritative SpriteRenderer2D / SpriteAnimator2D / transform semantics
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
- import/generation/repair/report/capture are explicit work outside ordinary frame hot paths.

## 3. Fixed implementation order

Exactly one child issue/PR is active at a time:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete]
 -> SR4 [complete] -> SR5 [complete] -> SR6 [complete]
 -> SR7 [active #138/#139] -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Completed renderer stages through SR6 are frozen. **Do not create or begin SR8 while #138 / #139 remains open or while the required owner Windows presentation-GPU gate is pending.**

## 4. Completed renderer foundation

### S0 — architecture / authority — complete

Frozen by #119/#120 and `SPRITE_ARCHITECTURE.md`: authority/ownership, exact coordinates, pivot/trim/rotation storage semantics, fixed-step presentation history, exact-frame capture semantics, typed resource/view/material seams, painter-order invariant, #97-#99 verification authority and hot-path/offline-tooling separation.

### S1 — canonical Sprite asset/import — complete

Merged via #121/#122 (`27250bff8afd40f55edf2bfbed9be8b143f1ea1d`). Canonical `.sprite.toml` v1 owns ordered pages/regions, exact source/trim/packed rectangles, reduced rational pivot, `none|cw90` packed rotation, color/alpha/sampling intent and the optional source-pixel `border` extension consumed by SR5. No normalized UV or GPU handle is canonical asset state.

### SR0 — asset/render separation — complete

Merged via #123/#124 (`aa30a8e4498fd5edd6df9d2be7bb9a91bcdea5db`). Setup-time region resolution produces fixed-size renderer-facing contract data; steady-state extraction is O(1) and allocation-free.

### SR1 — transform/history/geometry — complete

Merged via #125/#126 (`7b78c7bd5f792cfcf5a9171c62e06e792b1702ac`). Reuses `scene::Transform2D`, preserves exact rational pivot and untrimmed source geometry, applies semantic flip/scale/rotation/translation once, and separates authoritative fixed state from presentation history.

### SR2 — atlas/trim/pivot/rotated packing — complete

Merged via #127/#128 (`a42e65c8a7953d38ad2d82894332c1f39da288f1`). Trim remains embedded in untrimmed logical source space; packed storage never controls logical placement; UV geometry is exact pixel-edge truth and `cw90` changes storage mapping only.

### SR3 — color/alpha/blend/sampling — complete

Completed via #130/#131. Canonical source alpha remains straight. Sampling is nearest/linear with atlas-safe texel-center guards, sRGB/linear page intent is explicit, and built-in fragment output is premultiplied immediately before blending. Persistent samplers/pipelines and capacity-managed Sprite vertex resources are reused. The real Windows presentation-GPU gate passed on 2026-08-12.

### SR4 — painter order/sorting groups/masking — complete

Completed via #132/#133, squash `3c843bba10be536685c0fc70306b3fd6c75ed67a`.

SR4 freezes signed layer/order, explicit stable semantic order, bounded one-level sorting groups, no resource identity in semantic ordering, bounded stencil mask identities/phases, persistent stencil-compatible pipelines/mask target, and no ordinary-frame explicit GPU readback/fence wait. The committed owner Windows GPU fixture passed on 2026-08-12.

### SR5 — 9-slice and tiled/repeated primitives — complete

Completed via #134/#135, squash `6d84ada945774d54c089d1a5bec3b63e17a43334`. Concrete contract: [`SPRITE_PRIMITIVES.md`](SPRITE_PRIMITIVES.md).

SR5 adds exact source-pixel borders, `quad|sliced|tiled` runtime intent, deterministic 9-slice partition/compression, bounded tiled expansion, partial-final-tile geometry, trim-gap and `cw90` subrect mapping, per-patch atlas-safe sample bounds, caller-owned fixed-bounded patch output, `MaximumSpritePrimitiveQuads == 4096`, one top-level SR4 semantic item for all patches, and persistent GPU resource reuse.

The owner Windows presentation-GPU fixture `SpritePrimitiveGpuSmokeTests.Sr5TiledLinearMaskAndCapacityReuseMatchContract` passed after a real partial-tile linear-filter bleed regression was fixed and locked by CPU regression coverage.

### SR6 — pixel-perfect runtime presentation — complete

Completed via #136/#137. PR #137 merged into `main` as `3fd6e5a439a1e327bd89797f5d5a5a9dae69dace`. Concrete contract: [`SPRITE_PIXEL_PERFECT.md`](SPRITE_PIXEL_PERFECT.md).

SR6 freezes:

- explicit logical reference viewport -> final target integer-contain mapping,
- deterministic centered letterbox/pillarbox rectangle,
- logical-aspect `OrthographicView` shared by CPU mapping and production GPU vertex conversion,
- transformed untrimmed source pixel-edge origin `(0,0)` as exact grid authority,
- exact-grid eligibility for finite axis-aligned integer source-pixel bases including flips and quarter-turn swaps,
- structured rejection of fractional scaling/arbitrary-angle rotation,
- presentation-only nearest-edge snapping with no mutation of authoritative SR1 history,
- explicit authoritative-current vs SR1-interpolated presentation evidence,
- nearest sampling for exact pixel-perfect production presentation,
- full-target clear followed by integer SDL GPU viewport/scissor restriction,
- preservation of SR4 painter/mask semantics and SR5 primitive atomicity/resource reuse,
- no intermediate upscale texture and no ordinary-frame explicit GPU readback/fence wait.

The owner-local fixture `SpritePixelPerfectGpuSmokeTests.Sr6IntegerViewportNearestMaskPrimitiveAndReuseMatchContract` passed on 2026-08-12 before #137 merged.

## 5. Active and remaining production Sprite renderer stages

### SR7 — production batching/resource reuse/hot-path metrics — active

Active vehicle: **#138 / draft PR #139**. Concrete contract: [`SPRITE_BATCHING_SR7.md`](SPRITE_BATCHING_SR7.md).

SR7 preserves the complete SR4 painter sequence and introduces only optimization beneath that semantic order.

Frozen batch compatibility state:

```text
texture/resource identity
+ resolved material/pipeline identity
+ sampler compatibility
+ blend compatibility
+ exact mask mode/id
```

Hard rules:

- resource/material state never participates in sorting,
- only compatible contiguous **visible** work may merge,
- fully culled/zero-output top-level work does not split a run,
- SR5 patches remain atomic under one top-level Sprite,
- SR6 logical view is used for culling/vertex conversion when enabled,
- tint/opacity are derived per-vertex data and do not split otherwise-compatible built-in runs,
- SR7 executes only the built-in material/pipeline identity; #89 owns programmable material behavior,
- only visible vertices are compacted/uploaded,
- one triangle-list GPU draw is emitted per non-empty compatibility run,
- persistent sampler/pipeline/vertex-transfer/mask resources are reused,
- retained vertex capacity grows geometrically on visible-quad high-water marks,
- ordinary frame upload reuse adds no explicit readback/fence wait,
- SR7 scan/compaction/run work is O(N + Q) after existing SR4 semantic order resolution.

Public metrics expose semantic Sprite count, visible/culled Sprite count, actual batch/draw count, uploaded visible quads/bytes, compatibility runs, retained vertex slot/byte capacity, persistent resource creation counts, and explicit GPU readback/fence waits.

Backend-independent tests are committed. The blocking owner Windows presentation-GPU fixture is:

```text
SpriteBatchGpuSmokeTests.Sr7BatchesCullsPreservesAppearanceMaskPrimitivePixelPerfectAndReuse
```

It must pass with `TRACE2D_RUN_GPU_SMOKE=1` before #139 may leave draft/completion state. Hosted CPU CI may skip this explicit presentation-GPU fixture; owner-local evidence remains mandatory.

### SR8 — renderer conformance/workloads

After SR7 merges, commit CPU/GPU fixtures for transform, trim/pivot, rotated atlas storage, color/blend/sampling, ordering/groups/masks, 9-slice/tiled, pixel-perfect presentation and batch derivation, plus representative workload evidence. Do not create SR8 early.

## 6. Deterministic Sprite animation

### SA0 — timing/frame/event contract
Freeze exact animation time representation and deterministic frame/event boundary rules.

### SA1 — `SpriteAnimator2D` authoritative state
Implement renderer-independent typed clip/time/frame/playing/loop/completion/speed state.

### SA2 — playback/events/transitions
Implement deterministic play/restart/pause/resume/stop/reset, loops, completion, speed, events and bounded transitions.

### SA3 — Agent/MCP verification
Expose protocol-independent inspect/action/assert semantics; MCP remains an adapter.

### SA4 — conformance/workloads
Prove fixed-step frame/event sequences and measure animation update independently from rendering.

## 7. Offline Sprite processing / generation

### SPP0 — processing/QA report
Machine-readable raw measurements for dimensions, frame count, alpha/edge residue, trim, pivot/jitter, grid/palette, identity/motion warnings and atlas utilization.

### SPP1 — alpha/background/frame extraction
Deterministic explicit cleanup/segmentation modes; expected-frame mismatch fails instead of inventing frames.

### SPP2 — pixel-grid/palette/pivot/identity/motion QA and repair
Offline deterministic or explicitly labelled heuristic analysis/repair with reviewable raw evidence.

### SPP3 — Aseprite/generic importers
Convert supported external formats into canonical Trace2D Sprite assets; no source-tool runtime dispatch.

### SPP4 — sprite-gen / PerfectPixel-style interoperability
Consume useful external manifests through conversion/validation without runtime dependencies.

### SPP5 — provider-neutral generation orchestration

```text
sprite request
 -> replaceable external generation provider
 -> raw output
 -> deterministic Trace2D processing + QA
 -> canonical asset after validation
```

Live provider calls are not deterministic CI dependencies.

## 8. End-to-end proof

### SE2E
Prove request/import -> raw/generated pixels -> deterministic QA -> canonical asset -> animation -> headless exact-frame verification -> renderer/capture -> perceptual/human review.

### SPERF
Publish visible/animated counts, atlas pages, compatibility transitions, draws, culling, animation/extraction CPU time, upload bytes, retained capacities, texture/page memory/utilization and capture cost.

## 9. Verification/review authority

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

A screenshot cannot override deterministic failure. Sprite does not create a second review database.

## 10. Explicit handoffs / non-goals

#59 does not silently absorb generic Material2D/Shader2D (#89), Camera2D/Viewport2D ownership (#88), general resource lifecycle (#86), scene/component hierarchy (#71), arbitrary Mesh2D (#60), Spine/skeletal runtime before #61/#101 license/product decision, PBR/deferred/render-graph/bindless architecture, or an ECS/reflection/custom-allocator/job-system detour without evidence.

## 11. Handoff rule

Every Sprite child PR must:

1. implement only the first incomplete stage,
2. include tests/fixtures/evidence,
3. update relevant stage contracts, this roadmap and `PROJECT_STATUS.md`,
4. preserve enough structured evidence to continue without chat history,
5. avoid beginning the next child until the current PR merges green.

SR7 is the only active Sprite child. **Do not merge #139 until the final hosted checks are green and the owner-local `SpriteBatchGpuSmokeTests.Sr7BatchesCullsPreservesAppearanceMaskPrimitivePixelPerfectAndReuse` run passes with `TRACE2D_RUN_GPU_SMOKE=1`.** After both gates pass, record the evidence, mark ready/merge #139, confirm #138 closed, and stop that continuation. Only the following `@GitHub Trace2D 다음 진행해줘` continuation may create exactly one SR8 child. After the complete #59 program, the exact next core item remains **#103 Benchmark B1** before #69 game-production work.