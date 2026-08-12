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

Completed Sprite foundation/renderer stages:

- #119 / S0 architecture and authority — PR #120,
- #121 / S1 canonical SpriteAsset/import — PR #122,
- #123 / SR0 renderer contract — PR #124,
- #125 / SR1 transform/history — PR #126,
- #127 / SR2 trim/pivot/atlas/rotated storage — PR #128,
- #130 / SR3 color/alpha/blend/sampling — PR #131,
- #132 / SR4 painter order/sorting groups/masking — PR #133,
- #134 / SR5 9-slice/tiled primitives — PR #135,
- #136 / SR6 pixel-perfect runtime presentation — PR #137 / squash `3fd6e5a439a1e327bd89797f5d5a5a9dae69dace`,
- #138 / SR7 production batching/resource reuse/hot-path metrics — PR #139 / squash `9adc9f0e1aab714392b08d068c3ee9ebbad46dbb`,
- #142 / SR8 renderer conformance/capture QA/reproducible workloads — PR #143 / squash `2108122dad5ac2dcbb964f7ada0e80f7afa21003`.

Completed Sprite animation contract stage:

- #144 / SA0 deterministic animation timing/frame/event contract — PR #145 / squash `d9955d4c987a627f0009a018b9b5293c6f3d8e73`.

Trusted owner real-GPU automation is complete:

- #140 / PR #141 / squash `4cfad13e0d31ed015a00c0860f525974bcdc2743`,
- `.github/workflows/gpu-gate.yml` runs only trusted `main` / `agent/**` pushes on the owner Windows self-hosted presentation-GPU runner,
- public fork PR code is not routed to the self-hosted machine,
- `scripts/gpu_gate.ps1` records commit/environment/test/checksum evidence and rejects skipped real-GPU fixtures.

SR8 completion evidence was accepted before #143 merged:

- implementation head `74b5b82c9df961baeaeb84e80169e7e621535cfb`,
- hosted CI run `31569822039` green,
- trusted owner GPU Gate run `31569818936` green with all selected Sprite GPU suites and no skips,
- exact-head Sprite final-evidence artifact `gpu-gate-74b5b82c9df961baeaeb84e80169e7e621535cfb`, digest `sha256:dcd0014191cc7bb0fdead133a38acb466e2d156bf866d171b2baa5804177163d`.

**Active core program: #59 Complete Sprite program.**  
**Only active Sprite child: #146 / draft PR #147 — SA1 `SpriteAnimator2D` authoritative runtime state.**  
**Exact next child after SA1 merges green: SA2 — deterministic playback, event emission, and transitions.**

## #59 Sprite program

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
Architecture authority: [`docs/SPRITE_ARCHITECTURE.md`](docs/SPRITE_ARCHITECTURE.md).  
Frozen SA0 timing/event contract: [`docs/SPRITE_ANIMATION_TIMING_SA0.md`](docs/SPRITE_ANIMATION_TIMING_SA0.md).  
Active SA1 state contract: [`docs/SPRITE_ANIMATOR_STATE_SA1.md`](docs/SPRITE_ANIMATOR_STATE_SA1.md).

