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

- #119 / S0 — PR #120,
- #121 / S1 — PR #122,
- #123 / SR0 — PR #124,
- #125 / SR1 — PR #126,
- #127 / SR2 — PR #128,
- #130 / SR3 — PR #131,
- #132 / SR4 — PR #133,
- #134 / SR5 — PR #135,
- #136 / SR6 — PR #137,
- #138 / SR7 — PR #139,
- #142 / SR8 — PR #143 / squash `2108122dad5ac2dcbb964f7ada0e80f7afa21003`.

Completed Sprite animation predecessors:

- #144 / SA0 deterministic timing/frame/event contract — PR #145 / squash `d9955d4c987a627f0009a018b9b5293c6f3d8e73`,
- #146 / SA1 `SpriteAnimator2D` authoritative state — PR #147 / squash `dc8909b24dc5f67e8bec2506263d7b433c6fb2f4`,
- #148 / SA2 deterministic playback/events/loops/transitions — PR #149 / squash `7f530dc8001a49aeddc7b0d98aa9dbeb781b7c66`,
- #150 / SA3 Agent/MCP exact verification — PR #151 / squash `d7a509ca03f851436d495183503f798c8afb8c2a`.

Trusted owner real-GPU automation from #140/#141 remains the presentation-GPU gate. SR8 final real-GPU evidence remains accepted; SA0-SA4 do not add presentation/GPU behavior and therefore do not create a new GPU acceptance gate.

**Active core program: #59 Complete Sprite program.**  
**Only active Sprite child: #152 / draft PR #153 — SA4 animation conformance, determinism, and workloads.**  
**Active branch: `agent/sprite-sa4-conformance-workloads`.**  
**Exact next child after SA4 merges green: SPP0 — deterministic Sprite processing/QA report contract.**

## #59 Sprite program

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
SA0 timing/event contract: [`docs/SPRITE_ANIMATION_TIMING_SA0.md`](docs/SPRITE_ANIMATION_TIMING_SA0.md).  
SA1 authoritative-state contract: [`docs/SPRITE_ANIMATOR_STATE_SA1.md`](docs/SPRITE_ANIMATOR_STATE_SA1.md).  
SA2 playback contract: [`docs/SPRITE_ANIMATOR_PLAYBACK_SA2.md`](docs/SPRITE_ANIMATOR_PLAYBACK_SA2.md).  
SA3 Agent/MCP verification contract: [`docs/SPRITE_ANIMATION_AGENT_SA3.md`](docs/SPRITE_ANIMATION_AGENT_SA3.md).  
Active SA4 conformance/workload contract: [`docs/SPRITE_ANIMATION_CONFORMANCE_SA4.md`](docs/SPRITE_ANIMATION_CONFORMANCE_SA4.md).

