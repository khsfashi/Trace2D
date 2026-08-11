# Sprite Pipeline Contract

Status: **S0/S1/SR0/SR1 complete; SR2 atlas/trim/pivot/UV geometry active via #127**

Operational umbrella: GitHub Issue #59.  
Frozen S0 architecture: [`SPRITE_ARCHITECTURE.md`](SPRITE_ARCHITECTURE.md).  
Machine-readable S0 invariants: [`contracts/sprite-s0.json`](contracts/sprite-s0.json).  
Canonical S1 format: [`SPRITE_ASSET_FORMAT.md`](SPRITE_ASSET_FORMAT.md).  
SR0 render seam: [`SPRITE_RENDER_CONTRACT.md`](SPRITE_RENDER_CONTRACT.md).  
SR1 transform/presentation seam: [`SPRITE_TRANSFORM_PRESENTATION.md`](SPRITE_TRANSFORM_PRESENTATION.md).  
SR2 atlas/trim/UV seam: [`SPRITE_ATLAS_GEOMETRY.md`](SPRITE_ATLAS_GEOMETRY.md).

This document owns the complete fixed Sprite stage order and capability target. Stage-local documents may refine implementation details but cannot silently change S0 authority or replace canonical authored/runtime truth with renderer/tool state.

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

S0 authority direction:

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
- authored image metadata is exact pixel-space truth; normalized UVs/GPU handles/batch IDs are derived,
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
 -> SR0 [complete] -> SR1 [complete] -> SR2 [active] -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Current stage: **SR2 / #127**.  
Exact next stage after SR2 merges green: **SR3 — color/alpha/blend/sampling**.

Do not begin SR3 while #127 is active.

## 4. Completed foundation

### S0 — architecture / authority — complete

Frozen by #119/#120 and `SPRITE_ARCHITECTURE.md`:

- authority/ownership matrix,
- exact source-space coordinates,
- pivot/trim/rotated-storage semantics,
- fixed-step authoritative vs interactive presentation state,
- exact-frame capture semantics,
- resource/view/material compatibility seams,
- painter-order invariant,
- #97-#99 verification/review authority,
- hot-path/offline-tooling separation.

### S1 — canonical Sprite asset/import — complete

Merged via #121/#122 (`27250bff8afd40f55edf2bfbed9be8b143f1ea1d`).

Implemented:

- versioned `.sprite.toml` (`trace2d.sprite`, v1),
- normalized project-relative Sprite/texture identity,
- ordered atlas pages/regions,
- exact source/trim/packed metadata,
- exact reduced rational pivot,
- `none` / `cw90` storage rotation,
- explicit sRGB/linear, straight-alpha and nearest/linear sampling intent,
- strict structured validation,
- deterministic canonical serialization,
- immutable cache reuse and decoded CPU texture-dimension validation,
- no SDL/GPU handles or normalized UVs in canonical assets.

### SR0 — asset/render separation — complete

Merged via #123/#124 (`aa30a8e4498fd5edd6df9d2be7bb9a91bcdea5db`).

Implemented:

```text
canonical SpriteAsset
 -> setup-time region resolution
 -> ResolvedSpriteRegion
 -> O(1) SpriteRenderContractData extraction
 -> later presentation stages
```

Rules:

- semantic ID lookup/string relationship checks are setup-only,
- successful steady-state extraction is O(1) and allocation-free,
- CPU page resource key contains canonical identity/intent only,
- finite built-in material/sampler/blend/mask/primitive compatibility seam,
- no GPU/SDL handle or normalized UV enters canonical/render-contract state,
- existing `OrthographicView` is reused for future #88 view resolution,
- only compatible contiguous work may later batch.

### SR1 — transform/geometry and presentation history — complete

Merged via #125/#126 (`7b78c7bd5f792cfcf5a9171c62e06e792b1702ac`).

SR1 reuses `scene::Transform2D` for authoritative transform semantics and adds only Sprite-specific flip/history state.

```text
scene::Transform2D
 + flipX / flipY
 -> SpritePose2D
 -> previousFixed / currentFixed
```

Fixed-step rules:

- creation/reset/load/teleport/snap: `previousFixed = currentFixed = new pose`,
- successful fixed step: `previousFixed = old currentFixed; currentFixed = new pose`,
- invalid snap/commit leaves history unchanged,
- exact-frame presentation copies `currentFixed`,
- interactive alpha must be finite in `[0,1]`,
- position/scale interpolate linearly,
- rotation uses shortest signed float-radian arc,
- flip state is discrete and comes from `currentFixed`,
- presentation never writes back to authoritative history.

Logical geometry uses untrimmed `source_size` plus exact rational pivot through one source-space Y-down -> local Y-up boundary, then semantic flip, non-uniform/negative scale, CCW rotation and translation. The implementation is O(1), fixed-size, allocation-free on the normal API, and computes at most one sin/cos pair per quad.

## 5. SR2 — atlas/trim/pivot/rotated packing — active

