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

PR #118 establishes the executable B0 harness, the first matched current-capability task, real qualification evidence for all three engine/adapter lanes, and a real owner-local coding-Agent/model profile.

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

### Coding-Agent/model freeze — complete; model availability — proven

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

Owner-local model preflight has proved `gpt-5.5` is callable: `MODEL_OK`, process code `0`, completed turn and provider token usage were all preserved. Do not change this model after scored eligibility without explicitly superseding the benchmark version.

The Agent profile now records `permission_profile = qualification_pending_external_isolation_backend`. This is intentional: the previous native-Windows Codex permission profile was rejected, so the final isolation setting/profile hash must be frozen only after the replacement mechanism is independently qualified, but still **before the first matched lane trial**.

### Native-Windows Codex read-deny backend — rejected

The latest owner-local isolation attempt is decisive. With the frozen `gpt-5.5` Agent and the 285-second probe ceiling:

- the model completed normally,
- read/list operations inside the candidate workspace worked,
- workspace write attempts were rejected by Codex policy,
- Codex executed the exact read against the random canary beside the held-out verifier,
- that external read **succeeded**,
- the random canary value became model-visible,
- no matched lane trial started and no engine result exists.

This is preserved as [`benchmarks/b0/qualification/codex-chatgpt-native-windows-isolation-breach-2026-08-11.json`](benchmarks/b0/qualification/codex-chatgpt-native-windows-isolation-breach-2026-08-11.json), classified as `integrity_isolation_breach_native_windows_profile`.

Therefore the attempted custom Codex native-Windows permission profile is **not a valid B0 isolation backend**. Do not tune the prompt, increase the timeout, weaken the canary rule, or rerun the old calibration path until it happens to pass. The integrity boundary must change first.

The result is consistent with an upstream native-Windows deny-read report (`openai/codex#31265`), but Trace2D's decision rests on its own observed owner-local canary leak.

### Replacement isolation backend candidate — external Windows ACL

The next candidate moves held-out protection outside Codex's model-facing read-deny profile. [`scripts/qualify_benchmark_b0_windows_acl_isolation.py`](scripts/qualify_benchmark_b0_windows_acl_isolation.py) is a **model-free, engine-free** mechanism probe that operates only on throwaway local directories.

It must prove:

1. Codex's native Windows sandbox runs under a SID distinct from the host user,
2. Codex's built-in `:workspace` profile can write inside the candidate workspace,
3. a temporary NTFS deny ACE applied only to the sandbox SID prevents reading a random external canary,
4. the canary value does not leak,
5. the host/orchestrator retains access and the temporary ACE is cleaned up.

A passing probe qualifies only the mechanism. The full calibration runner may adopt it only after the evidence is reviewed and a repo/harness quarantine lifecycle with cleanup/failure recovery is committed.

Until then:

- do **not** run `run_benchmark_b0_codex_chatgpt_calibration_safe.py`; that entrypoint now fails closed,
- do **not** start any matched lane trial,
- suite/task remain `qualification_required` / `qualification_candidate`,
- `--scored` remains blocked.

Primary B0 implementation/contracts:

- [`benchmarks/b0/README.md`](benchmarks/b0/README.md)
- [`benchmarks/b0/suite.json`](benchmarks/b0/suite.json)
- [`benchmarks/b0/BASELINES.md`](benchmarks/b0/BASELINES.md)
- [`benchmarks/b0/AGENT_WRAPPER.md`](benchmarks/b0/AGENT_WRAPPER.md)
- [`benchmarks/b0/CODEX_COHORT.md`](benchmarks/b0/CODEX_COHORT.md)
- [`benchmarks/b0/qualification/README.md`](benchmarks/b0/qualification/README.md)
- [`benchmarks/b0/qualification/godot-agent.json`](benchmarks/b0/qualification/godot-agent.json)
- [`scripts/benchmark_b0.py`](scripts/benchmark_b0.py)
- [`scripts/qualify_benchmark_b0_windows_acl_isolation.py`](scripts/qualify_benchmark_b0_windows_acl_isolation.py)
- [`docs/AUTONOMOUS_BENCHMARK.md`](docs/AUTONOMOUS_BENCHMARK.md)

Three qualified environments and a proven real model are readiness evidence, not a comparative result.

### Remaining gate before PR #118 may merge / #102 may close

1. qualify a replacement hard isolation backend; current native Codex read-deny backend is rejected,
2. integrate the qualified boundary into the owner-local runner with safe cleanup/recovery and freeze its exact profile hash,
3. preserve exactly one three-lane **unscored** calibration with the frozen Agent/model/settings/budget,
4. review canary denial/no leakage, provider trajectory/usage, common profile identity and independent verifiers,
5. only then promote suite/task to `eligible`,
6. run the predefined repeated **scored** matched cohort,
7. retain every attempt including losses and infrastructure outcomes,
8. independently re-verify/replay artifacts and publish raw sample counts/distributions without best-of-N cherry-picking.

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
