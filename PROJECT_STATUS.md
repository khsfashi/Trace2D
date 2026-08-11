# Trace2D Project Status

Last repository-state update: **2026-08-11**

This file is the operational handoff for the next contributor or coding agent. Live PR/CI state, compiling code/tests, explicit owner-approved contracts, and exact active issue acceptance outrank stale prose.

## Current state

Trace2D is an **AI-first / AI-operated C++20 2D engine** with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Completed AI-operated foundation:

- #97 machine-readable intent / Definition of Done — PR #115,
- #98 unified verification / diagnosis / repair / WorkResult — PR #116,
- #99 Trace2D Workspace / human feedback loop — PR #117, merge `f45e3acf72de26c8c2e2757b75a0a221a76300e5`.

**Active core work: #102 Benchmark B0 in draft PR #118.**

Do not begin #59/#103 or later fixed-order core work while #102/PR #118 remains active or blocked on its recognized owner-local benchmark gate.

## #102 B0 — current contract

B0 currently contains one intentionally narrow matched task across exactly three lanes:

```text
godot.generic
godot.agent
trace2d.agent
```

Task `b0-semantic-scene-authoring` requires one semantic `player`, name `Player`, exact position `(4, 1)`. The same conceptual prompt/budget is used in all lanes. Independent engine-side verification decides objective success; Agent self-report does not.

Durable harness rules:

- fresh copied workspace/process per trial,
- strongest qualification-passing public Godot Agent baseline rather than a deliberately weak control,
- no task-shaped Trace2D helper,
- known-good and meaningful known-bad verifier fixtures,
- append-only SHA-256 hash-chained raw JSONL,
- capability, infrastructure, implementation, human and integrity outcomes remain distinct,
- provider token usage is preserved rather than estimated,
- no best-of-N selection,
- independent re-verification can run after the stochastic Agent is gone.

Suite/task remain:

```text
suite  qualification_required
task   qualification_candidate
```

No scored B0 trial exists yet.

## Environment/bridge qualification — complete

- `godot.generic` — official Godot `4.7.1-stable`; independent known-good accepted and wrong-position known-bad rejected.
- `godot.agent` — selected baseline `@satelliteoflove/godot-mcp@4.1.0`; hosted qualification proves authoring, structured runtime state, raw input and deterministic physics-tick stepping. Authoritative stop boundary is public `step_until` on `physics_ticks >= 12`, not render frames or fixed milliseconds.
- `trace2d.agent` — frozen Trace2D source/build qualified in Windows CI with independent known-good/known-bad task oracle.

## Frozen coding-Agent/model/isolation profile

