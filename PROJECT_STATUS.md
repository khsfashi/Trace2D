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

Owner-local preflight proves `gpt-5.5` is callable: `MODEL_OK`, process code `0`, completed turn and provider usage were preserved. Do not change model/backend/budget based on later lane outcomes.

### Rejected native-Windows Codex permission profile

The original custom Codex filesystem permission profile is permanently rejected. Real owner-local evidence showed:

- workspace reads worked,
- workspace writes were blocked by policy,
- the exact random held-out canary read executed,
- that read succeeded,
- the canary value became model-visible.

Evidence: [`benchmarks/b0/qualification/codex-chatgpt-native-windows-isolation-breach-2026-08-11.json`](benchmarks/b0/qualification/codex-chatgpt-native-windows-isolation-breach-2026-08-11.json), classification `integrity_isolation_breach_native_windows_profile`.

The retired entrypoint `scripts/run_benchmark_b0_codex_chatgpt_calibration_safe.py` intentionally fails closed and must not be revived.

### External Windows ACL mechanism — qualified

The replacement boundary moves held-out protection outside the model-facing permission policy. Model-free owner-local qualification proved:

1. Codex Windows sandbox SID differs from the host SID,
2. built-in `:workspace` can write the candidate workspace,
3. a temporary NTFS deny ACE for the sandbox SID prevents reading an external random canary,
4. no canary content leaks,
5. host access remains intact,
6. ACL and canary cleanup succeed.

Evidence: [`benchmarks/b0/qualification/codex-windows-acl-backend-qualified-2026-08-11.json`](benchmarks/b0/qualification/codex-windows-acl-backend-qualified-2026-08-11.json).

The first integrated runner attempt used a clean `CODEX_HOME` without explicitly pinning the Windows sandbox backend. SID discovery returned the host SID, so the wrapper failed closed before ACL application, real model isolation or any matched lane. This is preserved as [`benchmarks/b0/qualification/codex-windows-acl-integrated-backend-unspecified-attempt-2026-08-11.json`](benchmarks/b0/qualification/codex-windows-acl-integrated-backend-unspecified-attempt-2026-08-11.json).

Because matched lane count was still zero, `[windows] sandbox = "elevated"` was frozen before any lane result existed.

### Latest real-model isolation — boundary succeeded; matcher false negative fixed

Owner archive `codex-chatgpt-calibration-20260811-141748-a87da13f.zip` reached the final elevated-Windows ACL model isolation turn.

Observed facts:

- `gpt-5.5` model preflight passed,
- elevated sandbox identity was distinct from host identity,
- workspace write succeeded,
- the exact held-out canary read was actually attempted,
- Windows rejected it with `UnauthorizedAccessException` / access denied,
- Agent wrote `DENIED`,
- canary secret leakage was false,
- Codex completed normally with provider usage,
- matched lane trials started: `0`,
- scored results: `0`.

The run nevertheless failed closed because the old attempt matcher searched for the normal single-backslash Windows path while Codex's command display retained doubled backslashes. This is an infrastructure evidence-matcher false negative, not an isolation failure or engine result.

Evidence: [`benchmarks/b0/qualification/codex-windows-acl-integrated-command-matcher-false-negative-2026-08-11.json`](benchmarks/b0/qualification/codex-windows-acl-integrated-command-matcher-false-negative-2026-08-11.json), classification `infrastructure_isolation_evidence_matcher_windows_escape_false_negative`.

The final wrapper now:

- canonicalizes doubled Windows backslashes **only for the isolation command-path matcher**,
- preserves the raw Codex JSONL trajectory unchanged,
- exports scrubbed `acl-isolation.json` outside package-excluded `.probe-artifacts`,
- retains the same frozen model, prompt, task, budget, permission profile, elevated Windows backend and ACL policy.

Relevant implementation:

- [`scripts/benchmark_b0_codex_windows_acl_wrapper.py`](scripts/benchmark_b0_codex_windows_acl_wrapper.py)
- [`scripts/run_benchmark_b0_codex_windows_acl_calibration.py`](scripts/run_benchmark_b0_codex_windows_acl_calibration.py)
- [`benchmarks/b0/CODEX_COHORT.md`](benchmarks/b0/CODEX_COHORT.md)
- [`benchmarks/b0/qualification/README.md`](benchmarks/b0/qualification/README.md)

### Current owner-local gate

From an updated PR #118 checkout on native Windows, run only:

```powershell
python .\scripts\run_benchmark_b0_codex_windows_acl_calibration.py
```

The runner performs:

```text
gpt-5.5 model preflight
 -> real elevated-Windows ACL isolation canary
 -> only on positive isolation verdict:
    godot.generic   exactly one unscored attempt
    godot.agent     exactly one unscored attempt
    trace2d.agent   exactly one unscored attempt
 -> aggregate report + scrubbed evidence ZIP
```

A lane failure is preserved and does not prevent later lanes from running. Do not run a scored benchmark yet.

### Remaining gate before PR #118 may merge / #102 may close

1. preserve one corrected archive with positive real-model isolation and exactly three unscored lane records,
2. review canary denial/no leakage, packageable per-turn ACL cleanup, common frozen profile identity, provider trajectory/usage and independent verifiers,
3. promote suite/task to `eligible`,
4. run the predefined repeated **scored** matched cohort with the exact same setup,
5. retain every attempt including losses and infrastructure outcomes,
6. independently re-verify/replay artifacts,
7. publish raw sample counts/distributions without cherry-picking,
8. only then make PR #118 ready/merge and close #102.

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
