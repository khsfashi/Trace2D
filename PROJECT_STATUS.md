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

Do not begin #59/#103 or later fixed-order core work while #102/PR #118 remains active or blocked on its recognized owner-local coding-Agent gate.

## #102 active gate

PR #118 establishes the executable B0 harness, first matched current-capability task, real qualification evidence for all three engine/adapter lanes, and a real owner-local coding-Agent/model/isolation profile.

Current B0 contract:

- exact lanes: `godot.generic`, `godot.agent`, `trace2d.agent`,
- same prompt intent and task budget across lanes,
- first task: semantic scene authoring with stable `player` identity, `Player` name and exact `(4, 1)` position,
- public cross-engine semantic mapping is part of the common prompt rather than a hidden verifier convention,
- independent engine-side verification decides the score,
- known-good and meaningful known-bad fixtures validate the oracles,
- candidate trials use fresh copied workspaces/processes,
- raw trial records are append-only SHA-256 hash-chained JSONL,
- infrastructure, implementation, eligibility, human and integrity outcomes remain separate,
- reports preserve raw counts/success rates/distributions and do not produce a weighted composite score,
- independent re-verification can rerun after the stochastic Agent is gone,
- no best-of-N selection.

### Environment/bridge qualification — complete

- `godot.generic` — pinned official Godot `4.7.1-stable`; independent known-good accepted and wrong-position known-bad rejected.
- `godot.agent` — selected qualified baseline `@satelliteoflove/godot-mcp@4.1.0`; hosted editor/MCP qualification proves authoring, structured runtime inspection, raw input and deterministic physics-tick stepping. The accepted stop boundary is public `step_until` on authoritative `physics_ticks >= 12`, not render-frame count or fixed milliseconds.
- `trace2d.agent` — frozen Trace2D source/build qualified in Windows CI; independent known-good/known-bad task oracle passed.

### Coding-Agent/model/isolation freeze — complete before lane zero

The real coding-Agent candidate is frozen to:

```text
Agent                    openai-codex-cli@0.144.6
auth                     owner-local ChatGPT sign-in
model                    gpt-5.5
provider revision policy chatgpt_codex_cli_selector_no_dated_snapshot
reasoning                high
approval                 never
permission profile       :workspace
Windows sandbox backend  elevated
isolation backend        windows_ntfs_acl_v1_elevated
protected-root policy    repository-root deny for Codex sandbox SID
shell network            disabled
session persistence      ephemeral
human intervention       0
budget                   exact committed B0 task budget
```

Canonical Agent profile SHA-256:

```text
2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708
```

Freeze evidence: [`benchmarks/b0/qualification/codex-windows-acl-final-profile-freeze-2026-08-11.json`](benchmarks/b0/qualification/codex-windows-acl-final-profile-freeze-2026-08-11.json).

Owner-local preflight proves `gpt-5.5` is callable. The frozen input-token ceiling remains `100000`; it is not raised after observing larger calibration usage.

### External Windows ACL isolation — qualified and real-model proven

The original custom native-Windows Codex permission profile is permanently rejected after a real held-out canary leak. The replacement external NTFS ACL mechanism is qualified and a later real `gpt-5.5` canary turn proved the final elevated boundary:

- sandbox identity differed from the host,
- workspace write succeeded,
- exact held-out canary read was attempted,
- Windows denied the read,
- no canary secret leaked,
- ACL apply and cleanup succeeded,
- Codex completed normally.

Historical integration failures remain pre-scoring evidence rather than engine losses. See [`benchmarks/b0/qualification/README.md`](benchmarks/b0/qualification/README.md).

### First real matched calibration exposure

Owner archive `codex-chatgpt-calibration-20260811-150451-625123c9.zip` passed real-model isolation and reached the lane phase.

Observed:

- `godot.generic` authored the required `player / Player / (4, 1)` result and the independent verifier passed, but provider input usage `149255` exceeded the frozen `100000` budget;
- `trace2d.agent` independently verified the same semantic result, but input usage `279614` exceeded the same frozen budget;
- `godot.agent` exposed two pre-scoring harness defects before a record could be appended: sandbox SID rediscovery timed out after editor/MCP startup, then volatile `.godot/shader_cache` changed while recursive workspace hashing ran.

Those favorable verifier-pass artifacts are retained only as historical evidence and are not selected into the final calibration cohort.

Corrections committed without changing model/prompt/task/backend/budget:

- owner-local artifact identity is `authored_files_excluding_godot_cache_v1`, so engine-owned `.godot` cache is excluded;
- completed provider turns over the frozen budget are `budget_exceeded` implementation outcomes, not infrastructure transport failures;
- no result-driven budget increase is allowed.

### Latest owner-local attempt — auxiliary SID subprocess removed

Owner archive `codex-chatgpt-calibration-20260811-154315-31558452.zip` passed the `gpt-5.5` model preflight but stopped before ACL application or any lane because the auxiliary command used only to rediscover the sandbox SID,

```text
codex sandbox --permission-profile :read-only --cd <workspace> -- whoami /user /fo csv /nh
```

timed out after 60 seconds. Zero matched lane records and zero scored results were produced. Evidence is preserved as [`benchmarks/b0/qualification/codex-windows-acl-unscored-isolation-sid-discovery-timeout-2026-08-11.json`](benchmarks/b0/qualification/codex-windows-acl-unscored-isolation-sid-discovery-timeout-2026-08-11.json), classification `infrastructure_sandbox_identity_discovery_timeout`.

The final wrapper no longer launches that redundant sandbox process. The already-qualified elevated/network-disabled identity `CodexSandboxOffline` is resolved from the host Windows account database before model/editor startup, and its raw SID remains in-process only. The real-model exact-canary gate is still authoritative: if the resolved SID is not the effective model identity, the deny ACE will not block the canary and no lane may start.

Relevant implementation:

- [`scripts/benchmark_b0_codex_windows_acl_wrapper.py`](scripts/benchmark_b0_codex_windows_acl_wrapper.py)
- [`scripts/benchmark_b0_stable_harness.py`](scripts/benchmark_b0_stable_harness.py)
- [`scripts/run_benchmark_b0_codex_windows_acl_calibration.py`](scripts/run_benchmark_b0_codex_windows_acl_calibration.py)
- [`benchmarks/b0/CODEX_COHORT.md`](benchmarks/b0/CODEX_COHORT.md)
- [`benchmarks/b0/qualification/README.md`](benchmarks/b0/qualification/README.md)

### Preregistered scored cohort

Before any scored result and before eligibility, B0 freezes [`benchmarks/b0/scored-cohort-v1.json`](benchmarks/b0/scored-cohort-v1.json):

```text
repetitions per lane  3
total planned trials  9
automatic retries     0
replacement retries   0
early stop             false
best-of-N              false
```

The three repetitions use a deterministic rotating lane order so no lane is always first or last. Infrastructure outcomes remain visible; there is no favorable reroll.

### Current owner-local gate

From an updated PR #118 checkout on native Windows, run only:

```powershell
python .\scripts\run_benchmark_b0_codex_windows_acl_calibration.py
```

The runner performs:

```text
gpt-5.5 model preflight
 -> host-resolve CodexSandboxOffline SID
 -> real elevated-Windows ACL isolation canary
 -> only on positive isolation verdict:
    godot.generic   exactly one fresh unscored attempt
    godot.agent     exactly one fresh unscored attempt
    trace2d.agent   exactly one fresh unscored attempt
 -> aggregate report + scrubbed evidence ZIP
```

A valid `budget_exceeded` lane remains a benchmark outcome. Do not run a scored benchmark manually yet.

### Remaining gate before PR #118 may merge / #102 may close

1. preserve one corrected archive with positive real-model isolation and exactly three structurally valid unscored lane records,
2. review ACL cleanup, common frozen profile/budget identity, provider trajectory/usage and independent verifiers,
3. promote suite/task to `eligible`,
4. run exactly the preregistered nine scored trials with no retry/early stop,
5. retain every attempt including losses and infrastructure outcomes,
6. independently re-verify/replay artifacts and publish raw sample counts/status/resource distributions,
7. make PR #118 ready, merge it, close #102, then advance to #59 Complete Sprite program.

Do **not** manufacture model identity, token counts, provider usage, isolation evidence, or successful trials. Hosted CI has no owner model credential; green CI proves harness/contracts, not missing owner-local model/lane facts.

## Owner-fixed core execution order

Routine `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, or equivalent follows the first incomplete/unblocked item below and never skips an active core PR or recognized human/environment gate.

```text
AI-operated foundation
 -> #97 machine-readable intent / Definition of Done         [complete via PR #115]
 -> #98 unified verify / diagnose / repair / WorkResult      [complete via PR #116]
 -> #99 result-review Workspace / feedback loop              [complete via PR #117]
 -> #102 Benchmark B0 matched harness + current-capability tasks [active draft PR #118; owner-local unscored cohort gate]

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
