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

Completed Sprite animation stages:

- #144 / SA0 deterministic animation timing/frame/event contract — PR #145 / squash `d9955d4c987a627f0009a018b9b5293c6f3d8e73`,
- #146 / SA1 `SpriteAnimator2D` authoritative runtime state — PR #147 / squash `dc8909b24dc5f67e8bec2506263d7b433c6fb2f4`.

Trusted owner real-GPU automation is complete:

- #140 / PR #141 / squash `4cfad13e0d31ed015a00c0860f525974bcdc2743`,
- `.github/workflows/gpu-gate.yml` runs only trusted `main` / `agent/**` pushes on the owner Windows self-hosted presentation-GPU runner,
- public fork PR code is not routed to the self-hosted machine,
- `scripts/gpu_gate.ps1` records commit/environment/test/checksum evidence and rejects skipped real-GPU fixtures.

SR8 completion evidence remains accepted:

- implementation head `74b5b82c9df961baeaeb84e80169e7e621535cfb`,
- hosted CI run `31569822039` green,
- trusted owner GPU Gate run `31569818936` green with all selected Sprite GPU suites and no skips,
- exact-head Sprite final-evidence artifact `gpu-gate-74b5b82c9df961baeaeb84e80169e7e621535cfb`, digest `sha256:dcd0014191cc7bb0fdead133a38acb466e2d156bf866d171b2baa5804177163d`.

**Active core program: #59 Complete Sprite program.**  
**Only active Sprite child: #148 / draft PR #149 — SA2 deterministic playback, events, loops, and transitions.**  
**Exact next child after SA2 merges green: SA3 — Agent/MCP inspection, actions, and exact-frame assertions.**

## #59 Sprite program

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
Architecture authority: [`docs/SPRITE_ARCHITECTURE.md`](docs/SPRITE_ARCHITECTURE.md).  
Frozen SA0 timing/event contract: [`docs/SPRITE_ANIMATION_TIMING_SA0.md`](docs/SPRITE_ANIMATION_TIMING_SA0.md).  
Frozen SA1 authoritative-state contract: [`docs/SPRITE_ANIMATOR_STATE_SA1.md`](docs/SPRITE_ANIMATOR_STATE_SA1.md).  
Active SA2 playback contract: [`docs/SPRITE_ANIMATOR_PLAYBACK_SA2.md`](docs/SPRITE_ANIMATOR_PLAYBACK_SA2.md).

