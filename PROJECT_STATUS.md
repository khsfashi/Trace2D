# Trace2D Project Status

Last repository-state update: **2026-08-11**

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
- #127 / SR2 trim/pivot/atlas/rotated-storage geometry and UV derivation — PR #128 / squash `a42e65c8a7953d38ad2d82894332c1f39da288f1`.

**Active core program: #59 Complete Sprite program.**  
**Active Sprite child: #130 / SR3 — color/alpha/blend/sampling semantics.**  
**Active implementation PR: draft PR #131 on `agent/sprite-sr3-color-sampling`.**  
**Exact current action: finish SR3 validation only; do not start SR4.**  
**Exact next child after #130 / PR #131 merges green: SR4 — painter order, sorting groups and Sprite masking.**

Do not begin SR4, #103, or later fixed-order work while #130 / PR #131 is open.

## #59 Sprite program — active

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
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [active] -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Only one child is active at a time.

## Completed Sprite foundation

### #119 / S0 — complete

S0 froze the one-way authority chain:

```text
canonical authored Sprite truth
 + authoritative typed runtime state
 -> backend-independent extraction
 -> derived presentation
 -> backend resources
```

Durable invariants remain enforced by `docs/contracts/sprite-s0.json` and `scripts/test_sprite_s0_contract.py`: canonical Sprite/runtime truth requires no renderer initialization; source pixel metadata is canonical; normalized UV/GPU/batch state is derived; trim/packed rotation are storage semantics; exact-frame presentation uses authoritative current state; semantic painter order cannot be globally reordered for batching; explicit tooling/report/capture work stays outside ordinary hot paths.

### #121 / S1 — complete

S1 implemented deterministic versioned `.sprite.toml` canonical CPU truth with normalized project-relative identity, ordered pages/regions, exact source/trim/packed pixel metadata, exact reduced rational pivot, `none`/`cw90` packed storage rotation, explicit sRGB/linear color-space intent, canonical straight alpha, nearest/linear sampling intent, strict structured diagnostics, deterministic serialization, immutable cache reuse, decoded CPU texture-dimension validation and no renderer/GPU dependency.

### #123 / SR0 — complete

SR0 implemented setup-time semantic resolution followed by O(1), allocation-free `ResolvedSpriteRegion` / `SpriteRenderContractData` extraction. CPU resource keys preserve canonical page identity/intent only; no SDL/GPU handle or normalized UV enters canonical/render-contract state. Material, sampler, blend, mask and primitive compatibility stay finite and typed.

### #125 / SR1 — complete

SR1 reuses `scene::Transform2D`, adds typed Sprite flip/history state, and implements exact authoritative-current vs interactive interpolation semantics. Logical Sprite geometry derives untrimmed source-space offsets and exact rational pivot through one Y-down source -> Y-up world boundary, then PPU, flip, scale, CCW rotation and translation. The normal path is fixed-size O(1) with at most one sin/cos pair per quad.

### #127 / SR2 — complete via PR #128

SR2 extends the same source-point transform context to exact trimmed visible geometry and canonical packed-page UVs:

```text
ResolvedSpriteRegion
 + SpritePose2D
 + pixels_per_unit
 -> SpriteDrawQuad positions + canonical pixel-edge UVs
```

`packed_rect` affects storage/UV only, never logical placement. UVs use atlas top-left origin, +u right, +v down and pixel edges with no half-texel rewrite. `cw90` is represented only by the fixed UV corner permutation. Corrupted trim/page/packed/rotation metadata is rejected structurally. SR2 merged green via PR #128 / squash `a42e65c8a7953d38ad2d82894332c1f39da288f1`.

## #130 / SR3 — active via draft PR #131

SR3 is now implemented on branch `agent/sprite-sr3-color-sampling` and remains **draft / not complete** until the required real Windows presentation-GPU conformance run passes.

### Backend-independent contract

Implemented:

- `SpriteAppearance2D` with linear tint RGBA, opacity, `inherit_asset|nearest|linear` sampling and `normal|additive|multiply|screen` blending,
- strict finite `[0,1]` validation for tint/opacity; malformed authoritative state fails instead of being clamped,
- canonical source alpha remains explicitly straight,
- page `srgb|linear` resolves to finite sampled texture encoding identity,
- atlas-safe texel-center bounds are derived separately from SR2 canonical pixel-edge UV truth,
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

Implemented in the production renderer rather than stopping at enums:

