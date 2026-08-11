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
- #99 Trace2D Workspace / human feedback loop — complete via PR #117, merge `f45e3acf72de26c8c2e2757b75a0a221a76300e5`.

**Active core work: #102 Benchmark B0 in draft PR #118.**

Do not begin #59/#103 or later fixed-order core work while #102/PR #118 remains active or blocked on its recognized external coding-Agent gate.

## #102 active gate

PR #118 establishes the executable B0 harness, the first matched current-capability task, real qualification evidence for all three engine/adapter lanes, and a real owner-local coding-Agent wrapper/profile.

Current committed B0 state:

- three exact lanes: `godot.generic`, `godot.agent`, `trace2d.agent`,
- same prompt intent and task budget across lanes,
- first task: semantic scene authoring with stable `player` identity, `Player` name and exact `(4, 1)` position,
- public cross-engine semantic mapping is part of the common prompt rather than a hidden verifier convention,
- independent engine-side verification decides the score,
- committed known-good and meaningful known-bad fixtures validate the oracles,
- candidate trials use fresh copied workspaces/processes,
- raw trial records are append-only-at-harness SHA-256 hash-chained JSONL,
- infrastructure, implementation, eligibility, human and integrity outcomes remain separate,
- reports preserve raw counts/success rates/distributions and do not produce a weighted composite score,
- independent re-verification can rerun after the original stochastic Agent is gone,
- benchmark tooling is Python stdlib tooling outside engine runtime/frame hot paths.

### Environment/bridge qualification — complete

- `godot.generic` — official Godot `4.7.1-stable` Linux x86_64 binary checksum/version verified; known-good accepted and wrong-position known-bad rejected.
- `godot.agent` — **selected qualified baseline** `@satelliteoflove/godot-mcp@4.1.0`; hosted Godot editor/MCP qualification proved authoring, structured runtime inspection, real raw-`D` input and clean launch-frozen deterministic replay. The accepted protocol uses public `step_until` to stop on the fixture's authoritative `physics_ticks >= 12` predicate. Both clean runs stopped at exactly tick 12 with `Player.position_x == 2`; uncapped render frames differed (`267` vs `271`) and are intentionally non-authoritative. Earlier fixed-render-frame and fixed-200ms boundaries were rejected after they exposed scheduler-dependent physics progress. The independent lane oracle also accepted known-good and rejected wrong-position known-bad.
- `trace2d.agent` — frozen Trace2D source/build qualified in Windows CI; all 188 repository tests passed in the qualification run, then the independent known-good/known-bad task oracle passed.

### Coding-Agent/profile freeze — complete; real model availability — proven; isolation/three-lane qualification — pending

The real coding-Agent candidate was frozen before any scored matched-lane result existed:

- Agent: `openai-codex-cli@0.144.6`,
- auth surface: owner-local ChatGPT sign-in,
- model selector: `gpt-5.5`,
- provider revision policy: `chatgpt_codex_cli_selector_no_dated_snapshot`,
- reasoning effort: `high`,
- approval policy: `never`,
- web search: disabled,
- human interventions: zero,
- task budget: exactly the committed B0 budget.

Pre-scoring infrastructure attempts are deliberately preserved rather than rewritten as engine losses:

1. dated API snapshot `gpt-5.5-2026-04-23` — rejected by ChatGPT-managed Codex before tool use,
2. guessed `gpt-5.6-sol` — stopped before classifiable isolation evidence and exposed an observability gap that was then fixed,
3. `gpt-5.6` CLI selector — real provider reached through stdin prompt transport, but HTTP 400 reported that the model was not supported for the owner's ChatGPT Codex account; zero lane trials,
4. frozen `gpt-5.5` — **model preflight passed** with `MODEL_OK`, process code `0`, completed turn, input tokens `12589`, cached input tokens `1408`, output tokens `6`; the following filesystem-isolation child exceeded the original `90` second process ceiling before a normal isolation verdict, so no lane trial started.

The fourth attempt is preserved as [`benchmarks/b0/qualification/codex-chatgpt-gpt55-isolation-timeout-attempt-2026-08-11.json`](benchmarks/b0/qualification/codex-chatgpt-gpt55-isolation-timeout-attempt-2026-08-11.json). It is classified as `infrastructure_isolation_probe_timeout_before_verdict`, not an engine loss or isolation breach. The evidence does not establish whether the delay came from model/tool latency, native Windows sandbox setup, or another pre-verdict child-process condition.

The `gpt-5.5` model-availability gate is therefore resolved. Do not change this model after scored eligibility without explicitly superseding the benchmark version.

The recovery path now aligns the unscored isolation process ceiling with the real Agent wrapper ceiling of `285` seconds while leaving the acceptance boundary unchanged. Before any matched lane begins it must still prove all of the following:

1. normal candidate-workspace read/write succeeds,
2. Codex actually attempts an exact read of a random canary beside the held-out verifier,
3. that external read is denied,
4. the canary secret does not leak.

When available, Codex's public `.sandbox/sandbox.log` is copied into packageable `codex-sandbox-logs/` evidence before the credential-bearing transient Codex home is removed. `auth.json`, transient plugin caches and secret-bearing sandbox state remain excluded.

After isolation succeeds, owner-local qualification must preserve exactly one **unscored** calibration attempt in each of `godot.generic`, `godot.agent`, and `trace2d.agent`, with one common profile hash, provider trajectory/usage where exposed, zero human intervention and completed independent verifiers.

Until those facts are real, the suite/task intentionally remain `qualification_required` / `qualification_candidate` and `--scored` stays blocked.

Primary B0 implementation/contracts:

- [`benchmarks/b0/README.md`](benchmarks/b0/README.md)
- [`benchmarks/b0/suite.json`](benchmarks/b0/suite.json)
- [`benchmarks/b0/BASELINES.md`](benchmarks/b0/BASELINES.md)
- [`benchmarks/b0/AGENT_WRAPPER.md`](benchmarks/b0/AGENT_WRAPPER.md)
- [`benchmarks/b0/CODEX_COHORT.md`](benchmarks/b0/CODEX_COHORT.md)
- [`benchmarks/b0/qualification/README.md`](benchmarks/b0/qualification/README.md)
- [`benchmarks/b0/qualification/godot-agent.json`](benchmarks/b0/qualification/godot-agent.json)
- [`scripts/benchmark_b0.py`](scripts/benchmark_b0.py)
- [`scripts/run_benchmark_b0_codex_chatgpt_calibration_safe.py`](scripts/run_benchmark_b0_codex_chatgpt_calibration_safe.py)
- [`docs/AUTONOMOUS_BENCHMARK.md`](docs/AUTONOMOUS_BENCHMARK.md)

Three qualified environments, a frozen Agent profile and a successful standalone model preflight are readiness evidence, not a comparative result.

### Remaining gate before PR #118 may merge / #102 may close

1. complete the owner-local filesystem-isolation canary and three-lane **unscored** calibration with the frozen `gpt-5.5` profile,
2. review that evidence and only then promote the suite/task to `eligible`,
3. run the predefined repeated **scored** matched cohort with the same Agent/model/settings/budget,
4. retain every raw attempt, including losses and infrastructure outcomes,
5. independently re-verify/replay recorded result artifacts,
6. publish raw sample counts and distributions without best-of-N cherry-picking.

Do **not** manufacture model identity, token counts, provider usage, isolation evidence, or successful trials. A green repository CI run proves harness/environment contracts, not the missing owner-local isolation/lane-run facts.

## Owner-fixed core execution order

Routine `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, or equivalent follows the first incomplete/unblocked item below and never skips an active core PR or recognized human/environment gate.

```text
AI-operated foundation
 -> #97 machine-readable intent / Definition of Done         [complete via PR #115]
 -> #98 unified verify / diagnose / repair / WorkResult      [complete via PR #116]
 -> #99 result-review Workspace / feedback loop              [complete via PR #117]
 -> #102 Benchmark B0 matched harness + current-capability tasks [active draft PR #118; coding-Agent cohort gate]

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
11. #52 explicit GPU particle runtime — complete via PR #95 after required owner real-GPU evidence
12. #53 CPU/GPU conformance, workloads and measured recommendation guidance — complete via PR #114
13. #97 machine-readable intent / Definition of Done — complete via PR #115
14. #98 unified verify / diagnose / repair / WorkResult — complete via PR #116
15. #99 result-review Workspace / feedback loop — complete via PR #117

Production architecture freeze #85 is complete via PR #94.

The seven-part particle program #47-#53 and umbrella #46 are closed complete after owner-provided Windows/NVIDIA real-GPU smoke/conformance evidence and Release CPU-reference calibration were committed.

## Durable AI-operated boundaries

### #97 WorkSpec / capability boundary

```text
committed WorkSpec / capability catalog
  -> local readiness + outstanding acceptance + capability eligibility

live GitHub / CI / environment / hardware / license / human state
  -> queried by the owning orchestration stage when required
```

A capability is not inferred from a symbol merely existing. Missing capability makes a task not eligible rather than an Agent failure. Live facts remain external truth.

### #98 WorkResult boundary

```text
WorkSpec acceptance
 -> verification record
 -> structured failure + reproduction context
 -> external Agent/user repair
 -> new revision
 -> deterministic re-verification
 -> subjective review only where required
```

Agent self-report is not independent truth. Historical failures remain preserved after repair, and #102 owns an independent benchmark verifier/provenance boundary.

### #99 Workspace boundary

```text
WorkSpec + WorkResult + optional existing Agent InspectionSnapshot
 -> derived WorkspaceSnapshot
 -> result review
 -> revision-bound feedback/approval packet
 -> external Agent/user edit
 -> re-verification
 -> next WorkResult revision
```

Workspace does not create a second editor/project database, silently mutate engine state, or promote machine-owned failure into human review.

### #102 Benchmark boundary

```text
frozen task + lane + Agent/model/budget
 -> isolated fresh trial
 -> independent verifier
 -> immutable raw result record
 -> repeated matched cohort
 -> aggregate raw statistics
 -> independent re-verification/replay
```

Benchmark truth must remain independent from the candidate Agent and its WorkResult. Capability admission, infrastructure failures and human intervention remain separate from implementation success/failure.

## Particle architecture frozen by #47-#53

The V1 particle contract remains:

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

Primary particle contracts/evidence:

- [`docs/PARTICLES.md`](docs/PARTICLES.md)
- [`docs/PARTICLE_ANALYSIS.md`](docs/PARTICLE_ANALYSIS.md)
- [`docs/PARTICLE_GPU_RUNTIME.md`](docs/PARTICLE_GPU_RUNTIME.md)
- [`docs/PARTICLE_CONFORMANCE.md`](docs/PARTICLE_CONFORMANCE.md)
- [`docs/evidence/particle-53/924dbc1/README.md`](docs/evidence/particle-53/924dbc1/README.md)

## Benchmark growth after #102

```text
#102 B0 — harness + current-capability matched tasks
 -> #103 B1 — Sprite/animation/particle matched tasks
 -> #104 B2 — coherent autonomous top-down combat micro-game
```

Do not advance to #59/#103 until #102's recognized coding-Agent gate is actually resolved or the owner explicitly changes the fixed order.
