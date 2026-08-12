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
- #130 / SR3 color/alpha/blend/sampling — PR #131 merged; real Windows presentation-GPU gate passed 2026-08-12.

**Active core program: #59 Complete Sprite program.**  
**Active child: #132 / SR4 — painter order, sorting groups and Sprite masking.**  
**Active implementation vehicle: draft PR #133 / branch `sprite-sr4-order-mask`.**  
**SR4 is not complete until hosted validation is green and the committed real Windows presentation-GPU order/mask fixture passes.**  
**Exact next child after SR4 merges: SR5 — 9-slice and tiled/repeated Sprite primitives. Do not create SR5 early.**

## #59 Sprite program

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
Frozen architecture: [`docs/SPRITE_ARCHITECTURE.md`](docs/SPRITE_ARCHITECTURE.md).  
Canonical S1 format: [`docs/SPRITE_ASSET_FORMAT.md`](docs/SPRITE_ASSET_FORMAT.md).  
SR0 render seam: [`docs/SPRITE_RENDER_CONTRACT.md`](docs/SPRITE_RENDER_CONTRACT.md).  
SR1 transform/presentation seam: [`docs/SPRITE_TRANSFORM_PRESENTATION.md`](docs/SPRITE_TRANSFORM_PRESENTATION.md).  
SR2 atlas/trim/UV seam: [`docs/SPRITE_ATLAS_GEOMETRY.md`](docs/SPRITE_ATLAS_GEOMETRY.md).  
SR3 color/alpha/blend/sampling seam: [`docs/SPRITE_COLOR_SAMPLING.md`](docs/SPRITE_COLOR_SAMPLING.md).  
SR4 painter-order/group/masking seam: [`docs/SPRITE_ORDER_MASKING.md`](docs/SPRITE_ORDER_MASKING.md).