- `CreateSpriteTextureRgba8` creates/tag-validates linear UNORM vs sRGB sampled texture representation from canonical page intent,
- backend checks required sampled format support before creation,
- SR3 renderer overloads consume `SpritePresentationRenderData`, preserving caller order exactly,
- exact SR2 quad world positions and canonical UVs are uploaded through a reusable capacity-managed Sprite vertex buffer,
- fragment sampling clamps interpolated UVs to SR3 texel-center region bounds before lookup,
- tint/opacity and straight -> premultiplied conversion occur in the built-in fragment path,
- two persistent sampler objects cover nearest/linear,
- four persistent target-format-aware graphics pipelines cover the frozen blend modes,
- no per-Sprite/per-frame sampler/pipeline/shader creation,
- legacy Sprite/particle sampler/pipeline resources remain separate so SR3 cannot silently change the existing particle path,
- ordinary presentation does not perform explicit GPU readback or fence waits; capture/conformance retains explicit synchronization only where observation requires it.

Renderer metrics now expose SR3 draw counts, sampler/pipeline creation counts, retained Sprite vertex capacity, and explicit readback/fence-wait counts so reuse/no-sync behavior is directly testable rather than inferred.

### Tests / validation state

Committed deterministic tests cover appearance validation/resolution, exact sample bounds including one-pixel and `cw90`, straight-alpha preservation, premultiplied fragment math, all four blend equations, O(1) repeated extraction, and the transactional SR2 -> SR3 presentation seam.

Committed opt-in real-GPU test:

```text
SpriteGpuSmokeTests.Sr3ColorSamplingBlendAndCachesMatchFrozenContract
```

It is gated by `TRACE2D_RUN_GPU_SMOKE=1` and validates on a real presentation GPU:

- nearest and linear sampling,
- linear-filter atlas edge isolation from neighboring texels,
- sRGB source decode exactly once versus linear page behavior,
- normal/additive/multiply/screen captured output against the CPU oracle,
- tint + opacity premultiplication,
- persistent 2-sampler / 4-pipeline cache reuse,
- retained vertex capacity reuse,
- ordinary-frame explicit readback/fence-wait counts remain zero.

Hosted CI is the compile/unit/repository gate, not proof of the real presentation-GPU acceptance requirement. Keep PR #131 draft until both hosted CI and the explicit Windows GPU smoke evidence are green.

### Required local Windows GPU gate

From a clean checkout of PR #131 / `agent/sprite-sr3-color-sampling`:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel

$env:TRACE2D_RUN_GPU_SMOKE = "1"
ctest --preset windows-debug -R "SpriteGpuSmokeTests.Sr3ColorSamplingBlendAndCachesMatchFrozenContract" --output-on-failure
```

Expected result: exactly the SR3 test passes on a machine with a presentation GPU. Record the exact command output/driver evidence in PR #131 before marking ready.

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
      -> #130 SR3 color/alpha/blend/sampling                [active via draft #131; real-GPU gate pending]
      -> SR4..SR8 renderer
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

## Completed foundation sequence

1. #40 texture asset cache/import — PR #45
2. #42 text/basic UI — PR #55
3. #43 semantic UI tree/Agent interaction — PR #56
4. #39 MCP transport — PR #58
5. #41 reproducible renderer workloads — PR #63
6. #47 deterministic particle frame/random contracts — PR #64
7. #48 rich CPU particle reference — PR #65
8. #49 text-authored effects / `ParticleEmitter2D` — PR #66
9. #50 Agent particle verification — PR #83
10. #51 particle cost analysis/backend/compiler — PR #84
11. #52 explicit GPU particle runtime — PR #95
12. #53 CPU/GPU conformance/workloads/guidance — PR #114
13. #97 WorkSpec — PR #115
14. #98 WorkResult — PR #116
15. #99 Workspace — PR #117
16. #102 Benchmark B0 — PR #118
17. #119 Sprite S0 — PR #120
18. #121 Sprite S1 — PR #122
19. #123 Sprite SR0 — PR #124
20. #125 Sprite SR1 — PR #126
21. #127 Sprite SR2 — PR #128

Production architecture freeze #85 remains complete via PR #94.

## Continuation rule

#130 / SR3 is the only active Sprite child and PR #131 is the only active SR3 implementation PR.

Routine continuation while PR #131 remains open must:

1. recover #130 / PR #131 as the first incomplete work,
2. inspect the latest PR head and CI/review state,
3. fix only SR3 failures or acceptance gaps,
4. if hosted CI is green but no real Windows GPU evidence exists, stop at that genuine hardware gate and report the exact smoke command above,
5. never create or implement SR4 while #130 / PR #131 remains open.

After the exact SR3 real-GPU test and all required hosted checks pass:

1. attach exact evidence to PR #131,
2. mark PR #131 ready and merge only when the repository gates permit it,
3. close/confirm #130 through the merged PR,
4. update this file to mark SR3 complete,
5. create exactly one SR4 child issue,
6. implement SR4 only on a later continuation turn.
