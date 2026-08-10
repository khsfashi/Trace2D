# Trace2D Agent Operating Guide

This file is the entry point for any coding agent working in Trace2D.

The goal is that a fresh agent can open the repository, recover live project state, select the correct next task, implement it, validate it, and leave a deterministic handoff without relying on previous chat history.

## Project identity

Trace2D is an **AI-first, AI-operated C++20 2D game engine**.

Canonical product statement:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Short product line:

> **Tell AI what to build. Review the result.**

Trace2D is not a conventional editor-first engine with an AI chat panel bolted on. Supported workflows should progressively allow an agent to author/modify, build/import, run, inspect, verify, diagnose, repair, re-verify and present results without depending on hidden editor-only state.

Core authority rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

When Trace2D owns machine-readable truth, use structured/deterministic verification. Use multimodal review only for genuinely perceptual or subjective questions. Creative direction, taste, fun, tradeoff acceptance and final approval remain human decisions.

Current low-level workflow:

```text
edit authored/source state
 -> build/import
 -> run headlessly when possible
 -> inspect/query semantic state
 -> inject virtual input/actions
 -> step exact frames
 -> assert authoritative behavior
 -> profile/analyze explicitly when relevant
 -> capture pixels/audio only when presentation matters
```

Long-term product loop:

```text
human intent
 -> AI plan/author/generate
 -> build/import/normalize
 -> deterministic run/inspect/verify
 -> presentation evidence
 -> multimodal review only where necessary
 -> AI diagnose/repair/re-verify
 -> WorkResult / Workspace
 -> human review/feedback/approval
 -> requested revision back to AI
```

Generation or other external creative inputs may be nondeterministic. Imported canonical state, runtime semantics, assertions, migration, structural metrics and reproducible validation should be deterministic/machine-readable wherever practical.

## Required reading order

Before changing code:

1. `AGENTS.md`
2. `PROJECT_STATUS.md`
3. exact active issue / PR
4. active subsystem contract
5. `docs/COMMIT_KNOWLEDGE.md` for substantive work that may be shaped by prior decisions, constraints, rejected approaches, validation gaps or human gates
6. `docs/EXTERNAL_REFERENCE_PROTOCOL.md` for substantive subsystem/architecture/determinism/benchmark/performance/integration work
7. `docs/REPOSITORY_AUTOMATION.md` for repository self-description, readiness, environment diagnostics, capability metadata, contract auditing, security or release-provenance work
8. `docs/AI_OPERATED_WORKFLOW.md` for product/verification/human-feedback rules
9. `docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md` when work can affect world/components/resources/rendering/camera/material/tween/profiling/GPU validation
10. `docs/PRODUCTION_GAPS.md` when entering an owning future subsystem named there
11. `docs/ROADMAP.md`
12. `docs/ARCHITECTURE.md`
13. `docs/AGENT_FIRST_PRINCIPLES.md`

Additional required subsystem/program reading:

- particles #46 / #47-#53: `docs/PARTICLES.md` plus the exact child contract,
- AI-operated loop #96: `docs/AI_OPERATED_WORKFLOW.md`,
- result/feedback UI #98/#99: `docs/AI_OPERATED_WORKFLOW.md` and `docs/WORKSPACE.md`,
- autonomous benchmark #100/#102-#104: `docs/AUTONOMOUS_BENCHMARK.md`, `docs/REFERENCE_PROJECTS.md`, `docs/EXTERNAL_REFERENCE_PROTOCOL.md`, and `docs/REPOSITORY_AUTOMATION.md`,
- Sprite #59: `docs/SPRITES.md` and `docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md`,
- game production #69-#79 plus #86-#92: `docs/GAME_PRODUCTION.md`, `docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md`, `docs/REPOSITORY_AUTOMATION.md` where project/package/toolchain diagnostics or provenance are owned, and relevant sections of `docs/PRODUCTION_GAPS.md`,
- Mesh2D #60: #60 plus relevant Sprite/Game Production handoff,
- Spine #61: `docs/SPINE.md` before any work; SP0 is a human license gate,
- later breadth #93/#101: read only when entering/promoting the owning area; these registers do not authorize skipping the fixed lane.