Fixed internal order:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete]
 -> SR4 [complete] -> SR5 [complete] -> SR6 [complete] -> SR7 [complete]
 -> SR8 [complete]
 -> SA0 [complete] -> SA1 [active #146/#147] -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Exactly one Sprite child is active at a time.

## #146 / SA1 — `SpriteAnimator2D` authoritative runtime state — active via draft PR #147

SA1 implements renderer-independent typed animation state on top of the frozen SA0 timeline. It does not implement playback advancement, event traversal, transitions, Agent/MCP actions, or animation workloads.

### Prepared clip/timeline

`SpriteAnimationClip2D` is prepared once outside the fixed-step hot path:

- ordered frames use already-resolved numeric canonical Sprite region indices,
- frame durations are strictly positive integer nanoseconds,
- cumulative boundaries and exact total duration are cached once,
- duration overflow and invalid region indices fail deterministically,
- failed preparation preserves the previously valid prepared output,
- prepared clips are movable/non-copyable and are intended to be cached/reused.

Complexity:

```text
prepare clip            O(frame_count), setup allocation allowed
arbitrary time -> frame O(log frame_count), no allocation
current frame/region    O(1), no allocation
state inspection        O(1), no allocation
```

### Authoritative animator state

`SpriteAnimator2DState` is fixed-size/trivially-copyable typed state containing:

- non-owning prepared clip pointer,
- authoritative integer-nanosecond time,
- authoritative frame index,
- stopped / playing / paused state,
- once / loop / ping-pong policy,
- forward / reverse traversal direction,
- explicit completion,
- exact canonical non-negative rational speed magnitude.

Direction is separate from speed magnitude so future SA2 ping-pong bounce state does not depend on floating sign conventions. Speed normalizes by `gcd`; zero is canonical `0/1`. Floating frame progress or accumulated floating speed is not authoritative.

### Validation / ownership rules

State validation rejects:

- null/unprepared clip,
- invalid enum values,
- time outside `[0, duration]`,
- frame/time mismatch under SA0 half-open boundaries,
- non-canonical or zero-denominator speed,
- completion on loop/ping-pong,
- completed `Once` state away from its directional endpoint.

`SpriteAnimator2D::RestoreState` validates before commit, so invalid restore leaves prior state unchanged.

The clip pointer is deliberately non-owning; the prepared clip must outlive animator states that reference it. This avoids per-state shared-ownership/reference-count traffic in the fixed-step hot state. Future #86 may generalize resource lifetime without changing animation authority.

### Performance / architecture boundary

Ordinary animator state access has no:

- filesystem work,
- JSON generation/parsing,
- diagnostic string formatting,
- semantic-name/path lookup,
- renderer/GPU initialization or synchronization,
- mandatory heap allocation.

Current state/frame/region observation uses cached prepared data. Renderer failure or presentation cadence cannot mutate animation truth.

### External-reference decisions

SA1 performed the required bounded current primary-source review on 2026-08-12:

- Godot current `AnimatedSprite2D` — **ADAPT** explicit animation/frame/playing/speed state; **REJECT** floating frame progress/speed as Trace2D authority,
- Aseprite official format — **ADOPT/ADAPT** explicit per-frame duration/direction precedent; adapt runtime time to integer nanoseconds and terminate source-format identity at setup/import,
- SA0 — **ADOPT** half-open frame ownership, terminal final-frame presentation and renderer independence without reinterpretation.

No new runtime dependency is introduced.

### SA1 completion gates

PR #147 must remain draft/unmerged until the same final head satisfies:

1. focused `SpriteAnimator2DTests` compile and pass,
2. normal hosted repository CI/audits are green,
3. `docs/SPRITE_ANIMATOR_STATE_SA1.md`, `docs/SPRITES.md`, this file, issue #146 and implementation agree,
4. no renderer/GPU dependency or SA2 playback/event/transition implementation leaks into SA1.

SA1 is backend-independent and introduces no new presentation-GPU behavior, so no new local real-GPU acceptance gate is required for this child.

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
      -> S0/S1/SR0..SR8                                    [complete]
      -> #144 SA0 animation timing/frame/event contract     [complete]
      -> #146 SA1 SpriteAnimator2D authoritative state      [active via draft #147]
      -> SA2..SA4 animation
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

SA1 / #146 / draft PR #147 is the only active Sprite child. The current continuation must:

1. keep PR #147 scoped to prepared clip state plus `SpriteAnimator2D` authoritative state/validation/observation,
2. preserve SA0 integer-nanosecond/fixed-step authority and renderer independence,
3. repair only SA1 implementation/test/documentation issues exposed by review or CI,
4. require normal hosted CI/audits on the final PR head,
5. keep #147 draft/unmerged until those gates are green,
6. after all gates pass, record exact validation evidence, mark ready/merge #147, confirm #146 closes, and stop,
7. not create or implement SA2 in that completion continuation.

Only a **following** `@GitHub Trace2D 다음 진행해줘` continuation after SA1 merges green may create the SA2 child.
