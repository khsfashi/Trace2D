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

- #119 / S0 architecture and authority — PR #120 / squash `00dc587153bc4b0d6f6ac350d5491eec481585f0`,
- #121 / S1 canonical SpriteAsset/import — PR #122 / squash `27250bff8afd40f55edf2bfbed9be8b143f1ea1d`,
- #123 / SR0 renderer contract — PR #124 / squash `aa30a8e4498fd5edd6df9d2be7bb9a91bcdea5db`,
- #125 / SR1 transform/history — PR #126 / squash `7b78c7bd5f792cfcf5a9171c62e06e792b1702ac`,
- #127 / SR2 trim/pivot/atlas/rotated storage — PR #128 / squash `a42e65c8a7953d38ad2d82894332c1f39da288f1`,
- #130 / SR3 color/alpha/blend/sampling — PR #131; owner Windows presentation-GPU gate passed 2026-08-12,
- #132 / SR4 painter order/sorting groups/masking — PR #133 / squash `3c843bba10be536685c0fc70306b3fd6c75ed67a`; owner Windows presentation-GPU gate passed 2026-08-12,
- #134 / SR5 9-slice/tiled primitives — PR #135 / squash `6d84ada945774d54c089d1a5bec3b63e17a43334`; hosted + owner GPU gates passed 2026-08-12,
- #136 / SR6 pixel-perfect runtime presentation — PR #137 merged into `main` as `3fd6e5a439a1e327bd89797f5d5a5a9dae69dace`; owner fixture `SpritePixelPerfectGpuSmokeTests.Sr6IntegerViewportNearestMaskPrimitiveAndReuseMatchContract` passed 2026-08-12.

**Active core program: #59 Complete Sprite program.**  
**Only active Sprite child: #138 / draft PR #139 — SR7 production batching, persistent resource reuse, and hot-path metrics.**  
**Blocking before SR7 completion: final hosted checks green + owner-local real Windows presentation-GPU fixture pass. SR8 must not start before #139 merges.**

## #59 Sprite program

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
Frozen architecture: [`docs/SPRITE_ARCHITECTURE.md`](docs/SPRITE_ARCHITECTURE.md).  
Canonical S1 format: [`docs/SPRITE_ASSET_FORMAT.md`](docs/SPRITE_ASSET_FORMAT.md).  
SR0 render seam: [`docs/SPRITE_RENDER_CONTRACT.md`](docs/SPRITE_RENDER_CONTRACT.md).  
SR1 transform seam: [`docs/SPRITE_TRANSFORM_PRESENTATION.md`](docs/SPRITE_TRANSFORM_PRESENTATION.md).  
SR2 atlas/trim/UV seam: [`docs/SPRITE_ATLAS_GEOMETRY.md`](docs/SPRITE_ATLAS_GEOMETRY.md).  
SR3 color/sampling seam: [`docs/SPRITE_COLOR_SAMPLING.md`](docs/SPRITE_COLOR_SAMPLING.md).  
SR4 order/masking seam: [`docs/SPRITE_ORDER_MASKING.md`](docs/SPRITE_ORDER_MASKING.md).  
SR5 primitives seam: [`docs/SPRITE_PRIMITIVES.md`](docs/SPRITE_PRIMITIVES.md).  
SR6 pixel-perfect seam: [`docs/SPRITE_PIXEL_PERFECT.md`](docs/SPRITE_PIXEL_PERFECT.md).  
SR7 batching/hot-path seam: [`docs/SPRITE_BATCHING_SR7.md`](docs/SPRITE_BATCHING_SR7.md).