Fixed internal order:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete]
 -> SR4 [complete] -> SR5 [complete] -> SR6 [complete] -> SR7 [complete]
 -> SR8 [complete]
 -> SA0 [complete] -> SA1 [complete] -> SA2 [active #148/#149] -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Exactly one Sprite child is active at a time.

## #148 / SA2 — deterministic playback, events, loops, and transitions — active via draft PR #149

SA2 executes the renderer-independent animation timeline frozen by SA0 over SA1's typed authoritative state. It does not add Agent/MCP adapters or SA4 animation conformance/performance workloads.

### Prepared authored events

`SpriteAnimationClip2D` now accepts optional typed authored events at setup:

- numeric semantic event IDs are resolved before playback,
- event offsets use integer nanoseconds and must be in `[0, duration)`,
- authored ordinal provides deterministic equal-time ordering,
- retained events are sorted once by `(offset, authoredOrdinal)`,
- duplicate `(offset, authoredOrdinal)` identity is rejected,
- ordinary playback performs no semantic-name/path lookup.

Preparation remains O(frame_count + event_count log event_count) setup work with allocation allowed. Prepared frame/event storage is reused by playback.

### Exact speed advancement

`SpriteAnimator2DState` adds a retained rational `speedRemainder` so repeated small fixed steps do not lose fractional progress.

The implementation deliberately avoids an unchecked `delta_ns * numerator` product. It decomposes delta by the canonical denominator, checks the whole contribution, then carries the fractional numerator/remainder exactly. This keeps the supported MSVC C++20 path portable without requiring `__int128` or floating accumulation.

Remainder rules are explicit:

- pause/resume preserves it,
- actual speed/direction changes reset it,
- seek/stop/reset/restart reset it,
- completion resets it,
- zero speed is canonical `0/1` with zero remainder.

### Playback / traversal

SA2 implements:

- `Play`, `Pause`, `Stop`, `Reset`, `Restart`, `Seek`, `SetSpeed`, `SetDirection`,
- directional `Once` completion,
- linear `Loop` wrapping,
- `PingPong` endpoint bounce/direction flips,
- SA0 forward crossing `a < event_time <= b`,
- SA0 reverse crossing `b <= event_time < a`,
- equal-time authored ordinal preservation in both directions,
- forward loop re-entry offset-zero emission exactly once,
- reverse zero-arrival semantics with no synthetic duration event.

Structural runtime emissions are typed separately from authored event IDs:

```text
AuthoredEvent | Loop | Bounce | Completed
```

### Transactional bounded output

`Advance` writes into caller-owned/reused bounded output storage and performs no mandatory fixed-tick heap allocation.

To prevent silent or partial authoritative event loss, advancement uses:

```text
count-only traversal against caller capacity
 -> if insufficient: explicit OutputCapacityExceeded, no state mutation, no partial write
 -> if sufficient: deterministic replay into output
 -> validate final state
 -> commit authoritative state
```

This intentionally pays bounded duplicate traversal work to make state/output capacity behavior deterministic and transactional.

### Performance / architecture boundary

After preparation:

- current state/frame/region access remains O(1),
- arbitrary frame lookup remains O(log frame_count),
- event segment entry uses binary search,
- update work scales with crossed authored events/structural transitions rather than nanosecond count or unrelated assets,
- no mandatory per-tick heap/filesystem/JSON/formatting/name lookup,
- no renderer/GPU initialization, synchronization, readback, or presentation authority.

### External-reference decisions

SA2 performed the required bounded current primary-source review on 2026-08-12:

- Godot current `AnimatedSprite2D` — **ADAPT** explicit play/pause/stop and separate loop/finish notification precedent; **REJECT** floating progress/speed as Trace2D authority and signals as the sole source of truth,
- Aseprite official format — **ADOPT/ADAPT** integer frame duration and forward/reverse/ping-pong/repeat authoring precedent; runtime execution remains Trace2D-owned typed integer-nanosecond state/events.

No new runtime dependency is introduced.

### Current validation state

Draft PR #149 was opened from `agent/sprite-sa2-playback-events`.

Pre-publication supporting preflight passed for the core implementation as standalone C++20 with warnings treated as errors plus an executable behavioral harness covering rational remainder, forward/reverse event ordering, loop/ping-pong traversal, completion, arithmetic overflow rejection, controls, zero-speed behavior, and transactional output exhaustion.

Repository hosted Windows/MSVC CI/audits on the current PR head remain the acceptance authority. SA2 adds no new presentation-GPU behavior, so no new local real-GPU acceptance gate is required.

### SA2 completion gates

PR #149 must remain draft/unmerged until the same final head satisfies:

1. focused `SpriteAnimator2DTests` and `SpriteAnimatorPlaybackSA2Tests` compile and pass,
2. normal hosted repository CI/audits are green,
3. `docs/SPRITE_ANIMATOR_PLAYBACK_SA2.md`, `docs/SPRITES.md`, this file, issue #148 and implementation agree,
4. output-capacity and arithmetic failures remain transactional,
5. no renderer/GPU dependency or SA3/SA4 Agent/workload scope leaks into SA2.

After all gates pass, record exact final-head validation evidence, mark PR #149 ready, merge it, confirm #148 closes, and stop. Do not create SA3 in that same completion continuation.

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
      -> #146 SA1 SpriteAnimator2D authoritative state      [complete]
      -> #148 SA2 deterministic playback/events/transitions [active via draft #149]
      -> SA3..SA4 animation
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

SA2 / #148 / draft PR #149 is the only active Sprite child. The current continuation must:

1. keep PR #149 scoped to deterministic playback controls, exact retained-remainder advancement, authored event traversal/output and `Once|Loop|PingPong` structural transitions,
2. preserve SA0 integer-nanosecond/event-crossing authority and SA1 renderer-independent state semantics,
3. repair only SA2 implementation/test/documentation issues exposed by review or CI,
4. require normal hosted CI/audits on the final PR head,
5. keep #149 draft/unmerged until those gates are green,
6. require no new real-GPU gate because SA2 introduces no presentation-GPU behavior,
7. after all gates pass, record exact validation evidence, mark ready/merge #149, confirm #148 closes, and stop,
8. not create or implement SA3 in that completion continuation.

Only a **following** `@GitHub Trace2D 다음 진행해줘` continuation after SA2 merges green may create the SA3 child.
