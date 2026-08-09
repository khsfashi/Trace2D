# Trace2D Agent Operating Guide

This file is the entry point for any coding agent working in Trace2D.

The goal is that a fresh agent can open the repository, recover the project's current state, select the correct next task, implement it, validate it, and leave the repository in a state that another agent can continue from without relying on chat history.

## Project identity

Trace2D is a deterministic and observable C++20 2D engine designed for AI coding agents.

Its defining capability is not "AI generates game code." The engine itself must make development automatable:

```text
edit -> build -> run -> step -> inspect -> input -> assert -> capture
```

Authoritative gameplay state should be available as structured data. Pixels are for visual QA, not the only source of truth.

Long-term authored-asset work follows the same principle: generation may be nondeterministic, but imported canonical state, deterministic processing/QA, runtime animation state, assertions, and measured performance contracts must be machine-readable wherever practical.

## Required reading order

Before changing code, read these files in order:

1. `AGENTS.md`
2. `PROJECT_STATUS.md`
3. `docs/PUBLIC_RELEASE.md`
4. `docs/ROADMAP.md`
5. `docs/ARCHITECTURE.md`
6. `docs/AGENT_FIRST_PRINCIPLES.md`

Then inspect the live GitHub state:

1. open pull requests
2. CI/check status on the active PR
3. open issues relevant to the current phase
4. recent commits if repository state differs from `PROJECT_STATUS.md`

Additional required reading by active phase:

- particle umbrella/children **#46-#53**: read `docs/PARTICLES.md` and the exact active child issue,
- Sprite umbrella **#59**: read `docs/SPRITES.md` and the exact active child issue,
- Mesh2D umbrella **#60**: read #60 plus the relevant Mesh2D handoff sections in `docs/SPRITES.md`,
- Spine compatibility **#61**: read `docs/SPINE.md` before doing anything; SP0 is a human license gate.

Live repository state wins over stale prose. If `PROJECT_STATUS.md` is stale, update it as part of the work.

## Source-of-truth hierarchy

When documents disagree, use this order:

1. compiling code and automated tests
2. active PR and CI results
3. explicit owner-approved architecture/license decisions and hard constraints
4. exact active issue acceptance criteria
5. `PROJECT_STATUS.md`
6. subsystem contract document (`docs/PARTICLES.md`, `docs/SPRITES.md`, `docs/SPINE.md`, etc.)
7. `docs/PUBLIC_RELEASE.md`
8. `docs/ROADMAP.md`
9. older issue descriptions and discussion

For particle work, `docs/PARTICLES.md` plus the active #47-#53 issue records the owner-approved particle architecture.

For Sprite work, `docs/SPRITES.md` plus #59 and its current child record the owner-approved Sprite architecture and fixed internal execution order.

For Spine work, `docs/SPINE.md` plus #61 records the license gate. No stale issue or implementation convenience may override an unapproved SP0 gate.

If a contract document disagrees with live compiling code/tests because implementation has advanced, reconcile the documentation in the same PR rather than silently choosing one interpretation.

Do not silently reinterpret a deliberate architecture constraint. If a constraint must change, document why in the same PR.

## Explicit continuation-command protocol

The repository owner intentionally wants routine progress to be possible with a short command such as:

```text
@GitHub Trace2D 다음 진행해줘
Trace2D next
Trace2D continue
continue the next Trace2D task
```

Treat equivalent requests as an instruction to execute the following algorithm without asking the owner to restate repository context.

### Continuation algorithm

1. Read `AGENTS.md` and `PROJECT_STATUS.md`.
2. Inspect current open PRs and relevant CI/check status.
3. Reconcile live GitHub state with `PROJECT_STATUS.md` if it changed since the document was last updated.
4. If an owner-directed roadmap/governance change is currently requested, update the contracts/status first without opportunistically implementing a later feature.
5. If there is an active PR for the first incomplete task:
   - inspect it,
   - repair implementation/review/CI problems within scope,
   - validate it,
   - update required docs/status,
   - do **not** start a later issue while that PR remains the active work item.
