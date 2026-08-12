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
- #146 / SA1 `SpriteAnimator2D` authoritative runtime state — PR #147 / squash `dc8909b24dc5f67e8bec2506263d7b433c6fb2f4`,
- #148 / SA2 deterministic playback/events/loops/transitions — PR #149 / squash `7f530dc8001a49aeddc7b0d98aa9dbeb781b7c66`.

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
**Only active Sprite child: #150 / draft PR #151 — SA3 Agent/MCP inspection, actions, and exact-frame assertions.**  
**Active branch: `agent/sprite-sa3-agent-verification`.**  
**Exact next child after SA3 merges green: SA4 — animation conformance, determinism, and performance workloads.**

## #59 Sprite program

Program contract: [`docs/SPRITES.md`](docs/SPRITES.md).  
Architecture authority: [`docs/SPRITE_ARCHITECTURE.md`](docs/SPRITE_ARCHITECTURE.md).  
Frozen SA0 timing/event contract: [`docs/SPRITE_ANIMATION_TIMING_SA0.md`](docs/SPRITE_ANIMATION_TIMING_SA0.md).  
Frozen SA1 authoritative-state contract: [`docs/SPRITE_ANIMATOR_STATE_SA1.md`](docs/SPRITE_ANIMATOR_STATE_SA1.md).  
Frozen SA2 playback contract: [`docs/SPRITE_ANIMATOR_PLAYBACK_SA2.md`](docs/SPRITE_ANIMATOR_PLAYBACK_SA2.md).  
Active SA3 Agent/MCP verification contract: [`docs/SPRITE_ANIMATION_AGENT_SA3.md`](docs/SPRITE_ANIMATION_AGENT_SA3.md).

