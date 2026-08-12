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
- #134 / SR5 9-slice and tiled/repeated Sprite primitives — completion vehicle PR #135; all hosted and owner-GPU acceptance gates passed 2026-08-12.

**Active core program: #59 Complete Sprite program.**  
**SR5 acceptance is complete. PR #135 is the completion/merge vehicle.**  
**Exact next child after #135 merges: SR6 — pixel-perfect runtime presentation. Do not create SR6 in the same continuation that completes SR5.**

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

Fixed internal order:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete]
 -> SR4 [complete #132/#133] -> SR5 [complete #134/#135] -> SR6 [next] -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Exactly one child is active at a time.

## #134 / SR5 — acceptance complete via PR #135

SR5 extends canonical Sprite metadata and production presentation with deterministic 9-slice and tiled/repeated primitives while preserving S0-SR4 authority.

### Canonical/runtime split

Canonical `SpriteRegion` now has optional exact source-pixel border metadata:

```text
border = [left, top, right, bottom]
```

Omission remains backward-compatible schema-v1 zero. Borders are validated against untrimmed source size and are never rewritten by trim or packed rotation.

Runtime presentation chooses:

```text
quad | sliced | tiled
```

with explicit target size. Resizing preserves the canonical pivot's normalized source position rather than mutating authored pivot state.

### Geometry and sampling

SR5 implements:

- deterministic source/target 3x3 partition,
- proportional opposing-border compression when target size is undersized,
- explicit repeated geometry for tiled center/edge cells,
- partial final tiles without texture wrap authority,
- trim intersection that preserves transparent logical gaps,
- `none` and `cw90` arbitrary source-subrect packed mapping,
- exact pixel-edge UV geometry plus per-patch texel-center sample bounds,
- sub-texel partial tiles clamped to the represented texel center to avoid linear-filter atlas bleed,
- exact count-first caller-owned patch output,
- no partial writes on insufficient capacity,
- hard safety bound `MaximumSpritePrimitiveQuads == 4096`.

### SR4 atomicity and GPU path

Primitive expansion does not create separate semantic Sprite items:

```text
one SpritePresentationRenderData
 -> one SR4 order/group/mask item
 -> N contiguous SR5 quad slots
 -> one triangle-list draw for N > 0
```

The SDL GPU path preserves persistent sampler/pipeline/mask state, grows reusable vertex/transfer-buffer capacity geometrically, and performs no ordinary-frame explicit GPU readback or fence wait.

### Validation evidence

Final implementation head before completion documentation:

```text
e05dc7158f5ea68b990f0e87f753629d6149ebab
```

Hosted checks on that head all passed:

- CI run #604 — success,
- Content Evidence — success,
- Sprite S0 Contract — success,
- B0 Codex Wrapper — success,
- B0 Godot Agent Oracle — success.

Blocking owner Windows presentation-GPU fixture:

```text
SpritePrimitiveGpuSmokeTests.Sr5TiledLinearMaskAndCapacityReuseMatchContract
```

First owner execution correctly exposed a real 0.5-texel partial-tile linear-filter bleed. The observed failed sample mixed red/green (`red=64`, `green=191`) instead of the intended pure green. The sample-bound calculation was corrected and locked with the backend-independent `SubTexelPartialTileClampsToRepresentedTexelCenter` regression test.

The owner reran configure/build and the GPU fixture with `TRACE2D_RUN_GPU_SMOKE=1`. Final observed result:

```text
Start 120: SpritePrimitiveGpuSmokeTests.Sr5TiledLinearMaskAndCapacityReuseMatchContract
1/1 Test #120: SpritePrimitiveGpuSmokeTests.Sr5TiledLinearMaskAndCapacityReuseMatchContract ... Passed
100% tests passed out of 1
Total Test time (real) = 2.28 sec
```

No renderer/driver string was captured; do not invent one.

## Exact next stage: SR6

SR6 owns pixel-perfect runtime presentation. It must freeze the mapping among source pixels, `pixels_per_unit`, world transforms, resolved camera/view, logical viewport and final target pixels, including interpolation/camera interaction. It must preserve SR0-SR5 canonical/derived authority and keep deterministic mapping facts machine-verifiable.

Do not begin SR7, animation, offline Sprite processing, or game-production foundation work before SR6 is completed in the fixed order.

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
      -> SR6 pixel-perfect runtime presentation              [exact next]
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

SR5 acceptance is complete. The completion continuation must:

1. record the hosted and owner-GPU evidence,
2. update `docs/SPRITES.md`, `docs/SPRITE_PRIMITIVES.md` and this file,
3. mark PR #135 ready and merge it only with the final acceptance state intact,
4. confirm #134 closed,
5. stop without creating SR6.

The **next** `@GitHub Trace2D 다음 진행해줘` continuation may create exactly one SR6 child and no later stage.
