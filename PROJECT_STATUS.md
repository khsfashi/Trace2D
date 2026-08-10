# Trace2D Project Status

Last repository-state update: **2026-08-10**

This is the operational handoff for the next contributor or coding agent. Compiling code/tests, live PR/merge/CI state and explicit owner-approved product/architecture decisions win over stale prose.

## Current state

**Public Alpha `v0.1.0-alpha.1` is released. Particle children #47-#51 are merged complete. Production architecture freeze #85 is complete via PR #94. PR #95 implements #52, the explicit GPU particle runtime; while #95 is open, finish only that implementation/gate. After #95 merges green, #53 is the exact next core implementation task.**

PR #95 consumes the deterministic #51 compiler artifact without changing CPU semantic authority.

A parallel owner-requested governance update has also fixed Trace2D's long-term identity as an **AI-first / AI-operated 2D engine** and inserted AI-operated verification/review/benchmark stages into the future sequence. That governance work does **not** mark #52 complete and does not bypass the required real-GPU gate below.

Current handoff contracts present on this branch:

- [`docs/PARTICLE_GPU_RUNTIME.md`](docs/PARTICLE_GPU_RUNTIME.md)
- [`docs/PARTICLES.md`](docs/PARTICLES.md)
- [`docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md`](docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md)

Live owner-approved product/benchmark tracking is recorded by Issues #96-#106; once the governance documentation is merged, its `README.md`, `AGENTS.md`, `docs/AI_OPERATED_WORKFLOW.md`, `docs/WORKSPACE.md`, `docs/AUTONOMOUS_BENCHMARK.md`, `docs/REFERENCE_PROJECTS.md` and `docs/PRODUCTION_GAPS.md` become the corresponding repository contracts.

## Completed foundation and particle predecessors

1. **#40** deterministic texture asset cache/import — complete via PR #45
2. **#42** text/basic UI — complete via PR #55
3. **#43** semantic UI tree/Agent interaction — complete via PR #56
4. **#39** MCP transport over Agent/Testing — complete via PR #58
5. **#41** reproducible renderer workloads — complete via PR #63
6. **#47** particle deterministic frame/keyed-random contracts — complete via PR #64
7. **#48** rich deterministic CPU particle reference — complete via PR #65
8. **#49** text-authored effects + `ParticleEmitter2D` — complete via PR #66
9. **#50** complete Agent verification over CPU particle reference state — complete via PR #83
10. **#51** CPU particle cost analysis + explicit human backend choice + deterministic compiler — complete via PR #84
11. **#52** explicit GPU particle runtime — implementation in PR #95; **not complete until final-head CI and the required real-GPU smoke gate pass and the PR merges**

## Required #95 real-GPU gate

Hosted CI intentionally does not count a skipped opt-in GPU smoke test as real-GPU evidence.

Run from an updated checkout of **the final PR #95 head** on a Windows machine with a presentation GPU:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel

$env:TRACE2D_RUN_GPU_SMOKE = "1"
ctest --preset windows-debug -R ParticleGpuSmokeTests --output-on-failure
```

Required result:

```text
ParticleGpuSmokeTests.ExplicitGpuEmitterAdvancesCapturesAndReusesCapacity
```

must **pass, not skip**.

Do not mark #52 complete or start #53 based only on hosted CI.

## Owner-fixed core execution order

While PR #95 is open, finish #95 only. After it merges green, routine `Trace2D next/continue` follows:

```text
#53 particle CPU/GPU conformance, workloads, safe budgets and guidance

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

- **#13** core practical-engine roadmap,
- **#46** particle program,
- **#96** AI-operated production loop,
- **#100** autonomous benchmark program,
- **#59** Sprite program,
- **#67** game-production foundation,
- **#85** production architecture freeze,
- **#93** later mature-engine breadth gates,
- **#101** production capability gap register,
- **#106** later evidence-gated Agent template/diagnostic/repair-recipe knowledge layer.

#93/#101/#106 do not authorize routine `next/continue` to skip the fixed sequence. #106 is promoted only if benchmark evidence demonstrates repeated setup/debug failure classes where verified recipes materially improve outcomes.

## Trace2D product rule

Owner-approved product direction:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Operational judgment rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Consequences for future implementation:

- engine-owned facts use structured/deterministic verification rather than screenshot inference,
- multimodal AI is used only for genuine perceptual/subjective review,
- final creative taste/approval remains with the user,
- Workspace is result/review/feedback oriented, not a mandatory Unity-style manual editor,
- CLI/MCP/Workspace share protocol-independent Agent/result truth,
- autonomous superiority claims require matched multi-run benchmark evidence.

## Autonomous benchmark program

Concrete benchmark growth is:

```text
#102 B0 — harness + current-capability matched tasks
 -> #103 B1 — Sprite/animation/particle matched tasks
 -> #104 B2 — coherent autonomous top-down combat micro-game
```

Primary matched environments:

```text
Godot + generic coding tools
Godot + pinned reviewed Godot MCP/agent bridge
Trace2D + public Agent surface
```

Use the same pinned agent/task where capability-eligible. Record at minimum:

- success/partial/fail,
- revision/repair iterations,
- token usage when measurable,
- total tool calls,
- screenshot/capture/video/multimodal-review calls,
- human intervention count/type,
- deterministic/semantic verification coverage,
- final unresolved failures,
- environment-labelled elapsed time.

Missing engine capability is reported separately from autonomous-agent failure. Published comparison claims require multiple trials and disclosed sample counts; one successful demo is not evidence of superiority.

## Continuation rule

For `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, `Trace2D continue`, or equivalent routine continuation:

1. read `AGENTS.md` and this file,
2. inspect live open PRs/CI and recent merges,
3. reconcile stale prose against live state and owner-approved issue contracts,
4. if an active core PR exists, finish only that implementation/gate,
5. a parallel documentation/governance PR may coexist only when it does not supersede the active implementation,
6. if the active PR has a required local hardware/environment gate, do not pretend hosted CI satisfies it,
7. once the active PR merges green, select the first incomplete/unblocked item from the fixed sequence above,
8. implement one coherent issue/child vertical slice,
9. test/validate/document it,
10. update this handoff and publish/update one scoped PR,
11. do not begin the next core child until the current one merges green.

## #51 completion contract

Issue #51 established the explicit analysis/compiler gate between CPU particle semantics and GPU runtime:

- deterministic `ParticleProgram` from validated `ParticleEffectAsset`,
- stable semantic program/artifact fingerprints,
- static feature/read/write/random-channel analysis,
- exact CPU-reference semantic operation/memory accounting,
- deterministic minimized GPU layout/artifact,
- machine-readable structural analysis plus optional environment-labelled timing,
- explicit `backend = "cpu" | "gpu"` ownership with no analyzer mutation or silent fallback.

Primary #51 contract: [`docs/PARTICLE_ANALYSIS.md`](docs/PARTICLE_ANALYSIS.md).

## #52 implementation contract

PR #95 implements:

- exact #51 compiler/artifact authority,
- SDL GPU types isolated in `engine/render`,
- persistent compiler-sized GPU particle storage and immutable program constants,
- clear/update/spawn compute paths,
- CPU-side lifecycle/emission scheduling without duplicate CPU reference simulation,
- conservative capacity-bounded render instance upper bound without normal-frame alive-count readback,
- one instanced draw per GPU emitter,
- Sprite/GPU-particle painter-order merge without global material/texture sorting,
- alpha/additive presentation and compiler-derived size/color/rotation,
- explicit unsupported-feature diagnostics with no CPU fallback,
- structural GPU metrics for layout/retained bytes/dispatch/draw/resource/synchronization evidence,
- opt-in real-GPU smoke for create -> step -> capture -> capacity reuse.

Hard invariants:

- CPU reference remains the semantic oracle,
- no duplicate full CPU particle simulation in normal GPU mode,
- no normal-frame GPU readback/fence wait,
- no normal-frame shader compilation,
- no per-particle draw-call path,
- no painter-order-violating global sort,
- GPU storage remains bounded by authored capacity/compiler stride.

After #52 merges, **#53 is next**. After #53, the new AI-operated foundation begins at **#97**, not directly at Sprite #59.

## Production architecture freeze

#85/PR #94 already froze the integration seams later work must preserve:

- external typed game components in the same Scene model,
- authoritative fixed-step state separate from interpolated presentation,
- generation-safe typed resources and explicit lifecycle/memory evidence,
- reusable scene templates/world lifecycle,
- Camera2D/Viewport2D,
- material-ready renderer compatibility,
- setup-resolved deterministic tween bindings,
- structural metrics separate from machine timing,
- tiered real-GPU conformance,
- explicit later breadth gates.

The new AI-operated roadmap extends these contracts; it does not replace them.