Then inspect live GitHub state:

1. open PRs,
2. CI/check status on relevant PRs,
3. issue state for the first incomplete roadmap item,
4. recent merged PRs/commits if prose appears stale.

For substantive work, also query relevant final-`main` commit knowledge by issue/path/area as described in `docs/COMMIT_KNOWLEDGE.md`. Historical trailers are evidence, not authority: current code/tests/live state/owner decisions/contracts still win.

## Source-of-truth hierarchy

When sources disagree, use this order:

1. compiling code and automated tests,
2. live PR/merge/CI state,
3. explicit owner-approved architecture/product/license decisions and hard constraints,
4. exact active issue acceptance criteria,
5. `PROJECT_STATUS.md`,
6. `docs/AI_OPERATED_WORKFLOW.md` for the AI-operated product/judgment contract,
7. `docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md` for frozen production integration seams,
8. active subsystem/program contract,
9. `docs/ROADMAP.md`,
10. older issue descriptions/discussion.

Commit trailers and external references are durable evidence/input, not higher sources of truth. If an older commit decision is intentionally replaced, preserve the replacement with `Supersedes:` under `docs/COMMIT_KNOWLEDGE.md`. If external evidence reveals that an existing contract is wrong or obsolete, change the Trace2D contract explicitly rather than silently following the external source.

If live code/state has advanced beyond prose, reconcile documentation in the same work rather than silently relying on stale text.

## Explicit `next/continue` protocol

The repository owner intentionally wants routine progress to work from a short instruction such as:

```text
@GitHub Trace2D 다음 진행해줘
Trace2D next
Trace2D continue
continue the next Trace2D task
```

Equivalent requests mean: **execute the core continuation lane without asking the owner to restate repository context.**

### Continuation algorithm

1. Read `AGENTS.md` and `PROJECT_STATUS.md`.
2. Inspect live open PRs and relevant CI/check state.
3. Reconcile any recent merge that made `PROJECT_STATUS.md` stale.
4. If the current request is an explicit owner roadmap/governance change, update contracts/status first; do not opportunistically implement a later feature.
5. If an active owner-governance PR explicitly blocks routine progression, finish only that scope first. A parallel governance/tooling PR may coexist when it explicitly states that it does **not** supersede the active implementation PR or waive its acceptance gates.
6. If there is an active core PR for the first incomplete implementation item:
   - inspect implementation/review/CI,
   - repair only that scope,
   - validate it,
   - update docs/status,
   - do not start a later core implementation item while it remains active.
