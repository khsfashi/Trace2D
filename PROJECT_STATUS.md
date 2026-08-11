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
- #123 / SR0 renderer contract / canonical asset-render separation — PR #124 / squash `aa30a8e4498fd5edd6df9d2be7bb9a91bcdea5db`.

**Active core program: #59 Complete Sprite program.**  
**Active Sprite child: #125 / SR1 — transform geometry and fixed-step presentation history.**  
**Active implementation PR: #126 (`agent/sprite-sr1-transform-history`).**  
**Exact next child after #125/#126 merges green: SR2 — atlas/trim/pivot/rotated-packing correctness.**

Do not begin SR2, #103, or later fixed-order work while #125/#126 is open.

## #59 Sprite program — active

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
Frozen architecture: [`docs/SPRITE_ARCHITECTURE.md`](docs/SPRITE_ARCHITECTURE.md).  
Canonical S1 format: [`docs/SPRITE_ASSET_FORMAT.md`](docs/SPRITE_ASSET_FORMAT.md).  
SR0 render seam: [`docs/SPRITE_RENDER_CONTRACT.md`](docs/SPRITE_RENDER_CONTRACT.md).  
SR1 transform/presentation seam: [`docs/SPRITE_TRANSFORM_PRESENTATION.md`](docs/SPRITE_TRANSFORM_PRESENTATION.md).

Fixed internal order:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [active] -> SR2 -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Only one child is active at a time.

### #119 / S0 — complete

S0 froze:

```text
canonical authored Sprite truth
 + authoritative typed runtime state
 -> backend-independent extraction
 -> derived presentation
 -> backend resources
```

Durable S0 invariants remain enforced by `docs/contracts/sprite-s0.json` and `scripts/test_sprite_s0_contract.py`:

- canonical Sprite/runtime truth remains usable without renderer initialization,
- source-space pixel metadata is canonical; normalized UV/GPU/batch state is derived,
- source coordinates are top-left / +x right / +y down / half-open integer rectangles,
- trim and packed rotation are storage semantics only,
- `current_fixed` is authoritative; `previous_fixed` is presentation history,
- exact-frame capture defaults to authoritative current state,
- future #71/#86/#88/#89 attach through typed seams without replacing Sprite semantics,
- semantic painter order cannot be globally reordered for batching,
- explicit tooling/report/capture work remains outside ordinary frame hot paths.

### #121 / S1 — complete

S1 implemented deterministic versioned `.sprite.toml` canonical CPU truth with:

- normalized project-relative Sprite/texture identity,
- ordered pages/regions,
- exact source/trim/packed pixel metadata,
- exact reduced rational pivot,
- `none`/`cw90` storage rotation,
- explicit color-space/straight-alpha/sampling intent,
- strict structured diagnostics,
- deterministic serialization and immutable cache reuse,
- decoded CPU texture dimension validation,
- no renderer/GPU dependency.

### #123 / SR0 — complete

SR0 implemented:

```text
immutable canonical SpriteAsset
 -> setup-time region resolution
 -> ResolvedSpriteRegion
 -> O(1) ExtractSpriteRenderContract
 -> later presentation stages
```

Key rules:

- semantic ID lookup/string relationship checks are setup-only,
- successful extraction is O(1) and allocation-free,
- `SpritePageResourceKey` contains CPU identity/intent only,
- canonical/render-contract state contains no SDL/GPU handle or normalized UV,
- finite built-in material/sampler/blend/mask/primitive compatibility seam,
- `OrthographicView` is reused as the #88-ready resolved view seam,
- dependency direction is one-way: Assets/Scene truth -> Render extraction.

### #125 / SR1 — active

SR1 implements authoritative transform/history in Scene and logical geometry derivation in Render.

Authoritative state:

```text
scene::Transform2D
 + flipX / flipY
 -> scene::SpritePose2D
 -> previousFixed / currentFixed
```

Rules implemented by `SPRITE_TRANSFORM_PRESENTATION.md`:

- reuses existing `scene::Transform2D`; no renderer-owned transform model,
- finite position / float radians / non-uniform scale,
- zero and negative scale are valid,
- semantic flip X/Y remains independent from scale sign,
- `SnapSpritePoseHistory` synchronizes previous/current for discontinuities,
- `CommitSpriteFixedPose` advances history only on explicit successful commit,
- invalid commit/snap leaves history unchanged,
- exact-frame presentation copies `currentFixed` exactly,
- interactive alpha must be finite in `[0,1]`; no clamp/extrapolation,
- position/scale interpolate linearly,
- rotation uses shortest signed float-radian arc with deterministic +pi/-pi tie,
- flip X/Y is discrete and always comes from `currentFixed`,
- interpolated presentation is never written back to authority.

Logical geometry:

```text
S1 untrimmed source_size + exact rational pivot
 -> Y-down source offsets
 -> one Y-down -> Y-up derivation boundary
 -> / pixels_per_unit
 -> semantic flip
 -> non-uniform/negative scale
 -> CCW rotation
 -> translation
 -> SpriteLogicalQuad
```

Performance/ownership:

- interpolation O(1), fixed-size output,
- logical quad O(1), one sin/cos pair + four fixed corners,
- double intermediates with checked finite float output,
- no vector/list/string lookup/filesystem/TOML/image decode/GPU work,
- caller-owned outputs are reusable,
- SR1 consumes the pre-resolved SR0 region and does not re-resolve semantic names.

SR1 does not derive normalized UVs or stored trimmed/rotated atlas mapping; SR2 owns that exact handoff.

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
      -> #125 SR1 transform/history/geometry                [active via #126]
      -> SR2..SR8 renderer
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

Production architecture freeze #85 remains complete via PR #94.

## Continuation rule

While PR #126 is open, finish only #125/SR1 acceptance, tests, docs and CI. Do not create SR2 implementation in parallel.

After PR #126 merges green:

1. close/confirm #125 through the PR,
2. update this file to mark SR1 complete,
3. create exactly one SR2 child issue,
4. implement SR2 only.
