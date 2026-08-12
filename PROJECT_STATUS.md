# Trace2D Project Status

Last repository-state update: **2026-08-12**

This file is the operational handoff for the next contributor or coding agent. Compiling code/tests, live PR/CI/merge state, explicit owner-approved contracts, and exact active issue acceptance outrank stale prose.

## Current state

Trace2D is an **AI-first / AI-operated C++20 2D engine** with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Completed AI-operated foundation:

- #97 machine-readable intent / Definition of Done — PR #115,
- #98 unified verification / diagnosis / repair / WorkResult — PR #116,
- #99 Workspace / human feedback loop — PR #117,
- #102 Benchmark B0 — PR #118 / squash `13a28d7baf8bd72d9f3233a57b2a048450825bee`.

Completed Sprite stages:

- #119 / S0 Sprite architecture and authority contract — PR #120 / squash `00dc587153bc4b0d6f6ac350d5491eec481585f0`,
- #121 / S1 canonical SpriteAsset/import representation — PR #122 / squash `27250bff8afd40f55edf2bfbed9be8b143f1ea1d`,
- #123 / SR0 renderer contract / canonical asset-render separation — PR #124 / squash `aa30a8e4498fd5edd6df9d2be7bb9a91bcdea5db`,
- #125 / SR1 transform geometry and fixed-step presentation history — PR #126 / squash `7b78c7bd5f792cfcf5a9171c62e06e792b1702ac`,
- #127 / SR2 trim/pivot/atlas/rotated-storage geometry and UV derivation — PR #128 / squash `a42e65c8a7953d38ad2d82894332c1f39da288f1`,
- #130 / SR3 color/alpha/blend/sampling — PR #131; real Windows presentation-GPU gate passed 2026-08-12,
- #132 / SR4 painter order/sorting groups/masking — PR #133 / squash `3c843bba10be536685c0fc70306b3fd6c75ed67a`; real Windows presentation-GPU gate passed 2026-08-12,
- #134 / SR5 9-slice and tiled/repeated Sprite primitives — PR #135 merged as `6d84ada945774d54c089d1a5bec3b63e17a43334`; all hosted and owner-GPU acceptance gates passed 2026-08-12.

**Active core program: #59 Complete Sprite program.**  
**Only active Sprite child: #136 / draft PR #137 — SR6 pixel-perfect runtime presentation.**  
**Blocking before SR6 completion: current hosted checks green + owner-local real Windows presentation-GPU fixture pass. SR7 must not start before that PR merges.**

## #59 Sprite program

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
Frozen architecture: [`docs/SPRITE_ARCHITECTURE.md`](docs/SPRITE_ARCHITECTURE.md).  
Canonical S1 format: [`docs/SPRITE_ASSET_FORMAT.md`](docs/SPRITE_ASSET_FORMAT.md).  
SR0 render seam: [`docs/SPRITE_RENDER_CONTRACT.md`](docs/SPRITE_RENDER_CONTRACT.md).  
SR1 transform/presentation seam: [`docs/SPRITE_TRANSFORM_PRESENTATION.md`](docs/SPRITE_TRANSFORM_PRESENTATION.md).  
SR2 atlas/trim/UV seam: [`docs/SPRITE_ATLAS_GEOMETRY.md`](docs/SPRITE_ATLAS_GEOMETRY.md).  
SR3 color/alpha/blend/sampling seam: [`docs/SPRITE_COLOR_SAMPLING.md`](docs/SPRITE_COLOR_SAMPLING.md).  
SR4 painter-order/group/masking seam: [`docs/SPRITE_ORDER_MASKING.md`](docs/SPRITE_ORDER_MASKING.md).  
SR5 9-slice/tiled seam: [`docs/SPRITE_PRIMITIVES.md`](docs/SPRITE_PRIMITIVES.md).  
SR6 pixel-perfect presentation seam: [`docs/SPRITE_PIXEL_PERFECT.md`](docs/SPRITE_PIXEL_PERFECT.md).