Fixed internal order:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete]
 -> SR4 [complete] -> SR5 [complete] -> SR6 [complete] -> SR7 [complete]
 -> SR8 [complete]
 -> SA0 [complete] -> SA1 [complete] -> SA2 [complete] -> SA3 [complete]
 -> SA4 [active #152/#153]
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Exactly one Sprite child is active at a time.

## #152 / SA4 — animation conformance, determinism, and workloads — active via draft PR #153

SA4 closes the deterministic Sprite-animation runtime phase without changing SA0-SA3 runtime semantics.

The PR adds an explicit `trace2d_sprite_animation_workload` tool with three committed workloads:

- `steady_loop_rational` — 6,000 fixed advances at exact `2/3` speed with forward looping and offset-zero/event boundaries,
- `dense_event_ping_pong` — dense/equal-time events, exact `5/4` speed and repeated ping-pong bounces,
- `large_step_multi_wrap` — reverse looping with advances larger than three clip durations.

Structural mode runs a selected workload twice from fresh state and requires identical semantic summaries. Evidence includes exact state/emission counts and a stable FNV-1a digest over explicit numeric/enumerated semantics only. Pointers, wall-clock values, renderer/GPU state and platform object bytes are excluded.

Focused SA4 tests cover:

- long-running exact replay,
- split-step versus aggregate-step state/transcript equivalence where SA0 semantics define equivalence,
- long-run retained-rational quotient/remainder behavior,
- large ping-pong bounce/event replay,
- transactional output-capacity failure under multi-wrap stress,
- restart/seek future-transcript repeatability without historical event replay.

Optional timing mode is local Release evidence only:

- setup and report serialization are outside measured windows,
- warmup is explicit and discarded,
- repeated windows report average/median/p95,
- OS/compiler/build/machine metadata is emitted,
- hosted/shared CI never fails on wall-clock timing thresholds.

Ordinary `SpriteAnimator2D::Advance` receives no new mandatory allocation, hash, transcript, JSON/string, filesystem, Agent/MCP, wall-clock, renderer or GPU work. All new measurement/reporting cost is explicit tooling/test work.

### External-reference decisions

The required current primary-source pass for #152 was refreshed on 2026-08-12:

- FoundationDB Simulation — **ADOPT/ADAPT** deterministic replay as a correctness amplifier and separate correctness from performance testing,
- Google Benchmark user guide — **ADAPT** warmup/repetitions/statistics/context; **REJECT** shared-CI timing gates and a new dependency for this bounded runner,
- Godot `SpriteFrames` / `AnimatedSprite2D` — **ADAPT** loop/reverse/ping-pong comparison cases while retaining Trace2D integer/rational authority,
- Aseprite official file format — **ADOPT/ADAPT** per-frame duration and animation direction/repeat precedent.

No external runtime dependency is introduced.

### Current validation state

Draft PR #153 targets `main@d7a509ca03f851436d495183503f798c8afb8c2a`.

The implementation environment cannot resolve `github.com` for a full checkout, so local integration build evidence is unavailable. New C++ sources were syntax-checked with C++20 `-Wall -Wextra -Werror` against API-shape stubs before publication. Hosted GitHub Actions on the exact PR head are the integration compile/test authority and must be green before readiness.

SA4 introduces no presentation/GPU behavior; no new local real-GPU acceptance gate is required.

### SA4 completion gates

PR #153 must remain draft/unmerged until one final head satisfies:

1. all focused `SpriteAnimationConformanceSA4Tests` compile and pass,
2. workload discovery and all three deterministic structural workload CTests pass,
3. repeated fresh workload runs produce identical structural evidence,
4. normal hosted repository CI/audits are green,
5. `docs/SPRITE_ANIMATION_CONFORMANCE_SA4.md`, `docs/SPRITES.md`, this file, #152 and implementation agree,
6. timing remains environment-labelled local evidence and never a hosted-CI threshold,
7. no runtime hot-path reporting/hash/timing work or renderer/GPU dependency is introduced.

After all gates pass, record exact final-head validation evidence, mark PR #153 ready, merge it, confirm #152 closes, and stop. **Do not create SPP0 in that same completion continuation.**

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
      -> #144 SA0                                          [complete]
      -> #146 SA1                                          [complete]
      -> #148 SA2                                          [complete]
      -> #150 SA3                                          [complete]
      -> #152 SA4                                          [active via draft #153]
      -> SPP0..SPP5 offline processing/generation
      -> SE2E -> SPERF
 -> #103 Benchmark B1 Sprite/animation/particle matched tasks

External game-production foundation
 -> #69 -> #70 -> #71 -> #86 -> #87 -> #88 -> #72 -> #73 -> #74 -> #75
 -> #104 -> #89 -> #90 -> #76 -> #77 -> #91 -> #78 -> #92 -> #79

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
        -> explicit Agent inspection/action/assertion
        -> explicit deterministic conformance/workloads
        -> resolved/derived presentation
        -> backend renderer resources
```

Agent/MCP snapshots, workload hashes, timing samples, GPU resources, pixels and review artifacts never become canonical Sprite/gameplay truth.

The accepted B0 cohort/raw evidence remains under `benchmarks/b0/`; B0 proves the matched methodology/evidence loop, not broad engine superiority.

## Continuation rule

SA4 / #152 / draft PR #153 is the only active Sprite child. The current continuation must:

1. keep PR #153 scoped to animation conformance/deterministic workloads/local timing evidence,
2. preserve SA0-SA3 integer/rational/event/runtime authority,
3. repair only SA4 implementation/test/documentation issues exposed by review or CI,
4. require normal hosted CI/audits on the final PR head,
5. keep #153 draft/unmerged until those gates are green,
6. require no new real-GPU gate because SA4 introduces no presentation-GPU behavior,
7. after all gates pass, record exact validation evidence, mark ready/merge #153, confirm #152 closes, and stop,
8. not create or implement SPP0 in that completion continuation.