Concrete contract: [`SPRITE_ATLAS_GEOMETRY.md`](SPRITE_ATLAS_GEOMETRY.md).

SR2 consumes:

```text
ResolvedSpriteRegion
 + SpritePose2D
 + pixels_per_unit
        -> SpriteDrawQuad
```

### Visible trim placement

The visible quad uses the exact trim rectangle embedded in the original untrimmed source space:

```text
TL = (trim_offset.x,               trim_offset.y)
TR = (trim_offset.x + trim_size.w, trim_offset.y)
BR = (trim_offset.x + trim_size.w, trim_offset.y + trim_size.h)
BL = (trim_offset.x,               trim_offset.y + trim_size.h)
```

Each corner passes through the same refactored SR1 source-point transform context. `packed_rect` never determines logical placement. Exact rational/out-of-source pivot, semantic flips, negative scale, PPU, runtime rotation and translation retain SR1 meaning.

### Canonical UV truth

Trace2D page-space normalized UVs use top-left origin, +u right, +v down and pixel edges:

```text
u0 = packed.x / page.width
v0 = packed.y / page.height
u1 = (packed.x + packed.width) / page.width
v1 = (packed.y + packed.height) / page.height
```

No half-texel offset is added. SR3 owns sampler behavior; backend-native coordinate adaptation stays at the backend boundary.

`none` maps logical corners directly to packed TL/TR/BR/BL. `cw90` undoes clockwise storage through:

```text
logical TL -> packed TR
logical TR -> packed BR
logical BR -> packed BL
logical BL -> packed TL
```

Packed orientation therefore changes UV mapping/storage dimensions only, never logical positions.

### Structured guardrails and hot path

SR2 rejects unresolved/invalid pose/PPU/source/pivot state, zero/corrupted page/trim/packed extents, out-of-bounds trim/packed rectangles, rotation-specific extent mismatch, unsupported rotation values and non-finite/overflow output through stable allocation-free error/field categories.

Normal derivation remains:

- O(1),
- fixed four vertices,
- one shared geometry-context build and one sin/cos pair maximum,
- reusable caller-owned output,
- no vector/list allocation,
- no semantic string lookup/hash,
- no filesystem/TOML/image decode/path normalization,
- no GPU/SDL initialization,
- no canonical-state mutation or background work.

## 6. Remaining production Sprite renderer stages

### SR3 — color/alpha/blend/sampling

Implement tint/opacity, explicit alpha conversion boundary, supported blend modes only with backend conformance, and cached nearest/linear sampler behavior.

### SR4 — painter order/sorting groups/masking

Implement layer/stable order, sorting groups and bounded Sprite mask/clip semantics. Global texture/material sorting that changes semantic order remains forbidden.

### SR5 — 9-slice and tiled/repeated primitives

Implement explicit border/stretch/repeat semantics through the same canonical asset/resource/render path.

### SR6 — pixel-perfect runtime presentation

Freeze mapping among source pixels, `pixels_per_unit`, world transforms, resolved camera/view, logical viewport and final target pixels, including interpolation/camera interaction.

### SR7 — production batching/resource reuse

Preserve painter order, merge only compatible contiguous work, reuse persistent/capacity-managed GPU/upload resources, avoid unmeasured per-frame heap work, and publish extraction/upload/draw/memory metrics.

### SR8 — renderer conformance/workloads

Commit CPU/GPU fixtures for transform, trim/pivot, rotated atlas storage, color/blend/sampling, ordering/groups/masks, 9-slice/tiled, pixel-perfect presentation and batch derivation.

## 7. Deterministic Sprite animation

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

## 8. Offline Sprite processing / generation

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

## 9. End-to-end proof

### SE2E

Prove request/import -> raw/generated pixels -> deterministic QA -> canonical asset -> animation -> headless exact-frame verification -> renderer/capture -> perceptual/human review.

### SPERF

Publish visible/animated counts, atlas pages, compatibility transitions, draws, culling, animation/extraction CPU time, upload bytes, retained capacities, texture/page memory/utilization and capture cost.

## 10. Verification/review authority

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

## 11. Explicit handoffs / non-goals

#59 does not silently absorb:

- generic programmable Material2D/Shader2D (#89),
- general Camera2D/Viewport2D ownership (#88),
- generic resource lifecycle (#86),
- general world/component hierarchy (#71),
- arbitrary textured/deformable Mesh2D (#60),
- Spine/skeletal runtime before #61/#101 license/product decision,
- PBR/deferred/render graph/bindless/GPU-driven scene architecture,
- generic ECS/reflection/property bag or custom allocator/job system without evidence.

## 12. Handoff rule

Every Sprite child PR must:

1. implement only the first incomplete stage,
2. include tests/fixtures/evidence,
3. update relevant stage contracts, this roadmap and `PROJECT_STATUS.md`,
4. preserve enough structured evidence to continue without chat history,
5. avoid beginning the next child until the current PR merges green.

After the complete #59 program, the exact next core item is **#103 Benchmark B1** before #69 game-production work.
