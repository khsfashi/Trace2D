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

Completed Sprite predecessor:

- #119 / S0 Sprite architecture and authority contract — PR #120 / squash `00dc587153bc4b0d6f6ac350d5491eec481585f0`.

**Active core program: #59 Complete Sprite program.**  
**Active Sprite child: #121 / S1 — canonical SpriteAsset schema and deterministic import representation.**  
**Active implementation PR: #122 (`agent/sprite-s1-canonical-assets`).**  
**Exact next child after #121/#122 merges green: SR0 — renderer contract and asset/render separation.**

Do not begin SR0, #103, or later fixed-order work while #121/#122 is open.

## #59 Sprite program — active

Umbrella: #59. Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
Frozen architecture: [`docs/SPRITE_ARCHITECTURE.md`](docs/SPRITE_ARCHITECTURE.md).  
Concrete S1 format: [`docs/SPRITE_ASSET_FORMAT.md`](docs/SPRITE_ASSET_FORMAT.md).

Fixed internal order:

```text
S0 [complete] -> S1 [active]
 -> SR0 -> SR1 -> SR2 -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Only one child is active at a time.

### #119 / S0 — complete

S0 froze the authority direction:

```text
external source/generation
 -> deterministic import
 -> canonical SpriteAsset CPU truth
 -> authoritative SpriteRenderer2D / SpriteAnimator2D semantics
 -> backend-independent extraction
 -> derived presentation state
 -> renderer/backend resources
```

Durable S0 invariants:

1. canonical Sprite assets and authoritative runtime/animation state remain usable without renderer initialization;
2. source-space pixel metadata is canonical; normalized UV/GPU/batch state is derived;
3. source space is top-left origin, +x right, +y down, integer half-open rectangles;
4. trim and packed rotation are storage semantics only;
5. `current_fixed` is authoritative and `previous_fixed` is presentation history;
6. exact-frame capture defaults to authoritative current state;
7. future #71/#86/#88/#89 attach through typed world/resource/view/material seams without replacing Sprite authored semantics;
8. semantic painter order cannot be globally reordered for batching;
9. import/generation/repair/full inspection/capture/reporting remain explicit work outside ordinary frame hot paths;
10. deterministic -> multimodal -> human review authority reuses #97-#99.

Machine drift prevention remains in `docs/contracts/sprite-s0.json` and `scripts/test_sprite_s0_contract.py`.

### #121 / S1 — active

S1 implements the first concrete canonical authored/imported Sprite representation.

Current canonical file contract:

```toml
schema = "trace2d.sprite"
version = 1
sampling = "nearest"

[[pages]]
id = "main"
texture = "textures/player.png"
size = [256, 128]
color_space = "srgb"
alpha_mode = "straight"

[[regions]]
id = "idle_0"
page = "main"
source_size = [32, 32]
trim_offset = [2, 1]
trim_size = [28, 30]
packed_rect = [0, 0, 28, 30]
pivot = [16, 28, 1]
packed_rotation = "none"
```

Frozen S1 implementation decisions:

- canonical asset identity is normalized project-relative `.sprite.toml`,
- page texture references are normalized project-relative CPU references,
- page/region order is explicit and deterministic,
- pivot is an exact reduced rational `[x_numerator, y_numerator, denominator]`, denominator > 0,
- finite representable pivots may be outside source bounds and are not clamped,
- packed storage rotation supports exactly `none` and `cw90`,
- `cw90` means stored packed pixels are the trimmed logical content rotated 90 degrees clockwise,
- v1 color space is `srgb` or `linear`, alpha mode is `straight`, sampling is `nearest` or `linear`,
- declared atlas-page dimensions are checked against the existing decoded CPU `TextureAssetData`,
- canonical assets contain no SDL/GPU handles, normalized UVs, upload offsets, package compression/mip state, or renderer residency,
- strict parser diagnostics reject unknown fields, duplicate semantic IDs, malformed bounds/references/enums and unsupported future versions,
- canonical serializer uses fixed field ordering plus authored page/region ordering,
- `SpriteAssetCache` caches immutable successful imports and exposes structural cache metrics,
- TOML/path/file/decode work occurs only during explicit load/import/serialization, never as a future draw-loop requirement.

S1 implementation files:

- `engine/assets/include/trace2d/assets/SpriteAssets.hpp`,
- `engine/assets/src/SpriteAssets.cpp`,
- `tests/assets/SpriteAssetsTests.cpp`,
- `docs/SPRITE_ASSET_FORMAT.md`.

S1 must merge green before SR0 starts.

## Owner-fixed core execution order

Routine `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, or equivalent follows the first incomplete/unblocked item below.

```text
AI-operated foundation
 -> #97 machine-readable intent / Definition of Done         [complete]
 -> #98 unified verify / diagnose / repair / WorkResult      [complete]
 -> #99 result-review Workspace / feedback loop              [complete]
 -> #102 Benchmark B0 matched harness + current tasks        [complete]

Content production
 -> #59 complete Sprite program                              [active]
      -> #119 S0 Sprite architecture/authority contract      [complete via #120]
      -> #121 S1 canonical Sprite asset/import               [active via #122]
      -> SR0..SR8 renderer
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

### #97 WorkSpec / capability

A capability is not inferred from a symbol merely existing. Repository evidence establishes availability/testing/support; live CI/hardware/license/human facts remain external truth.

### #98 WorkResult

```text
WorkSpec acceptance
 -> verification record
 -> structured failure + reproduction context
 -> Agent/user repair
 -> new revision
 -> deterministic re-verification
 -> subjective review only where required
```

Agent self-report is never independent truth.

### #99 Workspace

Workspace is derived from WorkSpec/WorkResult/optional Agent inspection. It does not own project/world truth, silently mutate engine state, or promote machine-owned failures into human review.

### #102 Benchmark

The accepted B0 cohort and raw evidence remain preserved under `benchmarks/b0/`. B0 proved the matched methodology/evidence loop; its nine preregistered scored attempts exceeded the frozen input-token budget, so it is not evidence of broad engine superiority.

### Sprite authority

```text
canonical authored Sprite metadata
 + authoritative typed runtime/animation state
        -> derived presentation
        -> backend renderer resources
```

GPU resources, pixels, Agent snapshots and review artifacts never become canonical Sprite/gameplay truth.

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

Production architecture freeze #85 remains complete via PR #94.

## Continuation rule

While PR #122 is open, finish its S1 acceptance, tests, docs and CI only. Do not create SR0 implementation in parallel.

After PR #122 merges green:

1. close/confirm #121 through the PR,
2. update this file to mark S1 complete,
3. create exactly one SR0 child issue,
4. implement SR0 only.
