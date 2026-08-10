# Trace2D Project Status

Last repository-state update: **2026-08-10**

This file is the operational handoff for the next contributor or coding agent. Compiling code/tests, live PR/CI/merge state, and explicit owner-approved contracts outrank stale prose.

## Current state

Trace2D is an **AI-first / AI-operated C++20 2D engine** with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

The seven-part particle program #47-#53 is complete via PR #114. Issue #53 and umbrella #46 are closed complete after the required real Windows GPU conformance/smoke evidence and Release CPU-reference calibration were committed.

**#97 machine-readable intent / Definition of Done is now the active core implementation in draft PR #115. Do not begin #98 while #115 is open.**

PR #115 establishes:

- versioned machine-readable work intent, deliverables, acceptance and constraints,
- explicit deterministic / presentation / multimodal / human verification classes,
- an evidence-backed capability catalog that separates `available`, `tested`, and `production_supported`,
- derived local `ready` / `blocked` / `review_needed` / `complete` / `failed` state,
- unavailable capability as an eligibility block rather than an implementation failure,
- explicit `external_truth` requirements for changing GitHub/CI/environment/hardware/license/human facts,
- the `trace2d_work_state` JSON query for Agent/harness consumption.

Current #97 contracts on the PR branch:

- [`docs/WORK_SPEC.md`](docs/WORK_SPEC.md)
- [`config/trace2d.capabilities.toml`](config/trace2d.capabilities.toml)
- [`engine/agent/include/trace2d/agent/WorkSpec.hpp`](engine/agent/include/trace2d/agent/WorkSpec.hpp)

The connector environment used to author PR #115 could not run a local repository build, so **hosted final-head CI is the current acceptance gate**. Do not mark #97 complete or advance to #98 until #115 is green and merged.

## Completed foundation and particle sequence

1. #40 deterministic texture asset cache/import — complete via PR #45
2. #42 text/basic UI — complete via PR #55
3. #43 semantic UI tree/Agent interaction — complete via PR #56
4. #39 MCP transport over Agent/Testing — complete via PR #58
5. #41 reproducible renderer workloads — complete via PR #63
6. #47 particle deterministic frame/keyed-random contracts — complete via PR #64
7. #48 rich deterministic CPU particle reference — complete via PR #65
8. #49 text-authored effects + `ParticleEmitter2D` — complete via PR #66
9. #50 complete Agent verification over CPU particle reference state — complete via PR #83
10. #51 CPU cost analysis + explicit backend ownership + deterministic compiler — complete via PR #84
11. #52 explicit GPU particle runtime — complete via PR #95 after the required real-GPU smoke gate
12. #53 CPU/GPU conformance, workloads, measured recommendation guidance — complete via PR #114

Production architecture freeze #85 is complete via PR #94.

## Particle architecture frozen by #47-#53

The V1 particle contract is:

```text
rich text-authored effect
  -> deterministic CPU semantic oracle
  -> exact structured Agent verification
  -> deterministic structural cost analysis
  -> optional environment-labelled Release timing
  -> explicit human CPU/GPU backend decision
  -> deterministic minimized GPU artifact when selected
  -> persistent GPU compute/instanced presentation
  -> layered CPU/GPU conformance
```

Hard invariants:

- CPU reference remains semantic authority.
- Backend is explicit authored text; analyzer/build never silently changes it.
- Normal GPU mode does not duplicate the full CPU reference simulation.
- Normal GPU frames perform no particle readback/fence wait.
- GPU storage is capacity-bounded and reused.
- No per-particle draw-call path.
- Compiler/artifact/layout identity is deterministic.
- Cross-vendor GPU floating point is checked through explicit tolerance/invariants, not universal float-bit identity.
- Structural evidence and machine timing remain separate.
- Visual style/feel may use multimodal review; final creative judgment remains human.

Primary particle contracts/evidence:

- [`docs/PARTICLES.md`](docs/PARTICLES.md)
- [`docs/PARTICLE_ANALYSIS.md`](docs/PARTICLE_ANALYSIS.md)
- [`docs/PARTICLE_GPU_RUNTIME.md`](docs/PARTICLE_GPU_RUNTIME.md)
- [`docs/PARTICLE_CONFORMANCE.md`](docs/PARTICLE_CONFORMANCE.md)
- [`docs/evidence/particle-53/924dbc1/`](docs/evidence/particle-53/924dbc1/README.md)

## Owner-fixed core execution order

Routine `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, or equivalent follows the first incomplete/unblocked item below and never skips an active core PR or recognized human/environment gate.

```text
AI-operated foundation
 -> #97 machine-readable intent / Definition of Done         [active draft PR #115; hosted CI gate]
 -> #98 unified verify / diagnose / repair / WorkResult
 -> #99 result-review Workspace / feedback loop
 -> #102 Benchmark B0 matched harness + current-capability tasks

Content production
 -> #59 complete Sprite program
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

Umbrellas/registers:

- #13 core practical-engine roadmap,
- #96 AI-operated production loop,
- #100 autonomous benchmark program,
- #59 Sprite program,
- #67 game-production foundation,
- #85 production architecture freeze,
- #93 later mature-engine breadth gates,
- #101 production capability gap register,
- #106 evidence-gated Agent template/diagnostic/repair-recipe knowledge layer.

#93/#101/#106 do not authorize routine continuation to bypass the fixed order.

## #97 machine-readable intent boundary

PR #115 deliberately keeps committed and live truth separate:

```text
committed WorkSpec / capability catalog
  -> local readiness + outstanding acceptance + capability eligibility

live GitHub / CI / environment / hardware / license / human state
  -> queried by the owning orchestration stage when required
```

Important rules:

- a capability is never inferred from a symbol/file merely existing,
- positive capability claims carry repository evidence,
- `production_supported => tested => available`, but the reverse is not implied,
- a task requiring an unavailable capability is blocked/not eligible rather than counted as an Agent failure,
- deterministic and presentation criteria may complete at `verified`,
- multimodal and human criteria require explicit `approved`,
- live CI/hardware/license/human facts are declared as external requirements rather than copied into stale committed state,
- work-state parsing/evaluation is explicit tooling work and never enters engine frame hot paths.

After #115 merges green, close #97 and advance this file to **#98 unified verify / diagnose / repair / WorkResult**.

## AI-operated product foundation

The roadmap builds toward a workflow where:

- human intent becomes machine-readable Definition of Done,
- verify/diagnose/repair share structured truth,
- Workspace presents results/review/feedback instead of requiring a Unity-style editor loop,
- CLI/MCP/Workspace share protocol-independent Agent/result contracts,
- benchmark claims are based on matched multi-run evidence rather than demos.

Autonomous benchmark growth remains:

```text
#102 B0 — harness + current-capability matched tasks
 -> #103 B1 — Sprite/animation/particle matched tasks
 -> #104 B2 — coherent autonomous top-down combat micro-game
```

Primary matched environments remain Godot + generic coding tools, Godot + a pinned reviewed bridge, and Trace2D + its public Agent surface. Missing Trace2D capability is reported as not eligible, not as an autonomous-agent failure.

## Continuation rule

For routine continuation:

1. read `AGENTS.md` and this file,
2. inspect live open PRs/CI and recent merges first,
3. reconcile stale prose against live GitHub state,
4. if an active core PR exists, finish only that PR/gate,
5. never treat a skipped hardware test as real hardware evidence,
6. after the active PR merges green, choose the first incomplete/unblocked item from the fixed order,
7. implement one coherent issue/child vertical slice,
8. test, validate, document, and update the handoff,
9. publish/update one scoped PR,
10. do not begin the following core child until the current one merges green.
