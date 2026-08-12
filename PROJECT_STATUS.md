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

Trusted owner real-GPU automation is complete:

- #140 / PR #141 / squash `4cfad13e0d31ed015a00c0860f525974bcdc2743`,
- `.github/workflows/gpu-gate.yml` runs only trusted `main` / `agent/**` pushes on the owner Windows self-hosted presentation-GPU runner,
- public fork PR code is not routed to the self-hosted machine,
- `scripts/gpu_gate.ps1` records commit/environment/test/checksum evidence and rejects skipped real-GPU fixtures.

SR8 completion evidence was accepted before #143 merged:

- implementation head `74b5b82c9df961baeaeb84e80169e7e621535cfb`,
- hosted CI run `31569822039` green,
- trusted owner GPU Gate run `31569818936` green with all selected Sprite GPU suites and no skips,
- exact-head Sprite final-evidence manifest built/uploaded through the same trusted run,
- artifact `gpu-gate-74b5b82c9df961baeaeb84e80169e7e621535cfb`, digest `sha256:dcd0014191cc7bb0fdead133a38acb466e2d156bf866d171b2baa5804177163d`.

**Active core program: #59 Complete Sprite program.**  
**Only active Sprite child: #144 / SA0 — deterministic animation timing, frame, and event contract.**  
**Exact next child after SA0 merges green: SA1 — `SpriteAnimator2D` authoritative runtime state.**

## #59 Sprite program

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
SA0 contract: [`docs/SPRITE_ANIMATION_TIMING_SA0.md`](docs/SPRITE_ANIMATION_TIMING_SA0.md).  
Machine-readable SA0 invariants: [`docs/contracts/sprite-animation-sa0.json`](docs/contracts/sprite-animation-sa0.json).

Fixed internal order:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete]
 -> SR4 [complete] -> SR5 [complete] -> SR6 [complete] -> SR7 [complete]
 -> SR8 [complete]
 -> SA0 [active #144] -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Exactly one Sprite child is active at a time.

## #144 / SA0 — deterministic animation timing/frame/event contract — active

SA0 is a backend-independent contract freeze. It does not implement the full `SpriteAnimator2D` or playback command/state-transition surface.

### Exact time authority

- canonical animation/frame/event time uses integer nanoseconds, matching `FixedStepRuntime`'s existing integer `std::chrono::nanoseconds` domain,
- each frame duration is positive and clip duration is the checked exact sum,
- accumulated floating seconds are not authoritative animation state,
- wall-clock render time and presentation interpolation alpha never advance authoritative animation truth,
- future SA2 speed scaling must preserve exact integer-time progress with retained remainder rather than dropping/accumulating float error.

### Frame boundary contract

For cumulative frame boundaries `b0=0 < ... < bN=duration`:

```text
frame i owns [bi, b(i+1))
```

An exact internal boundary selects the following frame. Non-loop completion may retain `t == duration` explicitly while presenting the final authored frame; completion is state, not a synthetic extra frame.

### Event boundary contract

Authored events live in `[0, duration)` and preserve authored ordinal for equal offsets.

```text
forward a -> b : a < event_time <= b
reverse a -> b : b <= event_time < a
```

Large advances preserve every crossed event in deterministic traversal order. Linear loops and ping-pong traversal are composed from ordered segments so boundary events cannot be silently lost or double-fired. Forward loop entry emits offset-zero events once after the loop marker; the following positive-time segment excludes zero. Seek/reset/inspection do not replay historical events.

### Performance / ownership boundary

- animation truth is renderer/GPU/filesystem independent,
- setup may precompute cumulative frame/event offsets and resolved Sprite region identities,
- steady-state updates perform no mandatory heap allocation, JSON work, filesystem access, diagnostic formatting or semantic-name lookup per fixed tick,
- work scales with crossed timeline boundaries rather than nanoseconds or unrelated assets,
- event output uses caller-owned/reused bounded storage or an equivalent allocation-free seam; capacity exhaustion is explicit and never silently drops events.

### External-reference decisions

SA0 records a bounded 2026-08-12 primary-source review:

- Aseprite official file format — **ADOPT/ADAPT** explicit per-frame integer duration and one canonical timeline; adapt ms to Trace2D integer ns,
- Godot stable `SpriteFrames` / `AnimatedSprite2D` — **ADOPT/ADAPT** variable-duration/reverse/linear-loop/ping-pong semantics where compatible; **REJECT** float progress/speed as authoritative deterministic time,
- Godot fixed-timestep interpolation guidance — **ADOPT** the already-frozen separation between fixed authoritative state and rendered presentation.

These are references only; SA0 adds no runtime dependency.

### SA0 completion gates

The SA0 PR must remain unmerged until the same final head satisfies:

1. `Sprite SA0 Contract` green,
2. normal hosted repository CI/audits green,
3. `docs/SPRITE_ANIMATION_TIMING_SA0.md`, `docs/contracts/sprite-animation-sa0.json`, `docs/SPRITES.md`, and this file agree,
4. no full SA1 implementation has leaked into the SA0 scope.

SA0 is backend-independent and introduces no new presentation-GPU behavior, so no new local real-GPU acceptance gate is required for this child.

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
      -> #144 SA0 animation timing/frame/event contract     [active]
      -> SA1..SA4 animation
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

SA0 / #144 is the only active Sprite child. The current continuation must:

1. keep the SA0 PR scoped to exact animation time/frame/event authority and contract validation,
2. preserve integer-nanosecond/fixed-step authority and renderer independence,
3. require the new `Sprite SA0 Contract` plus normal hosted CI/audits on the final head,
4. reconcile documentation with live merged SR8 state,
5. keep the SA0 PR unmerged until those gates are green,
6. not implement or create SA1 in this continuation.

Only a **following** `@GitHub Trace2D 다음 진행해줘` continuation after SA0 merges green may create the SA1 child.