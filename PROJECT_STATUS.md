# Trace2D Project Status

Last repository-state update: **2026-08-11**

This file is the operational handoff for the next contributor or coding agent. Compiling code/tests, live PR/CI/merge state, explicit owner-approved contracts, and exact active issue acceptance outrank stale prose.

## Current state

Trace2D is an **AI-first / AI-operated C++20 2D engine** with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Completed AI-operated foundation:

- #97 machine-readable intent / Definition of Done — complete via PR #115,
- #98 unified verification / diagnosis / repair / WorkResult — complete via PR #116,
- #99 Workspace / human feedback loop — complete via PR #117,
- #102 Benchmark B0 — complete via PR #118, squash merge `13a28d7baf8bd72d9f3233a57b2a048450825bee`.

**Active core program: #59 Complete Sprite program.**  
**Active Sprite child: #119 S0 — production Sprite architecture and authority contracts.**  
**Exact next child after #119 merges: S1 — canonical Sprite asset model and deterministic import representation.**

Do not begin S1, #103, or later fixed-order work while #119 is open.

## #102 Benchmark B0 — complete

The accepted scored cohort is preserved in:

- [`benchmarks/b0/RESULTS.md`](benchmarks/b0/RESULTS.md),
- [`benchmarks/b0/qualification/codex-windows-acl-scored-cohort-accepted-2026-08-11.json`](benchmarks/b0/qualification/codex-windows-acl-scored-cohort-accepted-2026-08-11.json).

Accepted archive identity:

```text
codex-chatgpt-scored-20260811-180458-214dfeb0.zip
SHA-256 0625e084b6704258a537de7005a3f9a427d66147663abc7878d5880b2860ea52
```

The preregistered nine-slot cohort ran with no replacement retry or best-of-N. All nine attempts exceeded the frozen `100000` input-token ceiling, so all authoritative benchmark statuses are `budget_exceeded`. Independent verifier outcomes remain reported separately (`godot.generic` 1/3, `godot.agent` 3/3, `trace2d.agent` 3/3). The result proves the B0 methodology/evidence loop, not broad engine superiority.

## #59 Sprite program — active

Umbrella: #59. Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).

Fixed internal order:

```text
S0 -> S1
 -> SR0 -> SR1 -> SR2 -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Only one child is active at a time.

### #119 / S0 — active

S0 freezes the production Sprite architecture before the canonical asset and renderer implementation grows.

Primary contracts on the S0 branch:

- [`docs/SPRITE_ARCHITECTURE.md`](docs/SPRITE_ARCHITECTURE.md),
- [`docs/contracts/sprite-s0.json`](docs/contracts/sprite-s0.json),
- [`scripts/test_sprite_s0_contract.py`](scripts/test_sprite_s0_contract.py).

S0 freezes:

```text
external source/generation
 -> deterministic import
 -> canonical SpriteAsset CPU truth
 -> authoritative SpriteRenderer2D / SpriteAnimator2D semantics
 -> backend-independent extraction
 -> derived presentation state
 -> renderer/backend resources
```

Hard S0 invariants:

1. canonical Sprite assets and authoritative runtime/animation state remain usable without renderer initialization;
2. exact source-space pixel metadata is canonical; normalized UV/GPU/batch state is derived;
3. source space uses top-left origin, +x right, +y down, integer half-open pixel rectangles, and untrimmed-source pivot coordinates;
4. trim and packed rotation are storage semantics and may not change logical placement/pivot/animation meaning;
5. `current_fixed` is authoritative; `previous_fixed` is presentation history; interactive interpolation never writes back into gameplay truth;
6. exact-frame capture defaults to authoritative current state; sub-frame alpha is explicit evidence;
7. reset/load/teleport/snap synchronize transform history;
8. future #71/#86/#88/#89 attach through typed world/resource/view/material seams without replacing Sprite authored semantics;
9. batching may merge compatible contiguous work but may not globally reorder semantic painter order;
10. generation/import repair/full inspection/capture/report aggregation are explicit tooling work, not normal frame work;
11. deterministic/structured Sprite facts are machine-owned, perceptual facts may use multimodal review, and taste/final creative approval remains human;
12. external formats terminate at canonical import and never become runtime dispatch types.

S0 is a contract PR. It must not opportunistically implement Sprite renderer breadth.

## Owner-fixed core execution order

Routine `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, or equivalent follows the first incomplete/unblocked item below.

```text
AI-operated foundation
 -> #97 machine-readable intent / Definition of Done         [complete via PR #115]
 -> #98 unified verify / diagnose / repair / WorkResult      [complete via PR #116]
 -> #99 result-review Workspace / feedback loop              [complete via PR #117]
 -> #102 Benchmark B0 matched harness + current tasks        [complete via PR #118]

Content production
 -> #59 complete Sprite program                              [active]
      -> #119 S0 Sprite architecture/authority contract      [active]
      -> S1 canonical Sprite asset/import                    [next]
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

Umbrellas/registers #13/#96/#100/#67/#85/#93/#101/#106 do not authorize bypassing the fixed order.

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

```text
frozen task + lane + Agent/model/budget
 -> isolated fresh trial
 -> append-only Agent trajectory
 -> independent verifier
 -> immutable raw record
 -> preregistered repeated cohort
 -> aggregate raw statistics
 -> independent re-verification/replay
```

Capability, infrastructure, budget, verifier correctness, human intervention and implementation outcomes remain separate dimensions.

### #119 Sprite S0

```text
canonical authored Sprite metadata
 + authoritative typed runtime/animation state
        -> derived presentation
        -> backend renderer resources
```

No reverse dependency may make GPU/pixels/tool observations authoritative Sprite truth.

## Completed foundation and particle sequence

1. #40 deterministic texture asset cache/import — PR #45
2. #42 text/basic UI — PR #55
3. #43 semantic UI tree/Agent interaction — PR #56
4. #39 MCP transport — PR #58
5. #41 reproducible renderer workloads — PR #63
6. #47 deterministic particle frame/random contracts — PR #64
7. #48 rich CPU particle reference — PR #65
8. #49 text-authored effects / `ParticleEmitter2D` — PR #66
9. #50 complete Agent particle verification — PR #83
10. #51 particle cost analysis/backend/compiler — PR #84
11. #52 explicit GPU particle runtime — PR #95
12. #53 CPU/GPU conformance/workloads/guidance — PR #114
13. #97 WorkSpec — PR #115
14. #98 WorkResult — PR #116
15. #99 Workspace — PR #117
16. #102 Benchmark B0 — PR #118

Production architecture freeze #85 is complete via PR #94.

## Particle architecture frozen by #47-#53

```text
rich text-authored effect
 -> deterministic CPU semantic oracle
 -> exact structured Agent verification
 -> deterministic structural cost analysis
 -> optional environment-labelled timing
 -> explicit CPU/GPU backend decision
 -> deterministic minimized GPU artifact
 -> persistent GPU compute/instanced presentation
 -> layered CPU/GPU conformance
```

Primary particle contracts remain in `docs/PARTICLES.md`, `docs/PARTICLE_ANALYSIS.md`, `docs/PARTICLE_GPU_RUNTIME.md`, and `docs/PARTICLE_CONFORMANCE.md`.

## Benchmark/content growth

```text
#102 B0 — harness + current-capability matched task [complete]
 -> #59 Complete Sprite program [active]
 -> #103 B1 — Sprite/animation/particle matched tasks
 -> #104 B2 — coherent autonomous top-down combat micro-game
```
