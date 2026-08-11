# Sprite Pipeline Contract

Status: **S0/S1/SR0/SR1/SR2/SR3 complete; SR4 painter-order/sorting-group/masking is the next child, but must be created only after PR #131 is merged/closed**

Operational umbrella: GitHub Issue #59.  
Frozen S0 architecture: [`SPRITE_ARCHITECTURE.md`](SPRITE_ARCHITECTURE.md).  
Machine-readable S0 invariants: [`contracts/sprite-s0.json`](contracts/sprite-s0.json).  
Canonical S1 format: [`SPRITE_ASSET_FORMAT.md`](SPRITE_ASSET_FORMAT.md).  
SR0 render seam: [`SPRITE_RENDER_CONTRACT.md`](SPRITE_RENDER_CONTRACT.md).  
SR1 transform/presentation seam: [`SPRITE_TRANSFORM_PRESENTATION.md`](SPRITE_TRANSFORM_PRESENTATION.md).  
SR2 atlas/trim/UV seam: [`SPRITE_ATLAS_GEOMETRY.md`](SPRITE_ATLAS_GEOMETRY.md).  
SR3 color/alpha/blend/sampling seam: [`SPRITE_COLOR_SAMPLING.md`](SPRITE_COLOR_SAMPLING.md).

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
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete] -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Completed stage: **SR3 / #130 / PR #131**.  
Exact next stage after PR #131 merges green: **SR4 — painter order, sorting groups and Sprite masking**.

Do not create or begin SR4 in the same continuation that finalizes PR #131.

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

Semantic ID lookup and relationship checks are setup-only. Successful steady-state extraction is O(1), fixed-size and allocation-free. CPU page resource keys contain canonical identity/intent only. Material/sampler/blend/mask/primitive compatibility remains finite and typed. No GPU handle or normalized UV becomes canonical state.

### SR1 — transform/geometry and presentation history — complete

Merged via #125/#126 (`7b78c7bd5f792cfcf5a9171c62e06e792b1702ac`).

SR1 reuses `scene::Transform2D` for authoritative transform semantics and adds only Sprite-specific flip/history state. Creation/reset/load/teleport synchronizes previous/current; successful fixed-step commit advances history; exact-frame uses current authority; interactive presentation interpolates position/scale and shortest-arc rotation while flip remains discrete current state.

Logical geometry uses untrimmed `source_size` plus exact rational pivot through one source-space Y-down -> local Y-up boundary, then semantic flip, non-uniform/negative scale, CCW rotation and translation. The implementation is O(1), fixed-size, allocation-free on the normal API, and computes at most one sin/cos pair per quad.

### SR2 — atlas/trim/pivot/rotated packing — complete

Merged via #127/#128 (`a42e65c8a7953d38ad2d82894332c1f39da288f1`).  
Concrete contract: [`SPRITE_ATLAS_GEOMETRY.md`](SPRITE_ATLAS_GEOMETRY.md).

SR2 consumes:

```text
ResolvedSpriteRegion
 + SpritePose2D
 + pixels_per_unit
 -> SpriteDrawQuad
```

The visible quad uses the exact trim rectangle embedded in the original untrimmed source space. `packed_rect` never determines logical placement. Exact rational/out-of-source pivot, flips, negative/non-uniform scale, PPU, runtime rotation and translation retain SR1 meaning.

Canonical page UV truth remains pixel-edge based:

```text
u0 = packed.x / page.width
v0 = packed.y / page.height
u1 = (packed.x + packed.width) / page.width
v1 = (packed.y + packed.height) / page.height
```

No half-texel offset is added. `none` maps logical corners directly to packed TL/TR/BR/BL; `cw90` undoes clockwise storage by the frozen corner permutation. Packed orientation changes UV mapping/storage dimensions only, never logical positions.

SR2 rejects unresolved/invalid pose/PPU/source/pivot state, zero/corrupted page/trim/packed extents, out-of-bounds trim/packed rectangles, rotation-specific extent mismatch, unsupported rotation values and numeric overflow through stable fixed-size error categories.

## 5. SR3 — color/alpha/blend/sampling — complete

Concrete contract: [`SPRITE_COLOR_SAMPLING.md`](SPRITE_COLOR_SAMPLING.md).  
Completion vehicle: **#130 / PR #131 / branch `agent/sprite-sr3-color-sampling`**.

SR3 consumes exact SR2 geometry instead of introducing a parallel full-texture-quad semantic path:

```text
ResolvedSpriteRegion
 + SpritePose2D
 + pixels_per_unit
 + SpriteAppearance2D
        -> SpritePresentation2D {
             SpriteDrawQuad quad,
             SpriteAppearanceContractData appearance
           }
        -> production Renderer SpritePresentationRenderData
        -> cached SDL GPU state
```

### Runtime appearance authority

Finite runtime intent is:

```text
SpriteAppearance2D {
    tint     : linear RGBA, each finite in [0,1]
    opacity  : finite [0,1]
    sampling : inherit_asset | nearest | linear
    blend    : normal | additive | multiply | screen
}
```

Invalid authoritative values fail structurally. They are never silently clamped.

`inherit_asset` resolves from canonical `SpriteAsset::sampling` without mutating the asset or resolved region. Unsupported enum values fail instead of falling through to backend defaults.