Frozen before the first matched lane result:

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
wall budget              300 s
tool-call budget         80
input-token budget       100000
output-token budget      20000
```

Canonical Agent profile SHA-256:

```text
2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708
```

Freeze evidence: [`benchmarks/b0/qualification/codex-windows-acl-final-profile-freeze-2026-08-11.json`](benchmarks/b0/qualification/codex-windows-acl-final-profile-freeze-2026-08-11.json).

The `100000` input-token limit is **not** raised after observing calibration usage. A provider turn may finish before final usage is known; if final provider usage exceeds the frozen limit, that trial is `budget_exceeded` in the implementation domain rather than a transport failure.

## Isolation history — resolved for the final backend

The original custom native-Windows Codex permission profile is permanently rejected. Real owner-local evidence showed workspace writes blocked while the exact held-out canary read succeeded and leaked. The retired `scripts/run_benchmark_b0_codex_chatgpt_calibration_safe.py` intentionally fails closed and must not be revived.

Replacement external NTFS ACL qualification then proved:

1. Codex sandbox SID differs from host SID,
2. built-in `:workspace` permits candidate writes,
3. NTFS deny for only the sandbox SID blocks an external canary read,
4. no canary leaks,
5. host access remains intact,
6. ACL cleanup succeeds.

Evidence: [`benchmarks/b0/qualification/codex-windows-acl-backend-qualified-2026-08-11.json`](benchmarks/b0/qualification/codex-windows-acl-backend-qualified-2026-08-11.json).

A first integrated attempt omitted an explicit Windows backend and failed closed on host/sandbox SID equality before any lane. Because lane count was still zero, `[windows] sandbox = "elevated"` was frozen before matched results.

A later real-model isolation turn wrote the workspace, attempted the exact held-out read, received Windows access denied, wrote `DENIED`, leaked no canary and completed. Its original verdict was a false negative only because Codex's Windows command display retained doubled backslashes. The matcher now canonicalizes only that display form; raw provider JSONL is unchanged.

## First real three-lane calibration attempt — preserved, not selected

Owner archive `codex-chatgpt-calibration-20260811-150451-625123c9.zip` reached the lane phase. Preserved summary:

[`benchmarks/b0/qualification/codex-windows-acl-unscored-calibration-harness-attempt-2026-08-11.json`](benchmarks/b0/qualification/codex-windows-acl-unscored-calibration-harness-attempt-2026-08-11.json)

The real isolation gate fully passed:

```text
sandbox SID != host SID       true
workspace write               true
held-out read attempt         observed
held-out read                 denied
canary leak                   false
ACL apply                     true
ACL cleanup                   true
Codex turn                    completed
```

Lane evidence:

- `godot.generic` — full Agent turn and independent verifier **PASS** at `player / Player / (4, 1)`; provider input `149255` exceeded frozen `100000` budget. Old wrapper incorrectly labeled this transport failure.
- `trace2d.agent` — full Agent turn and independent verifier **PASS** at the same semantic result; provider input `279614` exceeded the same frozen budget and was likewise misclassified.
- `godot.agent` — no raw lane record. Sandbox SID discovery timed out after Godot editor/MCP startup; afterward engine-owned `.godot/shader_cache` changed while recursive artifact hashing ran and caused `FileNotFoundError`.

The two verifier-pass artifacts are historical pre-scoring evidence only. They are not cherry-picked into the final cohort.

## Corrections committed before the final unscored rerun

The frozen model, task, prompt, verifier, backend and budget did not change.

### Pre-editor sandbox identity

`scripts/benchmark_b0_codex_windows_acl_wrapper.py` now discovers host/sandbox identity during `CODEX_HOME` setup **before** Godot editor startup, keeps the raw identity only in-process, and reuses the exact identity for the ACL-guarded model turn. This removes the redundant live-editor `codex sandbox whoami` boundary exposed by the failed `godot.agent` attempt.

### Stable authored-artifact hash

Owner-local matched runs are routed through `scripts/benchmark_b0_stable_harness.py` with policy:

```text
authored_files_excluding_godot_cache_v1
```

Engine-owned `.godot` cache is not candidate-authored state and may change/disappear asynchronously after Godot exits, so it is excluded from artifact identity. Authored files still fail closed if they disappear while hashing.

### Budget outcome taxonomy

A completed provider turn whose final provider-reported resource usage exceeds the frozen task limit is now:

```text
budget_exceeded -> implementation
```

not:

```text
tool_transport_failure -> infrastructure
```

The budget itself remains exactly unchanged.

Hosted `B0 Codex Wrapper` CI tests all three corrections plus the exact frozen `100000` input-token limit.

## Final unscored owner-local gate

After updating PR #118, use only:

```powershell
python .\scripts\run_benchmark_b0_codex_windows_acl_calibration.py
```

The runner performs:

```text
gpt-5.5 model preflight
 -> real elevated-Windows ACL canary
 -> godot.generic   exactly one fresh unscored attempt
 -> godot.agent     exactly one fresh unscored attempt
 -> trace2d.agent   exactly one fresh unscored attempt
 -> aggregate report + scrubbed ZIP
```

A `budget_exceeded` record is a valid preserved benchmark outcome. A true infrastructure failure remains separately classified. No silent retry or best-of-N selection is allowed.

If the archive contains positive isolation plus exactly three structurally valid lane records with the common frozen profile/budget and independent verifier outputs, suite/task may be promoted to `eligible` even if one or more lanes lose by verifier/budget/timeout. Benchmark losses are evidence, not a reason to retune the benchmark.

## Scored cohort policy — preregistered before scored results

[`benchmarks/b0/scored-cohort-v1.json`](benchmarks/b0/scored-cohort-v1.json) freezes the B0 scored cohort before eligibility or any scored result:

```text
repetitions per lane  3
total planned trials  9
automatic retries     0
replacement retries   0
early stop             false
best-of-N              false
```

Lane order rotates deterministically by repetition so no lane is always first/last:

```text
R1  godot.generic -> godot.agent   -> trace2d.agent
R2  godot.agent   -> trace2d.agent -> godot.generic
R3  trace2d.agent -> godot.generic -> godot.agent
```

Every scheduled slot gets at most one attempt. Infrastructure failures remain visible rather than being rerolled. This is intentionally small because B0 is a harness-integrity milestone with one narrow task, not a publication-grade general engine benchmark. Later B1/B2 suites can expand sample size/task breadth.

## Remaining gate before PR #118 may merge / #102 may close

1. receive and review one complete corrected three-lane unscored archive,
2. if structurally valid, promote suite/task to `eligible`,
3. run the preregistered 9-trial scored cohort without retry/early stop,
4. retain every outcome and verify common profile/budget/integrity,
5. independently reverify/replay preserved artifacts where applicable,
6. publish raw sample counts/status/resource distributions without broad superiority claims,
7. make PR #118 ready, merge it, close #102,
8. advance fixed order to #59 Complete Sprite program.

Hosted CI has no owner model credential. Green CI proves repository/harness contracts, not owner-local stochastic model outcomes.

## Owner-fixed core execution order

Routine `@GitHub Trace2D 다음 진행해줘`, `Trace2D next`, or equivalent follows the first incomplete/unblocked item below and never skips an active core PR or recognized human/environment gate.

```text
AI-operated foundation
 -> #97 machine-readable intent / Definition of Done         [complete via PR #115]
 -> #98 unified verify / diagnose / repair / WorkResult      [complete via PR #116]
 -> #99 result-review Workspace / feedback loop              [complete via PR #117]
 -> #102 Benchmark B0 matched harness + current-capability tasks [active draft PR #118; final owner-local unscored gate]

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

