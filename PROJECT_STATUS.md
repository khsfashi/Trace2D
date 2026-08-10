# Trace2D Project Status

Last repository-state update: **2026-08-11**

This file is the operational handoff for the next contributor or coding agent. Compiling code/tests, live PR/CI/merge state, explicit owner-approved contracts, and exact active issue acceptance outrank stale prose.

## Current state

Trace2D is an **AI-first / AI-operated C++20 2D engine** with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Completed AI-operated foundation so far:

- #97 machine-readable intent / Definition of Done — complete via PR #115,
- #98 unified verification / diagnosis / repair / WorkResult — complete via PR #116,
- **#99 Trace2D Workspace / human feedback loop — active in draft PR #117.**

Do not begin #102 while PR #117 is open.

PR #117 establishes the first concrete result-review Workspace baseline:

- `WorkspaceSnapshot` derives intent, deliverable progress, acceptance evidence, review queue, changes, artifacts, limitations, revision history and external-truth requirements from #97/#98 state,
- deterministic/presentation failures cannot be promoted into subjective human review,
- live clients may attach the existing protocol-independent `InspectionSnapshot` rather than inventing GUI-owned world/entity truth,
- feedback and approval are strict versioned Workspace action packets bound to the current work ID/revision,
- stale-revision actions are rejected,
- approval is accepted only for an item currently present in the review queue,
- `trace2d_workspace` exposes text/JSON review state, self-contained HTML review output, artifact links/media previews, and feedback/approval packet generation,
- committed Workspace fixtures preserve a human review -> feedback -> Agent revision -> deterministic re-verification -> revised review loop,
- Workspace parsing/rendering/action generation remains explicit tooling work outside frame hot paths.

Current #99 contracts/implementation:

- [`docs/WORKSPACE.md`](docs/WORKSPACE.md)
- [`docs/WORKSPACE_IMPLEMENTATION.md`](docs/WORKSPACE_IMPLEMENTATION.md)
- [`engine/agent/include/trace2d/agent/Workspace.hpp`](engine/agent/include/trace2d/agent/Workspace.hpp)
- [`tests/data/workspace_spec.trace2d.toml`](tests/data/workspace_spec.trace2d.toml)
- [`tests/data/workspace_result.trace2d.toml`](tests/data/workspace_result.trace2d.toml)

Hosted final-head CI is the active acceptance gate for PR #117. Do not mark #99 complete, mark PR #117 ready, merge it, or advance to #102 until that final head is green.

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
11. #52 explicit GPU particle runtime — complete via PR #95 after required local real-GPU smoke evidence
12. #53 CPU/GPU conformance, workloads and measured recommendation guidance — complete via PR #114
13. #97 machine-readable intent / Definition of Done — complete via PR #115
14. #98 unified verification / diagnosis / repair / WorkResult — complete via PR #116

Production architecture freeze #85 is complete via PR #94.

The seven-part particle program #47-#53 and umbrella #46 are closed complete after the owner-provided Windows/NVIDIA real-GPU smoke/conformance evidence and Release CPU-reference calibration were committed.

## Owner-fixed core execution order

Routine `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, or equivalent follows the first incomplete/unblocked item below and never skips an active core PR or recognized human/environment gate.

```text
AI-operated foundation
 -> #97 machine-readable intent / Definition of Done         [complete via PR #115]
 -> #98 unified verify / diagnose / repair / WorkResult      [complete via PR #116]
 -> #99 result-review Workspace / feedback loop              [active draft PR #117; hosted CI gate]
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

PR #115 keeps committed and live truth separate:

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
- a task requiring an unavailable capability is blocked/not eligible rather than counted as an Agent implementation failure,
- deterministic and presentation criteria may complete at `verified`,
- multimodal and human criteria require explicit `approved`,
- live CI/hardware/license/human facts are declared as external requirements rather than copied into stale committed state,
- work-state parsing/evaluation is explicit tooling work and never enters engine frame hot paths.

## #98 WorkResult boundary

PR #116 extends #97 identities into a revision/result flow:

```text
WorkSpec acceptance
 -> verification record
 -> structured failure + reproduction context
 -> external Agent/user repair
 -> new revision
 -> deterministic re-verification
 -> presentation/multimodal/human review where required
```

Important rules:

- engine runtime does not silently repair source/content,
- current deterministic failure remains machine-authoritative even if a screenshot looks good,
- passed/approved records require evidence references,
- human/multimodal `passed` is not final approval,
- historical failures remain reviewable after a later repair passes,
- `external_truth` remains separate from local result completion,
- `WorkResult` composes evidence but does not turn `Agent says done` into independent truth,
- #102 keeps an independent benchmark verifier/provenance boundary.

## #99 Workspace boundary

PR #117 adds a derived review client over #97/#98 rather than a second editor/project database.

```text
WorkSpec + WorkResult + optional Agent InspectionSnapshot
 -> WorkspaceSnapshot
 -> text / JSON / HTML / future client
 -> human review
 -> feedback or approval action packet
 -> external Agent/user edit
 -> deterministic re-verification
 -> next WorkResult revision
```

Important rules:

- Workspace progress is derived from explicit acceptance/result state rather than hidden model conversation,
- current machine-owned failures remain machine-owned and cannot enter the subjective review queue,
- live world/entity inspection reuses `AgentFacade::Inspect()` semantic state when a client has a running engine,
- offline file review leaves live inspection absent rather than fabricating runtime state,
- feedback may carry a stable semantic target and optional acceptance identity,
- feedback never mutates engine state by itself,
- approval is revision-bound and may only target the current eligible review queue,
- HTML/media preview consumes already-produced artifact paths and never performs continuous capture/readback,
- there is no browser framework, HTTP server, editor database or normal-frame serialization requirement.

Committed #99 representative flow:

```text
revision-1 change
 -> deterministic PASS
 -> human review + preview
 -> human feedback
 -> revision-2 change
 -> deterministic PASS again
 -> revised preview returns to review
```

After PR #117 merges green, close #99 and advance this file to **#102 Benchmark B0**.

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

- CPU reference remains semantic authority,
- backend is explicit authored text; analyzer/build never silently changes it,
- normal GPU mode does not duplicate the full CPU reference simulation,
- normal GPU frames perform no particle readback/fence wait,
- GPU storage is capacity-bounded and reused,
- no per-particle draw-call path,
- compiler/artifact/layout identity is deterministic,
- cross-vendor GPU floating point is checked through explicit tolerance/invariants, not universal float-bit identity,
- structural evidence and machine timing remain separate,
- visual style/feel may use multimodal review; final creative judgment remains human.

Primary particle contracts/evidence:

- [`docs/PARTICLES.md`](docs/PARTICLES.md)
- [`docs/PARTICLE_ANALYSIS.md`](docs/PARTICLE_ANALYSIS.md)
- [`docs/PARTICLE_GPU_RUNTIME.md`](docs/PARTICLE_GPU_RUNTIME.md)
- [`docs/PARTICLE_CONFORMANCE.md`](docs/PARTICLE_CONFORMANCE.md)
- [`docs/evidence/particle-53/924dbc1/`](docs/evidence/particle-53/924dbc1/README.md)

## AI-operated product / benchmark handoff

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

#102 is the next core task only after #99/PR #117 is green and merged.