Fixed internal order:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete]
 -> SR4 [complete #132/#133] -> SR5 [complete #134/#135]
 -> SR6 [active #136/#137] -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Exactly one child is active at a time.

## #136 / SR6 — pixel-perfect runtime presentation — active via draft PR #137

SR6 adds presentation-only pixel mapping while preserving S0-SR5 canonical/runtime authority.

### Backend-independent mapping

`SpritePixelPerfectViewport2D` owns fixed-size derived presentation facts:

```text
logical reference size
 + acquired final target size
 -> integer contain scale
 -> centered exact content pixel rectangle
 -> logical-aspect OrthographicView
```

Frozen rules:

- `integer_scale = min(target_width / logical_width, target_height / logical_height)`,
- no fractional fallback; a target smaller than the logical reference is an explicit failure,
- odd unused pixels use floor placement on left/top and the remainder on right/bottom,
- target dimensions are bounded so viewport/scissor integers remain exactly representable by the GPU float viewport contract,
- the resolved `OrthographicView` uses logical aspect, not final target aspect,
- validation is O(1), fixed-size and allocation-free.

### Pixel-grid authority and snapping

The stable snap anchor is the transformed **untrimmed source pixel-edge origin `(0,0)`** obtained through the existing SR1/SR2 geometry contract. Pivot, trim and packed rotation do not replace that authority.

`ResolveSpritePixelPerfectPose`:

- selects either authoritative `currentFixed` or the existing SR1 interpolation path,
- records presentation-time mode and effective interpolation alpha as deterministic evidence,
- converts the source origin and one-source-pixel basis vectors through the same logical view used by rendering,
- accepts only finite axis-aligned integer logical-pixel bases, including semantic flips, integer magnification and quarter-turn axis swaps,
- rejects fractional magnification and arbitrary-angle rotation instead of claiming exactness,
- snaps source origin with `floor(x + 0.5)`, so integer logical-pixel translations preserve snap phase,
- applies the resulting delta only to the derived presentation pose,
- never mutates canonical Sprite metadata or authoritative `SpritePoseHistory2D`.

### Production SDL GPU path

`SpritePresentationRenderData` may carry a caller-owned SR6 viewport mapping for the current frame. The production Sprite GPU backend:

- requires one equal SR6 mapping for every Sprite when exact SR6 mode is enabled,
- requires nearest sampling for exact source-texel presentation,
- uses the SR6 logical view for Sprite vertex conversion,
- validates target/mapping state before beginning a GPU render pass,
- clears the full existing color target, then restricts drawing with the deterministic integer SDL GPU viewport and scissor,
- preserves SR4 painter order/sorting-group/mask semantics,
- preserves SR5 primitive atomicity and persistent vertex/transfer/sampler/pipeline/mask resource reuse,
- introduces no intermediate SR6 upscale texture,
- introduces no ordinary-frame explicit GPU readback or fence wait.

### Validation state

Committed backend-independent tests cover integer scale/centering, odd remainder placement, corrupt/too-small/inexact target rejection, 1x/integer source-pixel bases, flips, quarter turns, fractional/arbitrary rotation rejection, trim/pivot independence, authoritative-current vs interpolation selection/evidence, common camera/Sprite translation phase, history immutability and repeated caller-owned mapping reuse.

Committed owner GPU fixture:

```text
SpritePixelPerfectGpuSmokeTests.Sr6IntegerViewportNearestMaskPrimitiveAndReuseMatchContract
```

It proves on a real presentation GPU:

- centered integer viewport/scissor and untouched clear bars,
- nearest source texels occupying exact integer final-pixel blocks,
- SR4 stencil masking under the SR6 raster state,
- SR5 tiled primitive submission under the same raster state,
- persistent resource reuse,
- no additional ordinary-frame explicit readback/fence wait.

An earlier SR6 implementation head passed hosted Windows MSVC configure/build/CTest and clean-clone validation on 2026-08-12. The current PR head is still required to pass its fresh hosted checks after the final contract/safety edits. The owner-local GPU fixture above is also still required before PR #137 can leave draft/completion state. Do not invent owner GPU evidence.

## Owner-fixed core execution order

Routine `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, or equivalent follows the first incomplete/unblocked item below.

```text
AI-operated foundation
 -> #97 WorkSpec                                            [complete]
 -> #98 WorkResult verify/diagnose/repair                   [complete]
 -> #99 Workspace/review loop                               [complete]
 -> #102 Benchmark B0                                       [complete]

Content production
 -> #59 complete Sprite program                             [active]
      -> #119 S0 architecture                               [complete via #120]
      -> #121 S1 canonical asset/import                     [complete via #122]
      -> #123 SR0 asset/render contract                     [complete via #124]
      -> #125 SR1 transform/history/geometry                [complete via #126]
      -> #127 SR2 atlas/trim/pivot/UV geometry              [complete via #128]
      -> #130 SR3 color/alpha/blend/sampling                [complete via #131]
      -> #132 SR4 painter order/sorting groups/masking      [complete via #133]
      -> #134 SR5 9-slice/tiled/repeated primitives         [complete via #135]
      -> #136 SR6 pixel-perfect runtime presentation        [active via draft #137]
      -> SR7..SR8 renderer
      -> SA0..SA4 animation
      -> SPP0..SPP5 offline processing/generation
      -> SE2E -> SPERF
 -> #103 Benchmark B1 Sprite/animation/particle matched tasks

External game-production foundation
 -> #69 Game/Application boundary
 -> #70 Project manifest + external consumer build/install/package
 -> #71 Scene hierarchy + engine/game typed component composition
 -> #86 unified typed resource lifecycle
 -> #87 reusable scene templates + deterministic world lifecycle
 -> #88 Camera2D + Viewport2D
 -> #72 Input Actions + gamepad/mouse/text/IME
 -> #73 TileSet/TileMap
 -> #74 production UTF-8 font/text/localization
 -> #75 practical deterministic UI hierarchy/layout/widgets
 -> #104 Benchmark B2 autonomous top-down combat micro-game
 -> #89 Material2D + Shader2D
 -> #90 deterministic resolved-property tween animation
 -> #76 Physics2D
 -> #77 Audio
 -> #91 unified Agent-readable profiler/diagnostics
 -> #78 Linux/compiler/toolchain hardening
 -> #92 tiered real-GPU conformance/release validation
 -> #79 save/persistence + authored schema migration

Proof / later geometry and compatibility
 -> #12 flagship external game
 -> #60 generic Mesh2D foundation
 -> #61 Spine SP0 human license gate
```

Umbrellas/registers #13/#96/#100/#67/#85/#93/#101/#106 do not authorize bypassing this fixed order.

## Durable authority boundaries

WorkSpec/WorkResult/Workspace continue to enforce deterministic verification before perceptual review. Agent self-report is never independent truth.

Sprite authority remains:

```text
canonical authored Sprite metadata
 + authoritative typed runtime/animation state
        -> resolved/derived presentation
        -> backend renderer resources
```

GPU resources, pixels, Agent snapshots and review artifacts never become canonical Sprite/gameplay truth.

The accepted B0 cohort/raw evidence remains under `benchmarks/b0/`; B0 proves the matched methodology/evidence loop, not broad engine superiority.

## Continuation rule

SR6 is the only active child. The current continuation must:

1. keep #136 / draft PR #137 scoped only to SR6,
2. require the current hosted checks to pass,
3. require owner-local `TRACE2D_RUN_GPU_SMOKE=1` evidence for `SpritePixelPerfectGpuSmokeTests.Sr6IntegerViewportNearestMaskPrimitiveAndReuseMatchContract`,
4. keep PR #137 draft/not merged until both gates are satisfied,
5. after both gates pass, record exact evidence, mark ready/merge #137, confirm #136 closed, and stop,
6. not create SR7 in that completion continuation.

Only the **following** `@GitHub Trace2D 다음 진행해줘` continuation may create exactly one SR7 child.