7. If the active PR is complete and green but a recognized human/environment gate remains, report the single required action instead of pretending the gate passed or jumping ahead.
8. If there is no active core/governance blocker, select the first incomplete and unblocked item from `PROJECT_STATUS.md`.
9. If the item is an umbrella with a fixed child order, select the first incomplete child already named by the contract. Do not create a substitute stage or re-open an owner choice.
10. Read the exact issue and affected subsystem documents.
11. For substantive work, recover relevant final-`main` commit knowledge under `docs/COMMIT_KNOWLEDGE.md`: prior `Decision`, `Constraint`, `Rejected`, `Directive`, `Not-tested`, `Gate`, `Reference`, `Related`, and `Supersedes` facts that affect the active area. Do not blindly obey stale trailers.
12. For substantive subsystem/architecture/determinism/benchmark/performance/integration work, perform the bounded current external-reference pass in `docs/EXTERNAL_REFERENCE_PROTOCOL.md` **before freezing the design**. Start from `docs/REFERENCE_PROJECTS.md`, then refresh current primary sources. Extract `ADOPT / ADAPT / REJECT / DEFER` decisions instead of copying architectures blindly. Narrow regressions/typos may reuse an already-current established contract without a fresh literature search.
13. If the task touches repository readiness, environment/toolchain diagnostics, capability/eligibility metadata, contract auditing, OSS security or release provenance, follow `docs/REPOSITORY_AUTOMATION.md`. Use `ready` / `doctor` / `capabilities` machine-readable queries once their owning stages actually implement them; until then inspect the authoritative sources directly rather than inventing output.
14. Implement one coherent issue/child vertical slice using Trace2D-owned contracts. External references do not authorize scope creep, dependency addition, lane reordering or waiver of a human/environment gate.
15. Add/update automated tests, deterministic fixtures, diagnostics and measurement evidence appropriate to the behavior, including evidence for any adopted non-obvious external lesson.
16. Run the strongest practical validation available. The repository contract auditor runs in CI; run `pwsh ./scripts/repository_contract_audit.ps1` locally when changing its governed documentation/tooling and PowerShell is available.
17. Update subsystem/product contracts when behavior finalizes or changes. Add durable newly useful sources to `docs/REFERENCE_PROJECTS.md` or the owning reference contract when later work should rediscover them.
18. Update `PROJECT_STATUS.md` so completed/current/next is obvious when the active core state actually changes. A parallel governance/tooling PR must not rewrite live core status merely to describe itself.
19. Publish/update one scoped PR using `agent/<short-description>` unless an existing branch/PR owns the work. For substantive work, preserve a compact external-reference review and a final squash-commit knowledge block based on **actual** validation/gates.
20. Because `main` uses squash merges, ensure the final squash message preserves the durable knowledge required by `docs/COMMIT_KNOWLEDGE.md`; do not rely on branch-local commit messages surviving.
21. Do not begin the next core child until the current core PR is merged green.
22. Stop only when the turn's current work is complete, a real external blocker exists, or a recognized human gate is reached.

## External reference absorption rule

Trace2D should not repeatedly reinvent solved engineering problems from model memory.

For substantive work:

```text
active Trace2D uncertainty
    ↓
prior Trace2D commit knowledge
    ↓
current primary references
    ↓
ADOPT / ADAPT / REJECT / DEFER
    ↓
Trace2D-owned contract/test/workload/metric
    ↓
implementation evidence
    ↓
new squash-commit knowledge
```

Hard rules:

- search both **direct analogues** and the relevant **lower-layer precedent**; a database simulator, replay system, verifier framework or benchmarking tool may be more useful than another AI-game product,
- prefer official docs/repos/specs/papers and maintainer engineering posts over secondary summaries,
- use external work to learn edge cases and proven techniques, not to import its entire abstraction stack,
- references are not dependencies; vendoring/linking/redistribution still requires current version/license/security review,
- no external reference can reorder the owner-fixed core lane or mark an unmet gate complete,
- a borrowed technique is not "absorbed" until Trace2D has its own contract and evidence for it,
- performance/determinism/autonomy claims require reproducible Trace2D evidence rather than citation by authority,
- `docs/REFERENCE_PROJECTS.md` is a starting registry, not an exhaustive whitelist; future agents must search for stronger/newer work when the owning task activates.

## Repository self-description and automation rule

Trace2D should progressively replace repeated Agent guessing with small machine-readable repository/project diagnostics when a stable owning contract exists.

The direction in `docs/REPOSITORY_AUTOMATION.md` is:

```text
repository contract audit     active now
public OSS security baseline  active now
project readiness query       #97/#102
capability/eligibility query  #97/#98 -> #102 consumer
environment doctor            #70, extended by #78/#92
reproducible package evidence #70/release work
artifact/SBOM attestation     when real published packages exist
```

Hard rules:

- derive facts from existing authoritative state instead of mirroring them into another task database,
- do not install Beads/Dolt or Spec Kit as mandatory infrastructure merely because their patterns are useful,
- `available`, `eligible`, `tested`, and `supported` are distinct states,
- security/Scorecard results are advisory evidence, not proof of correctness or a substitute for tests,
- no future `doctor`, `ready`, or `capabilities` tool may silently clear a human/environment/license gate,
- repository tooling stays outside engine frame hot paths.

## Owner-fixed core order

`PROJECT_STATUS.md` is operationally authoritative. The intended long-term implementation order after the current particle work is:

```text
#52 explicit GPU particle runtime
 -> #53 particle CPU/GPU conformance/workloads/guidance

AI-operated foundation
 -> #97 machine-readable intent / Definition of Done
 -> #98 unified verify/diagnose/repair/WorkResult
 -> #99 minimal result-review Workspace / feedback loop
 -> #102 Benchmark B0 harness + current-capability matched tasks

Content production
 -> #59 complete Sprite program
 -> #103 Benchmark B1 Sprite/animation/particle matched tasks

External game-production foundation
 -> #69 Game/Application boundary
 -> #70 Project manifest + external consumer build/install/package
 -> #71 Scene hierarchy + engine/game typed components
 -> #86 unified resources
 -> #87 scene templates/world lifecycle
 -> #88 Camera2D/Viewport2D
 -> #72 Input Actions
 -> #73 TileMap
 -> #74 production text/localization
 -> #75 practical UI
 -> #104 Benchmark B2 autonomous top-down combat micro-game
 -> #89 Material2D/Shader2D
 -> #90 deterministic tween/property animation
 -> #76 Physics2D
 -> #77 Audio
 -> #91 profiler/diagnostics
 -> #78 Linux/non-MSVC hardening
 -> #92 real-GPU conformance
 -> #79 persistence/migration

Proof / later geometry and compatibility
 -> #12 flagship external game
 -> #60 Mesh2D
 -> #61 Spine SP0
```

Umbrellas/registers:

- #96 owns the AI-operated production loop but is not itself a one-PR implementation stage,
- #100 owns benchmark growth; concrete stages are #102 -> #103 -> #104,
- #93 records later lighting/navigation/platform/networking/hot-reload breadth,
- #101 records missing/shallow production capability contracts and must be consulted by owning future stages.

Earlier completed items #40/#42/#43/#39/#41/#47/#48/#49/#50/#51 remain historical predecessors.

### Why benchmark stages are interleaved

Do not wait until the engine is "finished" to test the AI-first claim.

- #102 proves the harness and current-capability workflow before Sprite breadth.
- #103 measures the content workflows most likely to need visual/multimodal feedback immediately after #59.
- #104 proves a coherent autonomous micro-game and one human-feedback revision cycle as soon as the minimum public external-game subset exists, before the rest of P8 breadth.

Tasks that require an unimplemented Trace2D capability are marked **not eligible**, not counted as autonomous-agent failures. Capability breadth and autonomous operability remain separate metrics.

### Sprite internal order

Within #59, follow `docs/SPRITES.md` exactly plus the frozen integration rules:

```text
S0 -> S1
 -> SR0 -> SR1 -> SR2 -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

The #85 integration rules remain non-optional:

- `SpriteRenderer2D` / `SpriteAnimator2D` fit the future #71 typed component model,
- fixed-step authoritative current state is separate from interactive previous/current presentation interpolation,
- exact-frame capture renders authoritative current state unless explicit sub-frame alpha is requested,
- Sprite assets remain CPU/project-relative truth separate from GPU handles for #86,
- renderer view input remains compatible with #88 Camera2D/Viewport2D,
- batch compatibility reserves a resolved material/pipeline identity for #89 without global painter-order sorting.

## AI-operated product hard rules

### Verification authority

Use this order for supported work:

1. **deterministic/structured verification** for engine-owned truth,
2. **multimodal review** only for perceptual/subjective questions,
3. **human judgment** for final taste/creative approval.

Do not ask a vision model to infer entity health, animation events, particle lifetime, UI semantic state, collision results or other facts that Trace2D can expose directly.

### Intent / Definition of Done

#97 must make project/task intent and completion criteria repository-visible where useful. A fresh agent should not need previous chat history to know what remains or which items require deterministic, multimodal or human review.

The #97 contract must also establish the minimum machine-readable project/task state needed for future derived readiness and capability queries without introducing a second project-management database.

### WorkResult / repair loop

#98 composes existing subsystem truth. It must support structured `verify -> diagnose -> repair -> re-verify` without making a second gameplay truth model or adding normal-frame report work. Where useful, it may compose capability/eligibility evidence established by #97 rather than inventing a parallel support model.

### Workspace

#99 is a review client over the same Agent/verification/result APIs used by CLI/MCP.

Preferred human flow:

```text
Read -> Review -> Request -> Approve
```

A world/entity browser is useful. A broad manually editable inspector is secondary and not the product center. No GUI-only authoritative database.

### Autonomous benchmark

#100/#102-#104 compare the same pinned agent on matched eligible tasks across:

```text
Godot + generic coding tools
Godot + pinned reviewed Godot MCP/agent bridge
Trace2D
```

Record at minimum:

- success rate,
- iterations/revision cycles,
- token usage when measurable,
- total tool calls,
- visual-feedback/capture/multimodal calls,
- human intervention count/type,
- deterministic verification coverage,
- final unresolved failures.

Published comparative claims require multiple trials, disclosed sample sizes and pinned versions. Do not cherry-pick one successful run.

The benchmark must also preserve enough execution identity/trace to distinguish stochastic Agent decisions from environment nondeterminism, and final objective success must come from an independent verifier rather than the Agent's self-declared completion. `docs/AUTONOMOUS_BENCHMARK.md` owns the full harness/replay/verifier contract.

B0 should consume machine-readable capability/eligibility metadata once #97/#98 establish it, so a missing Trace2D feature is classified as `not eligible` without a manually maintained benchmark-only capability table.

## Production architecture hard rules

The frozen contract in `docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md` exists to prevent later subsystem rework.

### Authoritative vs presentation vs tooling

- authoritative gameplay/world/component/animation/input/tween state stays headless and renderer-independent,
- presentation state may include interpolation, GPU resources, view matrices, batch runs and physical output,
- tooling/Agent/profiler/capture/WorkResult/benchmark state is built only on explicit request,
- presentation/tooling state never silently becomes gameplay truth.

### User-defined gameplay components

#71 must prove at least one external authored game component, not only engine components:

- stable explicit component type ID and schema version,
- authored vs runtime-only classification,
- explicit parse/validate/serialize/inspect adapters,
- strongly typed external game access,
- setup-time registration/type resolution,
- generation-safe invalidation,
- no generic reflection/property bag requirement,
- one component per type/entity baseline until multiplicity is concretely justified.

### Resource access

#86 establishes project-relative typed asset identity and generation-safe resolved handles:

- no per-frame path/filesystem resolution,
- CPU canonical asset state stays separate from GPU resources,
- successful immutable content is cached/reused,
- no tracing GC or mandatory atomic shared ownership in hot paths,
- unload/release occurs explicitly at safe points,
- memory reports distinguish known CPU bytes from renderer-created GPU resource evidence.

### Structural changes

#87 defines deterministic instantiate/despawn/world load-unload safe points. Do not mutate world structure from unordered callback races or silently pool arbitrary game entities.

### Material/shader

#89 is a small programmable 2D surface, not a material/render graph. Shader/parameter names resolve at setup; normal drawing uses cached pipelines and resolved parameter layouts.

### Tween/property animation

#90 resolves targets once. Never perform semantic selector + component string + property string lookup every fixed step.

### Profiling

#91 separates deterministic structural metrics from CPU/GPU machine timings. Hosted shared CI may gate structural budgets; hardware timing thresholds need stable dedicated runners.

### GPU conformance

#92 defines Tier A backend-independent validation, Tier B maintained real-GPU baseline and Tier C vendor/backend release evidence. Never claim universal cross-vendor exact pixels without proof.

### Production gap register

When #70/#72/#73/#75/#76/#77/#86/#60/#61 or other owning stages become active, read `docs/PRODUCTION_GAPS.md` and #101. Integrate the relevant missing/shallow contract before declaring that subsystem production-practical; explicitly defer or reject items that remain out of scope.

## Core continuation lane vs independent community lane

The strict order above governs the owner's automated/core progression and `Trace2D next/continue`.

It does **not** mean all unrelated open-source contributions are forbidden until the core item finishes.

### Core continuation lane

- first incomplete/unblocked owner-fixed item only,
- one coherent issue/PR at a time,
- no skipping to attractive later systems,
- active predecessor PR has precedence.

### Independent community contribution lane

A separate external/community PR may be reviewed when it is a narrow, isolated:

- bug fix,
- test improvement,
- documentation improvement,
- portability fix,
- tooling quality fix,
- small enhancement that does not preempt a frozen future architecture.

Independent work must not:

- compete with/duplicate active core implementation,
- silently redefine a future subsystem contract,
- add an unreviewed dependency/license obligation,
- violate determinism/ownership/performance boundaries,
- introduce broad speculative infrastructure,
- change product goals or human-gated decisions.

When overlap exists, prefer coordination/rebase over competing implementations.

Issue #80 tracks the contribution-lane policy explicitly.

## Do not ask unnecessary owner questions

A continuation request authorizes normal implementation decisions already constrained by repository contracts.

Do not ask the owner to choose when:

- execution order already chooses the next task,
- the active issue has sufficient acceptance criteria,
- product/architecture/performance/determinism rules determine a safe narrow solution,
- an implementation detail is reversible and does not change product goals.

When evidence is incomplete for a non-owner decision, prefer the simplest reversible design consistent with existing contracts, add tests/measurement and document the tradeoff.

Do not invent a human gate because a task is difficult.

## Recognized human gates

Human/owner intervention is reserved for real decisions that existing contracts intentionally do not delegate:

1. merge gate when policy/tooling genuinely requires owner merge,
2. explicit architecture/product-goal change,
3. required local hardware/environment validation that hosted CI cannot truthfully satisfy,
4. particle CPU/GPU backend choice where the particle contract requires human selection,
5. Spine #61 SP0 license/integration decision,
6. credentials/billing/paid external service authorization,
7. unresolved dependency/distribution/legal decision,
8. promotion of a #93/#101 later-breadth area into the fixed core order,
9. final subjective/creative approval where the AI-operated workflow explicitly assigns judgment to the user.

When a gate is reached, report one concrete required owner action. Do not pretend a missing hardware or human decision passed.

## Development workflow

Normal core flow:

```text
Issue / live PR / gate
 -> prior commit knowledge when substantive
 -> current external-reference pass when substantive
 -> environment/capability preflight when implemented and relevant
 -> branch
 -> implementation
 -> automated tests / fixtures
 -> local or CI validation
 -> repository contract audit for governed files
 -> docs/status
 -> draft PR
 -> green CI
 -> required environment/human gate if any
 -> final squash commit knowledge
 -> merge
