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
- #99 Trace2D Workspace / human feedback loop — complete via PR #117,
- #102 Benchmark B0 acceptance — **satisfied in PR #118; pending final green merge**.

**Next fixed-order core work after PR #118 merges: #59 Complete Sprite program.**

Do not start #103 or later fixed-order work before #59.

## #102 Benchmark B0 — acceptance satisfied

PR #118 now contains an end-to-end matched benchmark harness and real owner-local evidence across:

```text
godot.generic
godot.agent
trace2d.agent
```

Frozen matched task:

```text
semantic scene authoring
entity id = player
name      = Player
position  = (4, 1)
```

Frozen coding-Agent profile:

```text
Agent                    openai-codex-cli@0.144.6
model                    gpt-5.5
reasoning                high
approval                 never
permission profile       :workspace
Windows sandbox backend  elevated
isolation backend        windows_ntfs_acl_v1_elevated
shell network            disabled
human intervention       0
wall/tool/token budget   300 / 80 / 100000 input / 20000 output
```

Canonical Agent profile SHA-256:

```text
2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708
```

### Isolation

The original native-Windows Codex custom filesystem profile is permanently rejected after a real held-out canary leak.

The accepted replacement is the elevated Codex Windows sandbox identity plus external NTFS ACL. Before matched work the host applies a repository deny ACE to the effective Codex sandbox SID, a real frozen-model turn attempts the exact held-out canary read, and the run may proceed only if Windows denies that read with no leakage. ACL cleanup is mandatory and fail-closed.

### Accepted unscored calibration

```text
codex-chatgpt-calibration-20260811-163459-3812f9f7.zip
SHA-256 31d1e70938a3e98716559073518bf1e1de5465316f85bafffab4d58880e097fd
```

Evidence:

[`benchmarks/b0/qualification/codex-windows-acl-unscored-calibration-accepted-2026-08-11.json`](benchmarks/b0/qualification/codex-windows-acl-unscored-calibration-accepted-2026-08-11.json)

The calibration established eligibility without raising the frozen budget after observing over-budget usage.

### Accepted preregistered scored cohort

```text
codex-chatgpt-scored-20260811-180458-214dfeb0.zip
SHA-256 0625e084b6704258a537de7005a3f9a427d66147663abc7878d5880b2860ea52
```

Evidence:

- [`benchmarks/b0/qualification/codex-windows-acl-scored-cohort-accepted-2026-08-11.json`](benchmarks/b0/qualification/codex-windows-acl-scored-cohort-accepted-2026-08-11.json)
- [`benchmarks/b0/RESULTS.md`](benchmarks/b0/RESULTS.md)

Preregistered policy remained unchanged:

```text
repetitions per lane  3
total scheduled slots 9
automatic retries     0
replacement retries   0
early stop             false
best-of-N              false
```

All nine scheduled attempts exist in the predefined rotating order. No failed or unfavorable slot was replaced.

Authoritative benchmark status:

| Lane | Samples | `budget_exceeded` | Benchmark success |
|---|---:|---:|---:|
| `godot.generic` | 3 | 3 | 0/3 |
| `godot.agent` | 3 | 3 | 0/3 |
| `trace2d.agent` | 3 | 3 | 0/3 |

Every provider turn exceeded the frozen `100000` input-token ceiling. The ceiling is not rewritten after observing the outcome.

Independent verifier outcomes are retained separately:

| Lane | Verifier pass | Verifier fail |
|---|---:|---:|
| `godot.generic` | 1/3 | 2/3 (`player_missing`) |
| `godot.agent` | 3/3 | 0/3 |
| `trace2d.agent` | 3/3 | 0/3 |

Cohort integrity review passed:

- model preflight passed,
- real held-out read attempted and denied with no canary leakage,
- isolation + nine trial ACL records all applied and cleaned up (`10/10`),
- nine scored raw records and nine replay records exist,
- frozen Agent profile hash is common to all nine,
- human intervention is zero in all nine,
- raw and replay SHA chains independently recompute,
- all nine independent reverify subprocesses returned zero,
- all nine workspace hashes and verifier verdicts match their originals,
- packaged evidence contains no auth file, obvious API key, plaintext canary, raw Windows SID or bearer token.

Godot trajectories also preserve six `signal 11` crash events across five Godot trials. They are not hidden or rerolled; the cohort still completed all final verifiers and all final workspaces/verdicts reverified exactly.

B0 therefore proves the required benchmark methodology and evidence quality. It does **not** establish broad Trace2D-over-Godot superiority from one narrow three-sample task.

## Owner-fixed core execution order

Routine `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, or equivalent follows the first incomplete/unblocked item below.

```text
AI-operated foundation
 -> #97 machine-readable intent / Definition of Done         [complete via PR #115]
 -> #98 unified verify / diagnose / repair / WorkResult      [complete via PR #116]
 -> #99 result-review Workspace / feedback loop              [complete via PR #117]
 -> #102 Benchmark B0 matched harness + current tasks        [acceptance satisfied in PR #118; merge pending]

Content production
 -> #59 complete Sprite program                              [next after PR #118 merge]
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
16. #102 Benchmark B0 matched harness/evidence — acceptance satisfied in PR #118

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

Agent self-report is not independent truth. Historical failures remain preserved after repair.

### #99 Workspace boundary

```text
WorkSpec + WorkResult + optional Agent InspectionSnapshot
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
 -> append-only Agent trajectory
 -> independent verifier
 -> immutable raw result record
 -> preregistered repeated matched cohort
 -> aggregate raw statistics
 -> independent re-verification/replay
```

Benchmark truth remains independent from the candidate Agent. Capability admission, infrastructure failures, budget failures, verifier correctness and human intervention remain separate dimensions.

## Particle architecture frozen by #47-#53

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

## Benchmark growth

```text
#102 B0 — harness + current-capability matched task [complete after PR #118 merge]
 -> #59 Complete Sprite program
 -> #103 B1 — Sprite/animation/particle matched tasks
 -> #104 B2 — coherent autonomous top-down combat micro-game
```
