# Sprite Pipeline Contract

Status: **S0/S1/SR0/SR1/SR2/SR3/SR4/SR5 complete. SR6 — pixel-perfect runtime presentation is active via #136 / draft PR #137. SR7 must not begin before SR6 merges green.**

Operational umbrella: GitHub Issue #59.  
Frozen S0 architecture: [`SPRITE_ARCHITECTURE.md`](SPRITE_ARCHITECTURE.md).  
Machine-readable S0 invariants: [`contracts/sprite-s0.json`](contracts/sprite-s0.json).  
Canonical S1 format: [`SPRITE_ASSET_FORMAT.md`](SPRITE_ASSET_FORMAT.md).  
SR0 render seam: [`SPRITE_RENDER_CONTRACT.md`](SPRITE_RENDER_CONTRACT.md).  
SR1 transform/presentation seam: [`SPRITE_TRANSFORM_PRESENTATION.md`](SPRITE_TRANSFORM_PRESENTATION.md).  
SR2 atlas/trim/UV seam: [`SPRITE_ATLAS_GEOMETRY.md`](SPRITE_ATLAS_GEOMETRY.md).  
SR3 color/alpha/blend/sampling seam: [`SPRITE_COLOR_SAMPLING.md`](SPRITE_COLOR_SAMPLING.md).  
SR4 painter-order/group/masking seam: [`SPRITE_ORDER_MASKING.md`](SPRITE_ORDER_MASKING.md).  
SR5 9-slice/tiled seam: [`SPRITE_PRIMITIVES.md`](SPRITE_PRIMITIVES.md).  
SR6 pixel-perfect presentation seam: [`SPRITE_PIXEL_PERFECT.md`](SPRITE_PIXEL_PERFECT.md).

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
 -> SR4 [complete #132/#133] -> SR5 [complete #134/#135]
 -> SR6 [active #136/#137] -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Completed renderer stages through SR5 are frozen. **Do not create or begin SR7 while #136 / #137 remains open or while the required owner Windows presentation-GPU gate is pending.**

## 4. Completed foundation

### S0 — architecture / authority — complete

Frozen by #119/#120 and `SPRITE_ARCHITECTURE.md`: authority/ownership, exact coordinates, pivot/trim/rotation storage semantics, fixed-step presentation history, exact-frame capture semantics, typed resource/view/material seams, painter-order invariant, #97-#99 verification authority and hot-path/offline-tooling separation.

### S1 — canonical Sprite asset/import — complete

Merged via #121/#122 (`27250bff8afd40f55edf2bfbed9be8b143f1ea1d`). Canonical `.sprite.toml` v1 owns ordered pages/regions, exact source/trim/packed rectangles, reduced rational pivot, `none|cw90` packed rotation, color/alpha/sampling intent and the backward-compatible optional source-pixel `border` extension consumed by SR5. No normalized UV or GPU handle is canonical asset state.

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

SR4 freezes:

- signed `layer`/`order`, explicit stable semantic order and original input ordinal as exact-tie authority,
- bounded one-level sorting groups that remain atomic in top-level painter order,
- no texture/material/sampler/GPU identity in semantic ordering,
- bounded mask states `none`, `write(id)`, `test_inside(id)`, `test_outside(id)` for IDs 1..255,
- deterministic mask-phase validation,
- persistent stencil-compatible pipelines and reusable mask target,
- no ordinary-frame explicit GPU readback/fence wait.

The committed real-GPU fixture `SpriteOrderMaskGpuSmokeTests.Sr4PainterGroupsAndMasksMatchFrozenContract` passed on a real Windows presentation GPU on 2026-08-12.

### SR5 — 9-slice and tiled/repeated primitives — complete

Completion vehicle: **#134 / PR #135**. Concrete contract: [`SPRITE_PRIMITIVES.md`](SPRITE_PRIMITIVES.md).

SR5 adds:

- optional exact source-pixel `border = [left, top, right, bottom]` in canonical SpriteRegion metadata,
- runtime `quad|sliced|tiled` intent and explicit target size,
- normalized canonical-pivot preservation under resize,
- deterministic 9-slice partition and proportional opposing-border compression for undersized targets,
- explicit bounded geometry repetition for tiled center/edge cells,
- exact partial-final-tile geometry,
- trim-gap preservation and arbitrary `cw90` subrect UV mapping,
- per-patch atlas-safe linear-sampling bounds, including sub-texel partial tiles clamped to the represented texel center,
- caller-owned patch output with exact required count and no partial write on insufficient capacity,
- `MaximumSpritePrimitiveQuads == 4096` safety bound,
- one top-level SR4 semantic Sprite item for all patches,
- contiguous patch upload and one triangle-list draw per non-empty top-level Sprite,
- persistent vertex/transfer-buffer, sampler, pipeline and mask-target reuse.

### SR5 completion evidence

Final implementation head before completion documentation: `e05dc7158f5ea68b990f0e87f753629d6149ebab`.

Hosted validation on that head passed on 2026-08-12:

- CI run #604 — success,
- Content Evidence — success,
- Sprite S0 Contract — success,
- B0 Codex Wrapper — success,
- B0 Godot Agent Oracle — success.

The blocking owner Windows presentation-GPU fixture also passed with `TRACE2D_RUN_GPU_SMOKE=1`:

```text
SpritePrimitiveGpuSmokeTests.Sr5TiledLinearMaskAndCapacityReuseMatchContract
```

Observed result:

```text
Start 120: SpritePrimitiveGpuSmokeTests.Sr5TiledLinearMaskAndCapacityReuseMatchContract
1/1 Test #120: SpritePrimitiveGpuSmokeTests.Sr5TiledLinearMaskAndCapacityReuseMatchContract ... Passed
100% tests passed out of 1
Total Test time (real) = 2.28 sec
```

The first owner run exposed real linear-filter bleed on a 0.5-texel partial tile (`actual red=64`, `green=191` instead of pure green). SR5 was corrected so sub-texel partial bounds collapse to the represented texel center, and a backend-independent regression test now locks that behavior. The rerun above passed. No renderer/driver string was captured; do not invent one.

## 5. Remaining production Sprite renderer stages

### SR6 — pixel-perfect runtime presentation — active

Active vehicle: **#136 / draft PR #137**. Concrete contract: [`SPRITE_PIXEL_PERFECT.md`](SPRITE_PIXEL_PERFECT.md).

SR6 freezes and implements:

- explicit logical reference viewport -> final target integer-contain mapping,
- deterministic centered letterbox/pillarbox rectangle with floor-to-left/top remainder placement,
- logical-aspect `OrthographicView` reused by CPU mapping and production Sprite GPU vertex conversion,
- source-grid authority at the transformed untrimmed source pixel-edge origin `(0,0)`, independent of trim/pivot/packed rotation,
- exact-grid eligibility only for finite axis-aligned integer source-pixel bases, including flips and quarter-turn axis swaps,
- structured rejection of fractional scaling/arbitrary rotation instead of a false exactness claim,
- presentation-only nearest-edge snapping using `floor(x + 0.5)` with no mutation of authoritative SR1 pose history,
- explicit authoritative-current vs existing SR1-interpolated presentation-time evidence,
- exact nearest-sampling requirement for the SR6 production GPU path,
- full-target clear followed by deterministic SDL GPU viewport + scissor restriction to the integer content rectangle,
- preservation of SR4 painter/mask semantics and SR5 primitive atomicity/resource reuse,
- no intermediate SR6 upscale texture and no ordinary-frame explicit GPU readback/fence wait,
- O(1), fixed-size, allocation-free backend-independent mapping/validation APIs,
- target bounds that keep final viewport integers exactly representable by the GPU viewport float contract.

Backend-independent CPU coverage and the opt-in `SpritePixelPerfectGpuSmokeTests.Sr6IntegerViewportNearestMaskPrimitiveAndReuseMatchContract` fixture are committed. A prior implementation head passed hosted Windows MSVC configure/build/CTest and clean-clone validation on 2026-08-12; the current head must also be green before completion. The owner-local real Windows presentation-GPU fixture remains a blocking acceptance gate. Until that evidence is supplied, #137 stays draft and SR7 stays inactive.

### SR7 — production batching/resource reuse

Preserve painter order, merge only compatible contiguous work, reuse persistent/capacity-managed GPU/upload resources, avoid unmeasured per-frame heap work, and publish extraction/upload/draw/memory metrics.

### SR8 — renderer conformance/workloads

Commit CPU/GPU fixtures for transform, trim/pivot, rotated atlas storage, color/blend/sampling, ordering/groups/masks, 9-slice/tiled, pixel-perfect presentation and batch derivation.

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

SR6 is the only active Sprite child. **Do not merge #137 until the current hosted checks are green and the owner-local `SpritePixelPerfectGpuSmokeTests.Sr6IntegerViewportNearestMaskPrimitiveAndReuseMatchContract` run passes with `TRACE2D_RUN_GPU_SMOKE=1`.** After merge, close/confirm #136 and stop that continuation. Only the following `@GitHub Trace2D 다음 진행해줘` continuation may create exactly one SR7 child. After the complete #59 program, the exact next core item remains **#103 Benchmark B1** before #69 game-production work.