```

Branch naming:

```text
agent/<short-description>
```

Main uses squash merges. Keep one PR understandable as one coherent change. For substantive work, branch-local commit messages are not the durable knowledge authority; the final squash commit must preserve the actual decision/validation context under `docs/COMMIT_KNOWLEDGE.md`.

## C++ engineering rules

- C++20 baseline.
- RAII and explicit ownership.
- Prefer `std::unique_ptr` for owning heap relationships; avoid unnecessary shared ownership.
- Raw pointers/references are non-owning unless clearly documented otherwise.
- Stable public/runtime identity uses engine IDs/handles, not pointers.
- Keep SDL/backend/protocol/UI-client/model-provider types behind their owning boundaries.
- Do not add allocation to known per-frame hot paths without evidence and documentation.
- Reuse persistent/capacity-managed state where steady-state work would otherwise recreate resources.
- Resolve strings/paths/component/material/property names during setup where practical; hot paths use typed/resolved IDs/handles.
- Do not create custom allocators, lock-free queues, generic ECS machinery, generic reflection, job systems, render graphs, material graphs or binary plugin ABIs before requirements/measurement justify them.
- Deterministic observable output must not depend on unordered/unspecified iteration order.
- Prefer direct simple O(N) scans over speculative indexing until real workloads justify the index; move to resolved/indexed lookup when measured large dynamic-world use does justify it.
- No performance or autonomy claim becomes fact without a reproducible workload/benchmark and clear metric boundary.

## Agent-first / AI-operated rules

- MCP is an adapter, not the engine API.
- Engine/Agent APIs remain protocol-independent.
- Headless and windowed execution share authoritative runtime/game logic.
- Tests/agents explicitly control simulation time.
- Interactive render interpolation is presentation only; it does not change Agent/gameplay truth.
- Authored project/scene/component/asset/UI/tile/input/persistence/intent metadata stays text-first and diffable where practical.
- Semantic identity/selectors beat coordinate targeting.
- Structured runtime state beats pixel/audio inference for semantic correctness.
- Machine-facing commands return stable structured diagnostics and predictable exit behavior.
- Expensive snapshots/fingerprints/reports/capture/migration/generation/profiling/WorkResult/benchmark/multimodal work remain request/setup/tooling work, not ordinary frame work.
- If a safe versioned file edit is sufficient, do not create a redundant narrow MCP action for every property.
- Multimodal review is advisory; final subjective judgment remains human.
- For substantive new work, current external precedents are engineering input under `docs/EXTERNAL_REFERENCE_PROTOCOL.md`; they are never an excuse to violate Trace2D's authority/performance/ownership rules.
- When machine-readable `ready`, `doctor`, or `capabilities` surfaces exist, prefer them over re-parsing human prose, but verify they are current and do not outrank live GitHub/hard-gate truth.

Target vocabulary remains intentionally small and composable:

```text
build
run
inspect
query
input/action
step
assert
capture
test
analyze
profile
verify
migrate
```

Repository/project tooling may additionally expose narrow setup/state commands such as `ready`, `doctor`, or `capabilities` when their owning contracts are implemented; those are not gameplay APIs.

## Validation requirements

Every implementation runs the strongest relevant available checks.

Current Windows baseline:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug
```

Repository/governance validation:

```powershell
pwsh ./scripts/repository_contract_audit.ps1
```

The repository contract auditor runs in hosted CI and guards stable machine-checkable cross-document/security invariants. It intentionally does not decide live PR/issue state.

CI remains authoritative for hosted-toolchain dependency restore and repository-wide gates **only for what hosted CI actually executes**. A skipped opt-in real-GPU test is not real-GPU evidence.

New behavior should have automated tests whenever practical. Visual/audio/GPU behavior may require local/presented evidence, but semantic/math/order/import/serialization/query behavior should remain headless-CI testable whenever practical.

Benchmark claims require their own committed multi-run evidence and pinned environment metadata; ordinary unit-test CI does not prove autonomous-agent superiority.

Security scanners and repository-health tools are defense in depth. CodeQL/Scorecard success does not prove runtime correctness, determinism, memory safety or absence of vulnerabilities.

When #78 is complete, the added non-MSVC platform/toolchain becomes part of normal required validation. When #92 is complete, real-GPU claims must follow its tiered support/conformance contract rather than relying solely on CPU-side renderer tests.
