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

Completed Sprite foundation:

- #119 / S0 Sprite architecture and authority contract — PR #120 / squash `00dc587153bc4b0d6f6ac350d5491eec481585f0`,
- #121 / S1 canonical SpriteAsset/import representation — PR #122 / squash `27250bff8afd40f55edf2bfbed9be8b143f1ea1d`.

**Active core program: #59 Complete Sprite program.**  
**Active Sprite child: #123 / SR0 — renderer contract and canonical asset/render separation.**  
**Active implementation PR: #124 (`agent/sprite-sr0-render-contract`).**  
**Exact next child after #123/#124 merges green: SR1 — complete transform/geometry semantics plus fixed-step presentation history.**

Do not begin SR1, #103, or later fixed-order work while #123/#124 is open.

## #59 Sprite program — active

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
Frozen architecture: [`docs/SPRITE_ARCHITECTURE.md`](docs/SPRITE_ARCHITECTURE.md).  
Canonical S1 format: [`docs/SPRITE_ASSET_FORMAT.md`](docs/SPRITE_ASSET_FORMAT.md).  
SR0 render seam: [`docs/SPRITE_RENDER_CONTRACT.md`](docs/SPRITE_RENDER_CONTRACT.md).

Fixed internal order:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [active] -> SR1 -> SR2 -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
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

Durable S0 invariants remain enforced by `docs/contracts/sprite-s0.json` and `scripts/test_sprite_s0_contract.py`:

- canonical Sprite/runtime truth remains usable without renderer initialization,
- source-space pixel metadata is canonical; normalized UV/GPU/batch state is derived,
- trim/packed rotation are storage semantics only,
- `current_fixed` is authoritative and `previous_fixed` is presentation history,
- exact-frame capture defaults to authoritative current state,
- future #71/#86/#88/#89 attach through typed seams without replacing Sprite authored semantics,
- semantic painter order cannot be globally reordered for batching,
- explicit tooling/report/capture work remains outside ordinary frame hot paths.

### #121 / S1 — complete

S1 implemented deterministic canonical `.sprite.toml` CPU truth:

```toml
schema = "trace2d.sprite"
version = 1
sampling = "nearest"
```

with ordered pages/regions, exact source/trim/packed pixel metadata, exact reduced rational pivot, `none`/`cw90` storage rotation, explicit color/alpha/sampling intent, strict diagnostics, deterministic serialization, decoded texture-dimension validation, and immutable cache reuse.

Canonical assets contain no SDL/GPU handles, normalized UVs, upload offsets, package compression/mip state, or renderer residency.

Implementation/reference:

- `engine/assets/include/trace2d/assets/SpriteAssets.hpp`,
- `engine/assets/src/SpriteAssets.cpp`,
- `docs/SPRITE_ASSET_FORMAT.md`,
- `tests/assets/SpriteAssetsTests.cpp`,
- `tests/assets/SpriteAssetValidationTests.cpp`.

### #123 / SR0 — active

SR0 adds the first backend-independent renderer seam without implementing later renderer behavior.

Authority/runtime path:

```text
immutable canonical SpriteAsset
 -> setup-time ResolveSpriteRegionByIndices / ResolveSpriteRegionById
 -> ResolvedSpriteRegion
 -> O(1) ExtractSpriteRenderContract
 -> later SR1/SR2 presentation derivation
 -> renderer/backend resources
```

Current SR0 decisions:

- successful steady-state extraction is O(1),
- semantic ID lookup/string relationship checks happen only during setup resolution,
- steady-state extraction performs no filesystem/TOML/image decode/path normalization/string lookup,
- no required heap allocation or formatted diagnostic construction on success,
- canonical asset ownership remains external; resolved selections are non-owning and trivially copyable,
- `SpritePageResourceKey` uses canonical project-relative texture identity + page size/color/alpha intent only,
- no GPU/SDL handle or backend enum enters the canonical/render-contract boundary,
- built-in compatibility seam is finite: built-in Sprite pipeline, nearest/linear sampler, straight-alpha compatibility, no mask, quad primitive,
- existing backend-independent `OrthographicView` is reused as `SpriteResolvedView`; #88 later supplies Camera2D/Viewport2D resolution,
- invalid manual CPU states/selections fail through allocation-free `SpriteResolveError` + `SpriteResolveField`,
- Assets now precedes Render in CMake and Render publicly depends on Assets; dependency direction is one-way.

Implementation/reference:

- `engine/render/include/trace2d/render/SpriteRenderContract.hpp`,
- `engine/render/src/SpriteRenderContract.cpp`,
- `tests/render/SpriteRenderContractTests.cpp`,
- `docs/SPRITE_RENDER_CONTRACT.md`.

SR0 must merge green before SR1 starts.

## Owner-fixed core execution order

Routine `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, or equivalent follows the first incomplete/unblocked item below.

```text
AI-operated foundation
 -> #97 WorkSpec                                            [complete]
 -> #98 WorkResult verify/diagnose/repair                   [complete]
 -> #99 Workspace/review loop                               [complete]
 -> #102 Benchmark B0                                      [complete]

Content production
 -> #59 complete Sprite program                            [active]
      -> #119 S0 architecture                              [complete via #120]
      -> #121 S1 canonical asset/import                    [complete via #122]
      -> #123 SR0 asset/render contract                    [active via #124]
      -> SR1..SR8 renderer
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

### WorkSpec / WorkResult / Workspace

Repository evidence establishes capability. Agent self-report is never independent truth. WorkResult remains:

```text
WorkSpec acceptance
 -> verification record
 -> structured failure + reproduction context
 -> Agent/user repair
 -> new revision
 -> deterministic re-verification
 -> subjective review only where required
```

Workspace remains derived from WorkSpec/WorkResult/optional Agent inspection and does not own project/world truth.

### Benchmark

The accepted B0 cohort/raw evidence remains under `benchmarks/b0/`. B0 proved the matched methodology/evidence loop; its frozen scored cohort did not establish broad engine superiority.

### Sprite authority

```text
canonical authored Sprite metadata
 + authoritative typed runtime/animation state
        -> resolved/derived presentation
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
18. #121 Sprite S1 — PR #122

Production architecture freeze #85 remains complete via PR #94.

## Continuation rule

While PR #124 is open, finish only #123/SR0 acceptance, tests, docs and CI. Do not create SR1 implementation in parallel.

After PR #124 merges green:

1. close/confirm #123 through the PR,
2. update this file to mark SR0 complete,
3. create exactly one SR1 child issue,
4. implement SR1 only.