Umbrellas/registers: #13, #96, #100, #59, #67, #85, #93, #101, #106. These do not authorize routine continuation to bypass the fixed order.

## Completed foundation and particle sequence

1. #40 deterministic texture asset cache/import — PR #45
2. #42 text/basic UI — PR #55
3. #43 semantic UI tree/Agent interaction — PR #56
4. #39 MCP transport over Agent/Testing — PR #58
5. #41 reproducible renderer workloads — PR #63
6. #47 particle deterministic frame/keyed-random contracts — PR #64
7. #48 rich deterministic CPU particle reference — PR #65
8. #49 text-authored effects + `ParticleEmitter2D` — PR #66
9. #50 complete Agent verification over CPU particle reference state — PR #83
10. #51 CPU cost analysis + explicit backend ownership + deterministic compiler — PR #84
11. #52 explicit GPU particle runtime — PR #95 after owner real-GPU evidence
12. #53 CPU/GPU conformance/workloads/recommendation guidance — PR #114
13. #97 WorkSpec / Definition of Done — PR #115
14. #98 WorkResult / verify-diagnose-repair — PR #116
15. #99 Workspace / result-review feedback loop — PR #117

Production architecture freeze #85 is complete via PR #94. Particle program #47-#53 and umbrella #46 are closed after owner Windows/NVIDIA real-GPU smoke/conformance evidence and Release CPU-reference calibration.

## Durable AI-operated boundaries

### #97 WorkSpec

```text
committed WorkSpec / capability catalog
 -> local readiness + outstanding acceptance + capability eligibility
```

Capability is not inferred from a symbol merely existing. Missing capability is `not eligible`, not an Agent failure.

### #98 WorkResult

```text
WorkSpec acceptance
 -> deterministic verification
 -> structured failure/reproduction
 -> external Agent/user repair
 -> new revision
 -> re-verification
```

Agent self-report is not independent truth.

### #99 Workspace

```text
WorkSpec + WorkResult + optional InspectionSnapshot
 -> derived WorkspaceSnapshot
 -> review/feedback packet
 -> external edit
 -> re-verification
```

Workspace does not create a second project database or silently mutate authoritative engine state.

### #102 Benchmark

```text
frozen task + lane + Agent/model/budget
 -> isolated fresh trial
 -> independent verifier
 -> immutable raw record
 -> preregistered repeated cohort
 -> raw aggregate distributions
 -> independent re-verification/replay
```

Capability admission, infrastructure, budget, implementation and human outcomes remain separate.

## Particle architecture frozen by #47-#53

```text
rich text-authored effect
 -> deterministic CPU semantic oracle
 -> exact structured Agent verification
 -> deterministic structural cost analysis
 -> optional environment-labelled Release timing
 -> explicit human CPU/GPU backend decision
 -> deterministic minimized GPU artifact
 -> persistent GPU compute/instanced presentation
 -> layered CPU/GPU conformance
```

Primary contracts: `docs/PARTICLES.md`, `docs/PARTICLE_ANALYSIS.md`, `docs/PARTICLE_GPU_RUNTIME.md`, `docs/PARTICLE_CONFORMANCE.md`.

## Benchmark growth after #102

```text
#102 B0 — harness + current-capability matched task
 -> #59 Complete Sprite program
 -> #103 B1 — Sprite/animation/particle matched tasks
 -> #104 B2 — coherent autonomous top-down combat micro-game
```

Do not advance to #59 until #102's recognized gate is actually resolved or the owner explicitly changes the fixed order.
