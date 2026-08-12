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

Completed Sprite renderer stages:

- #119 / S0 architecture and authority — PR #120,
- #121 / S1 canonical SpriteAsset/import — PR #122,
- #123 / SR0 renderer contract — PR #124,
- #125 / SR1 transform/history — PR #126,
- #127 / SR2 trim/pivot/atlas/rotated storage — PR #128,
- #130 / SR3 color/alpha/blend/sampling — PR #131,
- #132 / SR4 painter order/sorting groups/masking — PR #133,
- #134 / SR5 9-slice/tiled primitives — PR #135,
- #136 / SR6 pixel-perfect runtime presentation — PR #137 / squash `3fd6e5a439a1e327bd89797f5d5a5a9dae69dace`,
- #138 / SR7 production batching/resource reuse/hot-path metrics — PR #139 / squash `9adc9f0e1aab714392b08d068c3ee9ebbad46dbb`; final hosted checks green and owner real-GPU fixture passed 2026-08-12.

Trusted owner real-GPU automation is also complete:

- #140 / PR #141 / squash `4cfad13e0d31ed015a00c0860f525974bcdc2743`,
- `.github/workflows/gpu-gate.yml` runs only trusted `main` / `agent/**` pushes on the owner Windows self-hosted presentation-GPU runner,
- public fork PR code is not routed to the self-hosted machine,
- `scripts/gpu_gate.ps1` records commit/environment/test/checksum evidence and rejects skipped real-GPU fixtures.

**Active core program: #59 Complete Sprite program.**  
**Only active Sprite child: #142 / draft PR #143 — SR8 renderer conformance, capture QA, and reproducible workloads.**  
**SA0 must not start before #143 merges green and #142 closes.**

## #59 Sprite program

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
SR8 contract: [`docs/SPRITE_RENDERER_CONFORMANCE_SR8.md`](docs/SPRITE_RENDERER_CONFORMANCE_SR8.md).

Fixed internal order:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete]
 -> SR4 [complete] -> SR5 [complete] -> SR6 [complete] -> SR7 [complete]
 -> SR8 [active #142/#143]
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Exactly one Sprite child is active at a time.

## #142 / SR8 — renderer conformance / capture QA / reproducible workloads — active via draft PR #143

SR8 adds validation/evidence only. It does not introduce new normal-frame Sprite semantics.

### Backend-independent conformance

`SpriteRendererConformanceTests` currently composes the completed renderer contracts and freezes:

- exact source/trim/pivot geometry,
- `cw90` packed-storage UV permutation without logical-placement mutation,
- authoritative-current vs interpolated SR6 presentation selection,
- a versioned fixed structural workload using 1,024 caller-owned `SpriteBatchItem2D` entries.

Frozen workload v1 raw facts:

```text
submitted_sprites  1024
visible_sprites     768
culled_sprites      256
visible_quads       960
contiguous_runs       7
```

These are deterministic structure metrics, not a weighted score and not a timing budget.

### Real presentation-GPU conformance

Existing SR3-SR7 GPU fixtures remain authoritative for their stage contracts. SR8 adds the missing SR2 presentation fixture:

```text
SpriteRendererGpuConformanceTests.Sr8TrimPivotCw90PresentationMatchesCanonicalGeometry
```

It first validates canonical geometry/UV facts, then uses a real 1x2 clockwise-packed red/green texture and requires the captured logical 2x1 Sprite to reconstruct red-left / green-right orientation.

All required Sprite GPU suites are selected through #140/#141's trusted owner GPU gate:

```text
SpriteGpuSmokeTests
SpriteOrderMaskGpuSmokeTests
SpritePrimitiveGpuSmokeTests
SpritePixelPerfectGpuSmokeTests
SpriteBatchGpuSmokeTests
SpriteRendererGpuConformanceTests
```

No selected real-GPU test may be skipped.

### Evidence / performance boundary

`scripts/sprite_renderer_final_gate.ps1` delegates hardware execution to `scripts/gpu_gate.ps1`, runs SR8 CPU conformance, parses the measured structural workload marker, records deterministic `trace2d_renderer_workload --list` JSON, hashes evidence, and writes `trace2d.sprite-renderer-final-gate.v1` bound to the exact Git commit.

Hard rules:

- captures are derived presentation evidence, never canonical Sprite/gameplay truth,
- exact CPU facts stay exact; GPU floating color paths use fixture-owned bounded per-channel tolerances,
- no automatic golden-image update path,
- no shared-runner wall-clock correctness threshold,
- no JSON/string/filesystem/reporting work in ordinary Sprite frames,
- no SR8-only per-frame heap container,
- no new ordinary-frame explicit GPU readback/fence wait,
- generic profiler/timing remains #91 and broader backend/release GPU qualification remains #92.

### Blocking completion gates

PR #143 must remain draft/not merged until the **same final head** satisfies all of:

1. required hosted checks green,
2. trusted owner Sprite GPU gate green with all required suites and no skips,
3. `scripts/sprite_renderer_final_gate.ps1` produces exact-head evidence,
4. exact evidence is recorded on PR #143,
5. docs/status match the final implementation.

Do not invent GPU or final-gate evidence.

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
      -> S0/S1/SR0..SR7                                    [complete]
      -> #142 SR8 renderer conformance/workloads            [active via draft #143]
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

SR8 is the only active Sprite child. The current continuation must:

1. keep #142 / draft PR #143 scoped only to SR8 validation/evidence,
2. fix only genuine SR0-SR7 regressions exposed by the conformance matrix and lock them narrowly,
3. require hosted checks on the final #143 head,
4. require trusted owner real-GPU evidence for all required Sprite GPU suites on that same head with no skips,
5. require exact-head `scripts/sprite_renderer_final_gate.ps1` evidence,
6. keep #143 draft/not merged until every gate is satisfied,
7. after all gates pass, record exact evidence, mark ready/merge #143, confirm #142 closed, and stop,
8. not create SA0 in that completion continuation.

Only the **following** `@GitHub Trace2D 다음 진행해줘` continuation may create exactly one SA0 child.