Fixed internal order:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete]
 -> SR4 [complete] -> SR5 [complete] -> SR6 [complete]
 -> SR7 [active #138/#139] -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Exactly one Sprite child is active at a time.

## #138 / SR7 — production batching/resource reuse/hot-path metrics — active via draft PR #139

SR7 optimizes the SR4-SR6 production `SpritePresentationRenderData` path without weakening semantic authority.

### Frozen semantic rules

- Full SR4 painter order and mask validation occurs before SR7 culling/batching.
- Texture, material/pipeline, sampler, blend, GPU resource identity, or estimated batch benefit never reorder Sprite items.
- Only **compatible contiguous visible work in resolved painter order** may merge.
- Fully culled or zero-output top-level Sprite work emits no pixels and does not split an otherwise-compatible visible run.
- SR5 primitive patches remain atomic under one top-level SR4 item and preserve patch order.
- SR6 logical view is the visibility and vertex-conversion view when pixel-perfect presentation is active.

### Compatibility seam

`SpriteBatchCompatibility2D` includes:

```text
texture/resource identity
+ resolved material/pipeline identity
+ sampler compatibility
+ blend compatibility
+ exact mask mode/id
```

`0` is the invalid material/pipeline identity. SR7 executes only `BuiltInSpriteMaterialPipelineIdentity == 1`; programmable Material2D/Shader2D remains owned by #89.

Tint and opacity are derived per-vertex presentation data rather than batch-key state, so appearance differences alone do not force an otherwise-compatible draw split.

### Production hot path

The current #139 implementation:

1. validates complete Sprite presentation input and resolves SR4 painter/mask order,
2. evaluates top-level visibility against the exact resolved presentation view,
3. compacts only visible regular/SR5 primitive vertices in that order,
4. derives compatible contiguous visible runs without resource-based sorting,
5. performs one visible upload and one triangle-list draw per non-empty run,
6. retains sampler/pipeline/vertex-transfer/mask resources across frames,
7. grows vertex GPU/transfer capacity geometrically only on a visible-quad high-water mark,
8. uses ordinary-frame upload cycling and adds no explicit ordinary-frame GPU readback/fence wait.

The SR7 scan/compaction/run work is O(N + Q) after the existing SR4 semantic order resolution, where N is top-level Sprite count and Q is emitted visible quad count.

### Deterministic observability

Production metrics now distinguish semantic submissions from actual GPU work:

- submitted production presentation Sprites,
- visible production presentation Sprites,
- culled/zero-output production presentation Sprites,
- actual production batch/draw count,
- uploaded visible quad count,
- uploaded Sprite vertex bytes,
- compatibility-run count,
- retained Sprite vertex capacity in quad slots and bytes,
- sampler/pipeline/mask-target creation counts,
- explicit GPU readback/fence-wait counters.

`drawCalls` remains actual encoded GPU draw work rather than semantic draw attempts.

### Validation state

Backend-independent SR7 tests are committed for compatible run merging, culled/zero-output gaps, compatibility-key splits, inclusive resolved-view visibility and primitive any-patch visibility.

The blocking owner Windows presentation-GPU fixture is:

```text
SpriteBatchGpuSmokeTests.Sr7BatchesCullsPreservesAppearanceMaskPrimitivePixelPerfectAndReuse
```

It is designed to prove in one real presentation-GPU gate:

- two compatible adjacent visible Sprites with different tint/opacity share one actual GPU draw,
- one incompatible texture transition creates exactly one additional draw,
- a fully culled Sprite between compatible visible neighbors creates no extra draw,
- captured pixels preserve each Sprite's derived appearance,
- SR4 stencil masking remains correct,
- SR5 tiled primitive submission remains atomic/correct,
- SR6 integer viewport/scissor and nearest presentation remain correct,
- repeated ordinary frames reuse retained sampler/pipeline/vertex capacity,
- repeated ordinary frames add no explicit readback/fence wait.

Do not invent owner GPU evidence. PR #139 remains draft until the final hosted head is green and this owner-local fixture passes with `TRACE2D_RUN_GPU_SMOKE=1`.

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
      -> S0/S1/SR0..SR6                                    [complete]
      -> #138 SR7 batching/resource reuse/hot-path metrics  [active via draft #139]
      -> SR8 renderer conformance/workloads
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

SR7 is the only active Sprite child. The current/next continuation must:

1. keep #138 / draft PR #139 scoped only to SR7,
2. require the final #139 hosted checks to pass,
3. require owner-local `TRACE2D_RUN_GPU_SMOKE=1` evidence for `SpriteBatchGpuSmokeTests.Sr7BatchesCullsPreservesAppearanceMaskPrimitivePixelPerfectAndReuse`,
4. keep PR #139 draft/not merged until both gates are satisfied,
5. after both gates pass, record exact evidence, mark ready/merge #139, confirm #138 closed, and stop,
6. not create SR8 in that completion continuation.

Only the **following** `@GitHub Trace2D 다음 진행해줘` continuation may create exactly one SR8 child.