Fixed internal order:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete] -> SR4 [active #132/#133] -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Exactly one child is active at a time.

## #132 / SR4 — active via PR #133

Concrete contract: [`docs/SPRITE_ORDER_MASKING.md`](docs/SPRITE_ORDER_MASKING.md).

SR4 extends the already-resolved SR0-SR3 presentation seam with finite semantic order/group/mask intent:

```text
SpritePresentation2D
 + SpriteOrder2D
 + SpriteMask2D
        -> validated semantic sequence
        -> production renderer submission in that exact order
        -> optional bounded SDL GPU stencil masking
```

### Backend-independent ordering

Current implementation adds:

- signed 32-bit `layer` and `order`,
- unsigned 64-bit stable semantic order with `UINT64_MAX` reserved invalid,
- bounded one-level sorting-group IDs `1..255`, with ID `0` meaning canonical ungrouped state,
- one top-level group anchor tuple `(layer, order, stableOrder)` shared by every member of a group,
- child-local `(layer, order, stableOrder)` ordering only after the group has been placed as one top-level unit,
- deterministic exact ties through original caller `sourceIndex`,
- explicit rejection of malformed/inconsistent group anchors,
- no texture/material/sampler/blend/GPU handle or allocation-address participation in the comparator.

Resolution uses O(n) validation, one in-place O(n log n) `std::sort`, then O(n) mask-phase validation. Group-anchor validation uses a fixed 256-entry table; the production renderer reuses capacity-managed scratch across frames.

### Bounded mask state machine

Finite states are:

```text
none
write(mask_id)
test_inside(mask_id)
test_outside(mask_id)
```

Mask IDs are `1..255`; ID `0` is reserved for `none`.

One mask phase is active at a time in resolved painter order:

```text
write(M) [write(M)...] -> tester(M) [tester(M)...]
```

- multiple same-ID writers may union coverage before the first tester,
- a writer after a tester in the same phase fails,
- a different writer closes the previous mask phase,
- a closed mask ID may not re-enter later,
- a tester without its currently active writer fails,
- unmasked draws do not change mask-phase state.

This prevents the CPU semantic contract from relying on stale backend stencil contents after another identity has overwritten pixels.

Mask writer coverage is frozen as:

```text
effective_alpha = sampledStraight.a * tint.a * opacity
covered         = effective_alpha >= 0.5
```

The writer uses normal SR2 geometry and SR3 atlas-safe sampling, writes no color, and changes only derived mask coverage.

### Production SDL GPU path

PR #133 currently implements:

- target-format-aware SR3 appearance preserved for masked testers,
- two persistent nearest/linear samplers,
- four color-only unmasked blend pipelines,
- four stencil-target-compatible unmasked blend pipelines for `none` Sprites inside a masked pass,
- four `test_inside` blend pipelines,
- four `test_outside` blend pipelines,
- one mask writer pipeline,
- 17 finite persistent Sprite presentation pipelines total,
- D24S8 then D32S8 stencil-target format probing,
- dynamic 8-bit stencil reference from semantic mask ID,
- writer `ALWAYS + REPLACE`, inside `EQUAL + KEEP`, outside `NOT_EQUAL + KEEP`,
- wholly unmasked submissions use a color-only pass and create/attach/clear no mask target,
- masked submissions clear stencil to zero and select target-compatible `none` pipelines without stencil tests,
- reusable size-matched mask target,
- reusable Sprite vertex and semantic-order scratch capacity,
- no global resource sorting and no ordinary-frame explicit GPU readback/fence wait.

SR7 still owns broad compatibility batching/culling. SR4 submits semantic order correctly first and optimizes only persistent/reusable finite state.

### Tests and completion gate

Committed backend-independent tests cover:

- signed layer/order/stable ordering,
- exact caller-order tie preservation,
- sorting-group atomicity and local child order,
- deterministic same-anchor group collision handling,
- inconsistent/malformed group states,
- writer/inside/outside phase success,
- tester-without-writer, writer-after-tester, phase-reentry and invalid-mask failures.

Committed opt-in real-GPU fixture:

```text
SpriteOrderMaskGpuSmokeTests.Sr4PainterGroupsAndMasksMatchFrozenContract
```

The fixture intentionally verifies:

- semantic painter order from reversed caller input,
- sorting-group semantics rather than caller/resource order,
- unmasked-only presentation does not create a mask target,
- a transparent `none` Sprite remains valid between a writer and tester in a masked pass without changing stencil state,
- inside-mask captured coverage,
- outside-mask inverse captured coverage,
- persistent sampler/pipeline/mask-target/vertex-capacity reuse,
- repeated ordinary rendering adds no explicit readback/fence waits.

**Do not claim SR4 complete or merge #133 until this real Windows presentation-GPU fixture has actually passed and the result is recorded.**

## #130 / SR3 — complete via PR #131

SR3 adds production Sprite color, alpha, blend and sampling semantics on top of SR0 selection and SR2 exact quad geometry.

### Backend-independent contract

Implemented:

- `SpriteAppearance2D` with linear tint RGBA, opacity, `inherit_asset|nearest|linear` sampling and `normal|additive|multiply|screen` blending,
- strict finite `[0,1]` validation for tint/opacity,
- canonical source alpha remains straight,
- page `srgb|linear` resolves to finite sampled texture encoding identity,
- atlas-safe texel-center sample bounds remain separate from SR2 canonical pixel-edge UV truth,
- `SpritePresentation2D` transactionally combines exact SR2 `SpriteDrawQuad` geometry with resolved SR3 appearance,
- fixed-size caller-owned O(1) extraction with no required heap allocation, semantic lookup, filesystem/TOML/image decode, GPU initialization or canonical mutation,
- CPU conformance oracles freeze the exact straight-alpha -> premultiplied fragment and all four blend equations.

Frozen fragment boundary:

```text
A = sampledStraight.a * tint.a * opacity
P = sampledLinear.rgb * tint.rgb * A
fragment = (P, A)
```

Frozen premultiplied blend factors:

```text
normal   : src=ONE,       dst=ONE_MINUS_SRC_ALPHA
additive : src=ONE,       dst=ONE
multiply : src=DST_COLOR, dst=ONE_MINUS_SRC_ALPHA
screen   : src=ONE,       dst=ONE_MINUS_SRC_COLOR

alpha for all modes:
src=ONE, dst=ONE_MINUS_SRC_ALPHA, op=ADD
```

### Production SDL GPU path

Implemented:

- sRGB/linear-tagged Sprite texture creation with sampled-format support validation,
- SR3 renderer overloads consuming `SpritePresentationRenderData`,
- exact SR2 quad positions/UVs uploaded through reusable capacity-managed Sprite vertex resources,
- fragment UV clamp to SR3 texel-center bounds before sampling,
- tint/opacity plus straight -> premultiplied conversion in the built-in fragment path,
- persistent nearest/linear samplers and built-in blend pipelines,
- no per-Sprite/per-frame sampler/pipeline/shader creation,
- legacy Sprite/particle resources remain separate,
- ordinary presentation performs no explicit GPU readback or fence wait; explicit capture/conformance synchronizes only where observation requires it.

### Validation evidence

The SR3 blocking real-GPU gate passed on **2026-08-12** from `D:\Trace2D-pr131` with `TRACE2D_RUN_GPU_SMOKE=1`:

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

No renderer/driver string was captured; do not invent one.

## External-reference decisions retained by SR4

Primary external authority for the active backend is current official SDL3 GPU documentation.

**ADOPT**

- explicit stencil compare/write masks,
- dynamic stencil reference per masked draw,
- `REPLACE` writer and `EQUAL` / `NOT_EQUAL` tester comparisons,
- runtime query for stencil-capable depth/stencil target format,
- persistent target-format-aware graphics pipelines,
- explicit color-write disable for mask writer,
- existing SR3 sRGB/linear sampled texture and premultiplied blend contract.

**ADAPT**

- SDL stencil reference is treated as derived mapping of a bounded semantic mask ID, never canonical asset state,
- one finite mask phase at a time is enforced so old stencil identities cannot be relied on after another writer,
- color-only and stencil-target-compatible unmasked pipeline sets are separated so target compatibility is explicit without taxing wholly unmasked frames,
- reusable scratch/target capacity is owned by the renderer rather than allocated per Sprite.

**REJECT**

- global texture/material/pipeline sorting,
- pointer/allocation/GPU handle tie-breakers,
- arbitrary recursive sorting-group inference in the renderer,
- programmable mask graphs/stencil property bags in SR4,
- per-frame/per-Sprite persistent resource creation,
- ordinary-frame GPU readback/fence waits.

**DEFER**

- recursive scene-group hierarchy to #71,
- Material2D/custom programmable render state to #89,
- SR5 9-slice/tiled geometry,
- SR7 broad batching/culling/resource-run optimization.

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
      -> #132 SR4 painter order/sorting groups/masking      [active via draft #133]
      -> SR5 9-slice/tiled/repeated Sprite primitives       [next after SR4 merge only]
      -> SR6..SR8 renderer
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

While #132 / PR #133 is active:

1. continue only SR4 implementation, validation, review fixes, real-GPU evidence, and merge work,
2. do not create SR5 or begin later Sprite stages,
3. do not claim backend-conformant masking from hosted compile/CPU tests alone,
4. once hosted validation is green, run the committed real Windows presentation-GPU SR4 fixture,
5. record the exact observed result without inventing driver/hardware data,
6. merge #133 and confirm #132 closed only after every SR4 gate is satisfied,
7. stop that continuation; create exactly one SR5 child on the next `@GitHub Trace2D 다음 진행해줘`.
