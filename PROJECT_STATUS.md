# Trace2D Project Status

Last repository-state update: **2026-08-10**

This file is the operational handoff for the next contributor or coding agent. Live PR/CI/merge state and explicit owner-approved contracts win over stale prose.

## Current state

Trace2D is an **AI-first / AI-operated C++20 2D engine** with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

The seven-part particle program #47-#53 is implementation-complete in PR #114. #53 now contains real Windows presentation-GPU conformance evidence, Release CPU-oracle calibration evidence, measured backend recommendation bands, and the final author -> verify -> measure -> human decide -> GPU compile/execute -> conform contract.

At the time of this update PR #114 is the active core PR and must receive final-head hosted CI before merge. **Do not begin the next core implementation until #114 merges green.** After it merges, close #53 and umbrella #46 and advance directly to **#97 machine-readable intent / Definition of Done**.

Primary final-particle contracts:

- [`docs/PARTICLES.md`](docs/PARTICLES.md)
- [`docs/PARTICLE_ANALYSIS.md`](docs/PARTICLE_ANALYSIS.md)
- [`docs/PARTICLE_GPU_RUNTIME.md`](docs/PARTICLE_GPU_RUNTIME.md)
- [`docs/PARTICLE_CONFORMANCE.md`](docs/PARTICLE_CONFORMANCE.md)
- [`docs/evidence/particle-53/924dbc1/`](docs/evidence/particle-53/924dbc1/README.md)

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
12. #53 CPU/GPU conformance, workloads, measured recommendation guidance — implementation-complete in PR #114; merge is the remaining repository-state gate

Production architecture freeze #85 is complete via PR #94.

## #53 final evidence

The final gate was executed against PR #114 commit `924dbc19027a350c9bae819eea28789eea77bbdd` on:

- AMD Ryzen 5 5600X,
- NVIDIA GeForce RTX 3070,
- Windows/MSVC,
- Debug real-GPU tests,
- Release analyzer timings.

Required real-GPU tests passed, not skipped:

```text
ParticleGpuConformanceTests.ExplicitGpuExecutionTracksCpuOracleAcrossRandomSpawnMotionAndLifetime
ParticleGpuSmokeTests.ExplicitGpuEmitterAdvancesCapturesAndReusesCapacity
```

Release calibration uses deterministic 240-frame CPU-reference windows. The normalized p95 values are p95 window totals divided by 240, not individual-frame p95 histograms:

```text
small:      capacity 128,  peak 42    -> 0.0009525 ms/frame equivalent
medium:     capacity 1024, peak 856   -> 0.0291575 ms/frame equivalent
gpu-scale:  capacity 4096, peak 3400  -> 0.1075925 ms/frame equivalent
```

The `gpu-scale` timing is CPU-reference-oracle timing for the same semantic program, not GPU wall-clock timing. Real GPU correctness/resource behavior is covered by the hardware conformance/smoke tests and structural runtime metrics.

Trace2D V1 recommendation bands on a representative target machine are:

```text
<= 0.05 ms/frame normalized p95  -> keep_cpu comfort band
0.05 .. 0.10 ms/frame            -> human judgment band
>= 0.10 ms/frame                  -> consider_gpu
```

These are measured guidance bands, not automatic backend switching and not universal particle-count limits. Final backend text remains human-controlled.

## Owner-fixed core execution order

Routine `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, or equivalent follows the first incomplete/unblocked item below, but never skips an active core PR or required local gate.

```text
#53 particle CPU/GPU conformance/workloads/guidance   [active PR #114; merge gate]

AI-operated foundation
 -> #97 machine-readable intent / Definition of Done
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
- #46 particle program,
- #96 AI-operated production loop,
- #100 autonomous benchmark program,
- #59 Sprite program,
- #67 game-production foundation,
- #85 production architecture freeze,
- #93 later mature-engine breadth gates,
- #101 production capability gap register,
- #106 evidence-gated Agent template/diagnostic/repair-recipe knowledge layer.

#93/#101/#106 do not authorize routine continuation to bypass the fixed order.

## Particle architecture now frozen by #47-#53

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

## AI-operated product foundation

After #114 merges, #97 is the exact next core implementation. The AI-operated roadmap must build toward a workflow where:

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

Primary matched environments remain Godot + generic coding tools, Godot + a pinned reviewed bridge, and Trace2D + its public Agent surface. Track success, repair iterations, token/tool usage when measurable, multimodal calls, human interventions, deterministic verification coverage, unresolved failures, and environment-labelled elapsed time.

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
