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
- #130 / SR3 color/alpha/blend/sampling — completion PR #131; real Windows presentation-GPU gate passed 2026-08-12.

**Active core program: #59 Complete Sprite program.**  
**SR3 acceptance: satisfied; PR #131 is the completion vehicle and must merge before any SR4 work begins.**  
**Exact next child after PR #131 merges: SR4 — painter order, sorting groups and Sprite masking.**  
**Continuation rule: do not create or implement SR4 in the same continuation that finalizes PR #131. Create exactly one SR4 child only on the next continuation.**

## #59 Sprite program

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
Frozen architecture: [`docs/SPRITE_ARCHITECTURE.md`](docs/SPRITE_ARCHITECTURE.md).  
Canonical S1 format: [`docs/SPRITE_ASSET_FORMAT.md`](docs/SPRITE_ASSET_FORMAT.md).  
SR0 render seam: [`docs/SPRITE_RENDER_CONTRACT.md`](docs/SPRITE_RENDER_CONTRACT.md).  
SR1 transform/presentation seam: [`docs/SPRITE_TRANSFORM_PRESENTATION.md`](docs/SPRITE_TRANSFORM_PRESENTATION.md).  
SR2 atlas/trim/UV seam: [`docs/SPRITE_ATLAS_GEOMETRY.md`](docs/SPRITE_ATLAS_GEOMETRY.md).  
SR3 color/alpha/blend/sampling seam: [`docs/SPRITE_COLOR_SAMPLING.md`](docs/SPRITE_COLOR_SAMPLING.md).

Fixed internal order:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete via #131] -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Only one child is active at a time. PR #131 must be merged/closed before the next child exists.

## #130 / SR3 — acceptance complete via PR #131

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
- SR3 renderer overloads consuming `SpritePresentationRenderData` while preserving caller order,
- exact SR2 quad positions/UVs uploaded through reusable capacity-managed Sprite vertex resources,
- fragment UV clamp to SR3 texel-center bounds before sampling,
- tint/opacity plus straight -> premultiplied conversion in the built-in fragment path,
- two persistent samplers for nearest/linear,
- four persistent target-format-aware graphics pipelines for normal/additive/multiply/screen,
- no per-Sprite/per-frame sampler/pipeline/shader creation,
- legacy Sprite/particle resources remain separate,
- ordinary presentation performs no explicit GPU readback or fence wait; explicit capture/conformance synchronizes only where observation requires it.

Renderer metrics expose SR3 draw counts, sampler/pipeline creation counts, retained Sprite vertex capacity, and explicit readback/fence-wait counts.

### Validation evidence

Hosted validation for PR #131 code/status heads is green for Windows MSVC configure/build/ctest, clean-clone quick-start, repository/release/content/Sprite contract audits, Benchmark B0 qualification, and Godot oracle/qualification workflows.

Blocking real-GPU gate passed on **2026-08-12** from `D:\Trace2D-pr131` with `TRACE2D_RUN_GPU_SMOKE=1`:

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

The opt-in test proves nearest/linear sampling, atlas-edge bleed protection, sRGB-vs-linear behavior, all four blend modes, tint/opacity premultiplication, persistent sampler/pipeline/capacity reuse, and zero ordinary-frame explicit readback/fence waits. No renderer/driver string was captured; do not invent one.

## External-reference decisions retained by SR3

Primary references are current official SDL3 GPU documentation.

**ADOPT**

- persistent SDL GPU samplers,
- `R8G8B8A8_UNORM_SRGB` for canonical sRGB pages and `R8G8B8A8_UNORM` for linear pages,
- sampled-format support query before Sprite texture creation,
- fragment uniform slot 0 for tint/opacity/sample bounds,
- fixed blend factors `ONE`, `DST_COLOR`, `ONE_MINUS_SRC_ALPHA`, `ONE_MINUS_SRC_COLOR`.

**ADAPT**

- separate SR3 GPU cache while preserving legacy/particle resources,
- reusable six-vertex-per-Sprite upload path now; broad compatibility batching remains SR7.

**REJECT**

- manual second sRGB decode,
- per-frame/per-Sprite sampler/pipeline/shader creation,
- arbitrary blend-factor property bags,
- rewriting SR2 canonical UVs for filtering safety,
- ordinary-frame GPU readback/fence waits.

**DEFER**

- mip/anisotropy,
- Material2D/custom programmable blends,
- SR4 painter order/groups/masks,
- SR7 broad batching/culling policy.

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
      -> #130 SR3 color/alpha/blend/sampling                [acceptance complete via #131]
      -> SR4 painter order/sorting groups/masking           [next child; create next continuation only]
      -> SR5..SR8 renderer
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

PR #131 is the SR3 completion vehicle. While it remains open, continuation may only finalize its evidence/status/merge state and must not create SR4.

After PR #131 merges and #130 is confirmed closed:

1. stop this continuation without implementing SR4,
2. on the next `@GitHub Trace2D 다음 진행해줘`, create exactly one SR4 child issue under #59,
3. freeze SR4 acceptance from the existing Sprite contract before implementation,
4. implement only SR4 until its own PR merges green.
