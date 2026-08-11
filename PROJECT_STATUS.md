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

Do not begin #59/#103 or later fixed-order core work while #102/PR #118 remains active.

## #102 active gate

PR #118 establishes the executable B0 harness, the first matched current-capability task, qualification evidence for all three lanes, a frozen owner-local Codex/model/isolation profile, and a preregistered repeated scored cohort.

Current B0 contract:

- exact lanes: `godot.generic`, `godot.agent`, `trace2d.agent`,
- same public task intent and frozen budget across lanes,
- task: semantic scene authoring with stable `player` identity, `Player` name and exact `(4, 1)` position,
- independent engine-side verifier decides objective acceptance,
- known-good/meaningful known-bad fixtures validate the verifiers,
- fresh candidate process/workspace per attempt,
- append-only canonical SHA-256 hash-chained raw records,
- infrastructure, implementation, budget, eligibility, human and integrity outcomes remain distinct,
- independent re-verification can run after the stochastic Agent is gone,
- no best-of-N, no favorable replacement retry.

### Environment/bridge qualification — complete

- `godot.generic` — pinned official Godot `4.7.1-stable`; independent known-good accepted and wrong-position known-bad rejected.
- `godot.agent` — selected `@satelliteoflove/godot-mcp@4.1.0`; live authoring, structured runtime state, raw input and authoritative physics-tick stepping qualified.
- `trace2d.agent` — frozen Trace2D source/build plus independent known-good/known-bad oracle qualified.

### Frozen Agent/model/isolation — complete

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
wall/tool/token budget   300 / 80 / 100000 input / 20000 output
```

Canonical Agent profile SHA-256:

```text
2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708
```

The model/backend/prompt/verifier/budget are frozen. The `100000` input-token limit was **not** raised after observing calibration usage above the ceiling.

### External Windows ACL isolation — complete

The old custom native-Windows Codex filesystem profile is permanently rejected after a real held-out canary leak.

The final boundary uses the elevated/network-disabled `CodexSandboxOffline` local identity plus external NTFS ACL. The host resolves that local account SID, applies a repository deny ACE, runs the real frozen model, and requires the exact held-out canary read to be denied with no leakage before any matched work may start. ACL cleanup is mandatory in `finally`.

The account name is not trusted as the verdict. The real-model exact-canary denial validates that the applied SID actually protects the model turn; otherwise execution fails closed before the cohort.

### Accepted unscored calibration — complete; B0 is eligible

Accepted owner archive:

```text
codex-chatgpt-calibration-20260811-163459-3812f9f7.zip
SHA-256 31d1e70938a3e98716559073518bf1e1de5465316f85bafffab4d58880e097fd
```

Evidence: [`benchmarks/b0/qualification/codex-windows-acl-unscored-calibration-accepted-2026-08-11.json`](benchmarks/b0/qualification/codex-windows-acl-unscored-calibration-accepted-2026-08-11.json).

Verified facts:

- model preflight passed,
- real ACL isolation passed,
- exact canary read attempt observed and denied,
- no canary leakage,
- ACL apply/cleanup passed for isolation and all three lane turns,
- exactly three unscored records exist,
- same frozen canonical profile hash across all records,
- canonical record hashes and previous-record chain independently recompute,
- human intervention is zero,
- provider usage and independent verifier evidence are preserved,
- no credential/raw SID/canary secret is packaged.

Calibration outcomes:

| Lane | Status | Verifier | Input tokens | Tools |
|---|---|---|---:|---:|
| `godot.generic` | `budget_exceeded` | pass | 187515 | 17 |
| `godot.agent` | `budget_exceeded` | fail (`player_missing`) | 508388 | 32 |
| `trace2d.agent` | `budget_exceeded` | pass | 195453 | 17 |

These are pre-scoring outcomes. The two verifier passes are not selected as scored wins, and the `godot.agent` failure is not repaired away. A valid calibration proves faithful measurement, not favorable performance.

`benchmarks/b0/suite.json` and `b0-semantic-scene-authoring` are now `eligible`.

### Preregistered scored cohort — ready

[`benchmarks/b0/scored-cohort-v1.json`](benchmarks/b0/scored-cohort-v1.json) was committed before eligibility and before any scored result:

```text
repetitions per lane  3
total scheduled slots 9
automatic retries     0
replacement retries   0
early stop             false
best-of-N              false
```

Order:

```text
R1  godot.generic -> godot.agent   -> trace2d.agent
R2  godot.agent   -> trace2d.agent -> godot.generic
R3  trace2d.agent -> godot.generic -> godot.agent
```

Every scheduled slot gets at most one attempt. Infrastructure and budget outcomes remain visible; no reroll replaces an unfavorable sample.

### Current owner-local gate

From an updated PR #118 checkout on native Windows, run only:

```powershell
python .\scripts\run_benchmark_b0_codex_windows_acl_scored_cohort.py
```

The runner performs:

```text
frozen gpt-5.5 preflight
 -> real elevated-Windows ACL canary
 -> exactly nine preregistered scored slots
 -> aggregate report
 -> independent reverify of all nine preserved workspaces
 -> scrubbed evidence ZIP
```

Do not manually run individual `--scored` slots and do not rerun a failed scheduled slot. If the orchestrator stops because of a genuine harness/integrity defect, preserve/upload its ZIP rather than creating an unofficial replacement sample.

### Remaining gate before PR #118 may merge / #102 may close

1. preserve the single preregistered nine-attempt scored archive,
2. verify nine raw records, nine replay records, common profile/budget identity, real isolation and ACL cleanup,
3. publish raw sample counts/status/resource distributions without broad superiority claims,
4. make PR #118 ready, merge it, close #102,
5. advance to #59 Complete Sprite program.

Hosted CI has no owner model credential. Green hosted CI proves harness/contracts, not owner-local stochastic scored outcomes.

## Owner-fixed core execution order

Routine `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, or equivalent follows the first incomplete/unblocked item below and never skips an active core PR or recognized human/environment gate.

```text
AI-operated foundation
 -> #97 machine-readable intent / Definition of Done         [complete via PR #115]
 -> #98 unified verify / diagnose / repair / WorkResult      [complete via PR #116]
 -> #99 result-review Workspace / feedback loop              [complete via PR #117]
 -> #102 Benchmark B0 matched harness + current-capability tasks [active draft PR #118; owner-local scored 9-slot cohort gate]

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

Benchmark truth remains independent from the candidate Agent and its WorkResult. Capability admission, infrastructure failures, budget failures and human intervention remain separate from implementation correctness.

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
 -> #59 Complete Sprite program
 -> #103 B1 — Sprite/animation/particle matched tasks
 -> #104 B2 — coherent autonomous top-down combat micro-game
```

Do not advance to #59/#103 until #102's scored cohort is reviewed and PR #118 is genuinely complete, unless the owner explicitly changes the fixed order.