### Straight-alpha / linear-working-space boundary

Canonical texture source truth remains **straight alpha**. Page color-space intent remains canonical and is represented by an sRGB-capable sampled format for sRGB pages or linear UNORM for linear pages.

Once sampled RGB is in linear working space, the built-in fragment contract is exactly:

```text
A = sampledStraight.a * tint.a * opacity
P = sampledLinear.rgb * tint.rgb * A
fragment = (P, A)
```

The canonical source pixels are never rewritten to premultiplied authority. Premultiplication is derived immediately at the fragment boundary before fixed-function blending.

The built-in premultiplied blend factors are frozen:

```text
normal   : color src=ONE,       dst=ONE_MINUS_SRC_ALPHA
additive : color src=ONE,       dst=ONE
multiply : color src=DST_COLOR, dst=ONE_MINUS_SRC_ALPHA
screen   : color src=ONE,       dst=ONE_MINUS_SRC_COLOR

all alpha: src=ONE, dst=ONE_MINUS_SRC_ALPHA, op=ADD
```

No arbitrary blend-factor property bag belongs to SR3. Generic programmable Material2D/Shader2D remains #89.

### Atlas-safe sampling without changing SR2 UV truth

SR2 pixel-edge UVs remain canonical derived geometry. SR3 adds a separate texel-center sampling guard:

```text
umin = (packed.x + 0.5) / page.width
umax = (packed.x + packed.width  - 0.5) / page.width
vmin = (packed.y + 0.5) / page.height
vmax = (packed.y + packed.height - 0.5) / page.height
```

The interpolated fragment UV is clamped to those bounds before sampling. One-pixel extents collapse min/max to the same texel center. `cw90` keeps the same packed-region guard while SR2 alone owns UV corner permutation.

This separation is intentional: filtering safety must never rewrite canonical SR2 pixel-edge UV geometry.

### Production GPU state and reuse

PR #131 implements a dedicated SR3 SDL GPU backend while retaining the legacy Sprite/particle sampler/pipeline resources unchanged.

Renderer/device lifetime owns:

- two persistent Sprite samplers: nearest and linear,
- four persistent built-in blend pipelines for the renderer target format,
- sRGB/linear-tagged sampled texture resources matching canonical page color-space intent,
- reusable capacity-managed six-vertex-per-Sprite upload resources that consume exact SR2 quad positions/UVs,
- small per-draw fragment uniform data for tint, opacity and sample bounds.

Ordinary presentation performs no explicit GPU readback or fence wait. Explicit capture/conformance may synchronize because observation requires readback.

SR3 deliberately preserves caller order and does not yet implement broad compatibility batching or mixed Sprite/particle painter semantics. SR4 owns painter-order/group/mask semantics; SR7 owns production batching/culling/reuse policy beyond the current fixed cache/capacity requirements.

### Metrics and deterministic proof

Renderer metrics expose:

- SR3 Sprite draw/submission counts,
- Sprite sampler creation count,
- Sprite pipeline creation count,
- retained Sprite vertex capacity,
- explicit GPU readback count,
- explicit GPU fence-wait count.

Backend-independent tests prove validation, exact appearance extraction, sRGB/linear encoding identity, one-pixel and `cw90` sample bounds, premultiplication, all four blend equations, transactional SR2->SR3 composition, and repeated fixed-size extraction.

### Real GPU completion evidence

The committed opt-in test:

```text
SpriteGpuSmokeTests.Sr3ColorSamplingBlendAndCachesMatchFrozenContract
```

passed on a real Windows presentation GPU on **2026-08-12** with `TRACE2D_RUN_GPU_SMOKE=1`.

Command used after successful configure/build:

```powershell
ctest --test-dir .\build\windows-msvc -C Debug -R "SpriteGpuSmokeTests.Sr3ColorSamplingBlendAndCachesMatchFrozenContract" --output-on-failure
```

Observed result:

```text
Test project D:/Trace2D-pr131/build/windows-msvc
    Start 94: SpriteGpuSmokeTests.Sr3ColorSamplingBlendAndCachesMatchFrozenContract
1/1 Test #94: SpriteGpuSmokeTests.Sr3ColorSamplingBlendAndCachesMatchFrozenContract ... Passed
100% tests passed out of 1
Total Test time (real) = 3.07 sec
```

That real-GPU fixture proves:

- nearest/linear sampling,
- atlas-edge linear filtering does not bleed the adjacent texel,
- sRGB sampled texture behavior is decoded exactly once and differs correctly from linear-page behavior,
- normal/additive/multiply/screen captured output matches the CPU oracle,
- tint/opacity premultiplication,
- exactly the fixed persistent sampler/pipeline set is reused,
- retained vertex capacity is reused,
- ordinary frames keep explicit readback/fence waits at zero.

No renderer/driver string was captured, so the completion record intentionally does not invent one.

## 6. Remaining production Sprite renderer stages

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

PR #131 is the SR3 completion vehicle. While it remains open, routine continuation may only finalize its evidence/status/merge state. After it merges and #130 closes, stop that continuation. On the next continuation only, create exactly one SR4 child issue and begin SR4 from the frozen order above.

After the complete #59 program, the exact next core item remains **#103 Benchmark B1** before #69 game-production work.