6. If the active PR is complete and green but repository policy/tool permissions leave merge to the owner, report the single required owner action: merge that PR. Do not create unrelated work merely to avoid the merge gate.
7. If there is no active PR, select the **first incomplete and unblocked** item in the owner-fixed `PROJECT_STATUS.md` execution order.
8. If the item is an umbrella with a fixed internal order (`#46`, `#59`, `#60`, `#61` after SP0 approval), select or create exactly the first incomplete child defined by its contract. Do not re-run an owner choice that the contract already fixed.
9. Read the exact issue acceptance criteria and affected architecture documents.
10. Implement only one coherent child/issue vertical slice.
11. Add/update the relevant automated tests and machine-readable diagnostics/fixtures.
12. Run the strongest practical local/CI validation available.
13. Update subsystem contract documents when implementation finalizes or changes a contract.
14. Update `PROJECT_STATUS.md` so completed/current/next state is obvious.
15. Publish/update a draft PR using `agent/<short-description>` naming unless an existing branch/PR already owns the work.
16. Do not begin the next child until the current PR is merged green.
17. Stop only when work is complete for the current turn, a real external blocker exists, or an explicit human gate is reached.

### Do not ask unnecessary questions

A continuation request is authorization to make normal implementation decisions that are already constrained by repository contracts. Do not ask the owner to choose among alternatives when:

- the execution order already chooses the next task,
- the active issue has sufficient acceptance criteria,
- an implementation detail can be decided by existing architecture/performance/determinism rules,
- a safe narrow vertical slice can be completed without changing project goals.

If a decision is not owner-level and evidence is incomplete, prefer the simplest reversible design consistent with existing contracts, add tests/measurement, and document the tradeoff.

Do not invent a human gate merely because a task is difficult.

## Allowed human gates

Human intervention should be reserved for decisions that cannot safely be delegated by existing contracts.

Current recognized gates include:

1. **Merge gate** — when repository policy/tool availability requires the owner to merge an otherwise complete green PR.
2. **Explicit owner architecture/product-goal change** — a decision that changes the fixed roadmap or a hard invariant rather than merely implementing it.
3. **Particle backend choice** — CPU/GPU backend selection where `docs/PARTICLES.md` explicitly requires a human decision.
4. **Spine license gate (Issue #61 SP0)** — no Spine Runtime integration before explicit owner approval after license confirmation.
5. **Credentials/paid external service authorization** — required secrets, billing, or third-party access that the repository does not already provide.
6. **Legal/license decision** — adding/distributing a dependency when rights/obligations are unresolved.

When a human gate is reached, state one concrete required owner action. Do not ask broad questions such as "what should we do next?" when the missing decision can be named precisely.

Example:

```text
Human gate reached: Spine runtime-license integration approval is required.
No Spine code/dependency has been added.
Record the approved integration/distribution model before SP1 can begin.
```

## How to choose the next task

1. If `PROJECT_STATUS.md` lists an active PR, finish or repair that PR first.
2. Never start a later feature while the active predecessor PR has failing CI unless the failure is proven unrelated and the status explicitly documents the exception.
3. Otherwise select the first unblocked issue in the "Next execution order" section of `PROJECT_STATUS.md`.
4. Work on one coherent issue/PR at a time unless two issues are inseparable by the active contract.
5. Prefer the smallest vertical slice that satisfies one complete child contract and leaves the repository runnable/testable.

Do not jump ahead to attractive later features such as an editor, advanced rendering, custom allocators, lock-free infrastructure, GPU particles, physics, animation, Sprite generation, Mesh2D, or Spine while earlier owner-fixed gates are incomplete.

Within particles, complete exactly one of #47 -> #48 -> #49 -> #50 -> #51 -> #52 -> #53 at a time.

After #53, the owner has already selected #59 Sprite as the next breadth program. Do **not** re-open the old physics-vs-animation-vs-hot-reload choice. Follow `docs/SPRITES.md` sequentially, then #60 Mesh2D, then #61 SP0.

## Development workflow

Use this flow for normal changes:

```text
Issue
  -> branch
  -> implementation
  -> tests
  -> local/CI validation
  -> documentation/status update
  -> draft PR
  -> green CI
  -> merge gate / merge
```

Branch naming for agent-created branches:

```text
agent/<short-description>
```

Main uses squash merges. Keep PR scope understandable from one squash commit.

## Before editing

For each task:

- read the issue acceptance criteria,
- inspect the modules that will be affected,
- confirm dependency direction in `docs/ARCHITECTURE.md`,
- read the active subsystem contract,
- identify the relevant automated test level,
- identify hot-path/resource-lifetime implications,
- avoid adding a dependency unless the current phase actually needs it and its license/distribution status is clear.

## Validation requirements

Every implementation must run the most relevant available checks.

Current Windows baseline:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug
```

CI may use a separate preset appropriate for the current GitHub runner image.

New behavior should have automated tests when practical. A machine-facing feature without tests is considered incomplete unless the PR explains why it cannot be tested yet.

Visual GPU behavior may require windowed/manual or dedicated workload evidence, but backend-independent semantic/math/order/import behavior should remain headless-CI testable whenever practical.

## C++ engineering rules

Prioritize predictable code over clever code.

- C++20 is the project baseline.
- Use RAII and explicit ownership.
- Prefer `std::unique_ptr` for owning heap relationships.
- Raw pointers/references are non-owning unless clearly documented otherwise.
- Avoid unnecessary shared ownership.
- Keep platform/library types behind module boundaries where practical.
- Do not add allocation to known per-frame hot paths without a reason.
- Reuse persistent/capacity-managed objects/resources where steady-state work would otherwise recreate them unnecessarily.
- Do not build custom allocators, lock-free queues, ECS machinery, or bespoke containers before measurement/requirements justify them.
- No benchmark claim may be documented as fact without measured data.
- Stable entity identity exposed to automation must never be a raw pointer.
- Deterministic observable behavior must not depend on unspecified container iteration order.
- Prefer direct/simple O(N) scans over speculative indexing until workload measurement justifies extra structures.

## Agent-first rules

The following are hard architectural constraints unless deliberately revised with documentation:

- MCP is an adapter, not the engine API.
- The automation facade must remain protocol independent.
- Headless execution shares authoritative runtime logic with windowed execution.
- Simulation time must be explicitly controllable by tests/agents.
- Authored project/scene/asset metadata should be text-first and diffable where practical.
- Automation prefers semantic identity/selectors over screen coordinates.
- Structured runtime state is preferred over visual inference.
- Machine-facing commands use stable exit behavior and structured diagnostics.
- CLI/JSON/MCP adapters should compose a small vocabulary of operations instead of mirroring every engine function.
- Explicit expensive QA/snapshot/capture work must not become mandatory ordinary per-frame work.

Target vocabulary remains close to:

```text
build
run
inspect
query
input
step
assert
capture
test
```

Subsystem tooling may add narrow operations such as asset import/generate/validate, but must preserve the same composable/structured philosophy.

## Particle-specific hard rules

When working on #46-#53:

- CPU particle execution is the deterministic semantic reference and must be fully inspectable headlessly.
- Rich supported CPU particle properties are allowed; per-particle heap objects, strings, maps, callbacks, renderer handles, and arbitrary script/module state are not.
- Particle capacity is explicit and validated before simulation.
- Ordinary stepping must not build JSON, detailed snapshots, screenshots, or fingerprints unless explicitly requested.
- Keyed randomness must keep unrelated emitters and random channels isolated.
- The CPU cost report separates deterministic structural metrics from machine-dependent timing evidence.
- Never invent a portable "CPU percentage" from semantic operation counts.
- A coding agent may recommend CPU or GPU using documented measurements, but **must never change the backend automatically**.
- CPU -> GPU is a human decision represented by explicit reviewable authored/build configuration.
- `backend=cpu` remains CPU even if analysis recommends considering GPU.
- `backend=gpu` must fail clearly when unsupported; never silently fall back to CPU.
- GPU compilation minimizes runtime attributes from the verified ParticleProgram rather than copying the complete rich CPU reference layout.
- Normal GPU mode must not also run the full CPU reference simulation unless explicit conformance/debug mode requests dual execution.
- CPU is the exact semantic oracle; do not claim universal bit-identical floating-point GPU behavior across vendors/drivers without a separately proven numeric contract.
- Particle pixels are visual QA evidence, not the only correctness oracle.

## Sprite-specific hard rules

When working on #59, follow `docs/SPRITES.md`.

Key invariants include:

- external/generator formats are import inputs; canonical Trace2D SpriteAsset data is runtime truth,
- generated image output is never automatically authoritative,
- source geometry prefers exact integer pixel metadata; normalized UVs are derived renderer state,
- trim/atlas packing must preserve source-space pivot/placement semantics,
- the target Sprite Renderer is production-complete traditional 2D sprite presentation, not a minimal quad milestone,
- semantic painter order must not be globally resorted for batching,
- expensive generation/repair/QA is offline explicit work,
- deterministic runtime animation is independent of renderer initialization,
- Agent animation QA uses structured state/exact-frame assertions before visual capture,
- live AI provider calls are not ordinary CI correctness gates,
- recorded/synthetic fixtures prove deterministic processing/runtime behavior,
- arbitrary deformable textured geometry belongs to #60 Mesh2D rather than bloating SpriteRenderer.

## Spine-specific hard rules

Issue #61 and `docs/SPINE.md` define the gate.

Until SP0 is explicitly approved, agents MUST NOT vendor/copy/fetch/build/distribute the Spine Runtime as part of Trace2D, add Spine-derived implementation code, or claim shipped Spine compatibility.

Generic native Sprite/Animation/Mesh2D work that contains no Spine code may proceed according to the owner-fixed sequence.

## Scope control

Trace2D grows through narrow measured vertical slices, not by attempting to become a full general-purpose engine at once.

Do not make these implicit requirements unless `PROJECT_STATUS.md` and the active issue intentionally introduce them:

- full editor,
- scripting language,
- networking,
- audio engine,
- advanced lighting/PBR,
- full-featured ECS,
- custom allocator framework,
- work-stealing job system,
- broad platform support beyond tested baselines,
- generic particle graph/editor,
- gameplay-authoritative particle collision,
- particle trails/sub-emitter recursion,
- generic material/shader graph,
- render graph/bindless/GPU-driven scene architecture.

## Documentation responsibilities

Update `PROJECT_STATUS.md` in the same PR whenever work changes any of these:

- current phase,
- active PR,
- completed release gate,
- next execution order,
- known blocker/human gate,
- CI/build assumptions,
- major architecture decision.

Update `docs/PUBLIC_RELEASE.md` only when public release scope/gates change, not after every normal task.

Update architecture/design documents when a change affects module boundaries, determinism guarantees, authored formats, licenses/dependencies, or the Agent contract.

Particle child PRs must update `docs/PARTICLES.md` whenever they finalize or change particle semantic, cost-analysis, compiler, backend, or conformance contracts.

Sprite child PRs must update `docs/SPRITES.md` whenever they finalize or change sprite asset, renderer, animation, processing, generation, QA, or performance contracts.

Spine work must update `docs/SPINE.md` with the recorded license decision before SP1 and whenever the integration/distribution contract changes.

## Finishing a task

Before considering a PR complete:

- acceptance criteria are satisfied or explicitly called out,
- tests pass,
- CI is green or an external blocker is documented,
- no generated/build artifacts are accidentally committed,
- no unresolved dependency/license obligation was silently introduced,
- machine-readable output remains stable/deterministic where applicable,
- subsystem contract docs reflect finalized behavior,
- `PROJECT_STATUS.md` reflects the new state,
- next work is obvious to a fresh agent.

The handoff standard is simple: another agent should not need the previous chat to know what to do next.