Fixed internal order:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete]
 -> SR4 [complete] -> SR5 [complete] -> SR6 [complete] -> SR7 [complete]
 -> SR8 [complete]
 -> SA0 [complete] -> SA1 [complete] -> SA2 [complete]
 -> SA3 [active #150/#151] -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Exactly one Sprite child is active at a time.

## #150 / SA3 — Agent/MCP inspection, actions, and exact-frame assertions — active via draft PR #151

SA3 exposes the renderer-independent SA0-SA2 animation authority to coding agents. It does **not** add a second animation state machine, renderer authority, SA4 workloads, scene-component ownership, or a new real-GPU path.

### Protocol-independent Agent surface

New `agent::SpriteAnimatorBinding` is explicit and non-owning:

```text
entity semantic id
SpriteAnimator2D*
```

The caller owns animator/prepared-clip lifetime. Agent operations consume the existing `SpriteAnimator2DState` and prepared `SpriteAnimationClip2D` only.

`AgentFacade::InspectSpriteAnimator` returns exact scalar state:

```text
entity id
clip duration/frame/event counts
time_ns
frame_index
region_index
playback
loop_mode
direction
completed
speed numerator/denominator/remainder
```

Inspection does not advance time, replay events, initialize rendering/GPU, or perform ordinary-frame reporting work.

### Explicit actions

`AgentFacade::ActOnSpriteAnimator` delegates directly to SA2 runtime methods:

```text
Play | Pause | Stop | Reset | Restart | Seek
SetSpeed | SetDirection | Advance
```

The Agent layer does not reproduce transition, loop, ping-pong, rational-speed, completion, or event-crossing logic.

Explicit `Advance`:

- takes integer nanoseconds, never wall-clock time,
- materializes evidence only because the Agent request asked for it,
- preserves ordered `AuthoredEvent | Loop | Bounce | Completed` emissions,
- bounds one request to at most 4096 emissions,
- inherits SA2 transactional `OutputCapacityExceeded`: no state mutation and no partial authoritative output.

### Exact assertions

`AgentFacade::AssertSpriteAnimator` checks one finite typed current-state field immediately.

Supported values are bool/int64/uint64/string. Supported fields cover clip duration/counts, time/frame/region, playback/loop/direction/completion, speed numerator/denominator/remainder.

Assertions deliberately have:

- no implicit retry,
- no implicit simulation step,
- no timeout/wait loop,
- no renderer/capture dependency.

Mismatch diagnostics contain exact expected/observed values and bounded current-state context rather than unrelated world or pixel dumps.

### MCP adapter

MCP remains serialization only. A configured server exposes exactly:

```text
trace2d.sprite_animation.inspect
trace2d.sprite_animation.action
trace2d.sprite_animation.assert
```

Playback operations are an `action` enum rather than one MCP tool per runtime method. Servers with no configured Sprite animator bindings keep the previous 12-tool catalog.

The repository remains on MCP `2026-07-28`. `structuredContent` remains the machine-readable result and compatibility `TextContent` carries the serialized payload. Read-only/action annotations are hints only.

### Performance / ownership boundary

Ordinary `SpriteAnimator2D` stepping is unchanged by SA3 and receives no:

- Agent snapshot work,
- JSON/string formatting,
- filesystem access,
- semantic-name lookup,
- renderer/GPU access,
- mandatory heap allocation,
- background verification/report maintenance.

Explicit MCP binding lookup is O(configured bindings) by linear scan. No speculative cache/index is added without SA4 workload evidence.

### External-reference decisions

SA3 performed the required bounded current primary-source pass on 2026-08-12:

- MCP `2026-07-28` — **ADAPT** stateless/self-describing requests, deterministic/cacheable lists and full JSON Schema tool inputs,
- MCP Tools — **ADOPT** schema-defined inputs and structured results; **ADAPT** repository compatibility text alongside `structuredContent`,
- MCP tool annotations — **ADAPT** read-only/action hints only; never runtime authority,
- Playwright assertions — **ADAPT** expected/observed diagnostic shape; **REJECT** auto-retry for exact animation state because retrying would implicitly move the simulation,
- Trace2D Particle Agent verification — **ADOPT** explicit-request snapshots, finite typed assertions, stable errors and zero ordinary-step reporting work.

No new runtime dependency is introduced.

### Focused tests added

Agent tests cover:

- authoritative state inspection without renderer,
- exact ordered forward emission evidence,
- direct runtime action delegation,
- seek/restart no historical event replay,
- transactional output-capacity failure,
- exact assertion success/state mismatch/type mismatch,
- unavailable binding/state errors.

MCP tests cover:

- Sprite tool discovery only with configured bindings,
- inspect -> exact advance -> assert against one authoritative animator,
- ordered emission serialization,
- capacity failure/state preservation,
- unknown binding and runtime-rejected action error stability.

Test fixtures explicitly link `Trace2D::Assets`; the runtime library itself remains free of an unnecessary Assets dependency.

### Current validation state

Draft PR #151 is open from `agent/sprite-sa3-agent-verification` against `main@7f530dc8001a49aeddc7b0d98aa9dbeb781b7c66`.

The implementation environment used for this continuation cannot directly clone GitHub due container DNS restrictions, so local full-repository compile evidence is unavailable here. Normal hosted GitHub Actions on the exact PR head are the compile/test acceptance authority and must be repaired to green before readiness.

SA3 adds no presentation/GPU behavior; no new local real-GPU acceptance gate is required.

### SA3 completion gates

PR #151 must remain draft/unmerged until the same final head satisfies:

1. focused Agent and MCP Sprite-animation tests compile and pass,
2. existing Agent/MCP tests remain green, including the unchanged 12-tool catalog for unbound servers,
3. normal hosted repository CI/audits are green,
4. `docs/SPRITE_ANIMATION_AGENT_SA3.md`, `docs/SPRITES.md`, this file, issue #150 and implementation agree,
5. action/capacity failures remain structured and transactional,
6. no renderer/GPU dependency, normal-step reporting cost, or SA4 workload scope leaks into SA3.

After all gates pass, record exact final-head validation evidence, mark PR #151 ready, merge it, confirm #150 closes, and stop. Do not create SA4 in that same completion continuation.

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
      -> #148 SA2 deterministic playback/events/transitions [complete]
      -> #150 SA3 Agent/MCP exact verification              [active via draft #151]
      -> SA4 animation conformance/workloads
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
        -> Agent inspection/action/assertion
        -> resolved/derived presentation
        -> backend renderer resources
```

Agent/MCP snapshots, GPU resources, pixels and review artifacts never become canonical Sprite/gameplay truth.

The accepted B0 cohort/raw evidence remains under `benchmarks/b0/`; B0 proves the matched methodology/evidence loop, not broad engine superiority.

## Continuation rule

SA3 / #150 / draft PR #151 is the only active Sprite child. The current continuation must:

1. keep PR #151 scoped to protocol-independent authoritative animator inspection/actions/exact assertions plus a thin MCP adapter,
2. preserve SA0 integer-nanosecond/event-crossing authority and SA1-SA2 renderer-independent runtime semantics,
3. repair only SA3 implementation/test/documentation issues exposed by review or CI,
4. require normal hosted CI/audits on the final PR head,
5. keep #151 draft/unmerged until those gates are green,
6. require no new real-GPU gate because SA3 introduces no presentation-GPU behavior,
7. after all gates pass, record exact validation evidence, mark ready/merge #151, confirm #150 closes, and stop,
8. not create or implement SA4 in that completion continuation.

Only a **following** `@GitHub Trace2D 다음 진행해줘` continuation after SA3 merges green may create the SA4 child.
