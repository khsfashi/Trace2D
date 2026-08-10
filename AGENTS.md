# Trace2D Agent Operating Guide

This file is the entry point for any coding agent working in Trace2D.

The goal is that a fresh agent can open the repository, recover live project state, select the correct next task, implement it, validate it, and leave a deterministic handoff without relying on previous chat history.

## Project identity

Trace2D is a deterministic and observable C++20 2D engine designed so coding agents can work through structured engine contracts instead of editor-only state or pixel guessing.

Core workflow:

```text
edit authored/source state
 -> build/import
 -> run headlessly when possible
 -> inspect/query semantic state
 -> inject virtual input/actions
 -> step exact frames
 -> assert authoritative behavior
 -> profile/analyze explicitly when relevant
 -> capture pixels only when presentation matters
```

Generation or other external creative inputs may be nondeterministic. Imported canonical state, runtime semantics, assertions, migration, structural metrics and reproducible validation should be deterministic/machine-readable wherever practical.

## Required reading order

Before changing code:

1. `AGENTS.md`
2. `PROJECT_STATUS.md`
3. exact active issue / PR
4. active subsystem contract
5. `docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md` when work can affect world/components/resources/rendering/camera/material/tween/profiling/GPU validation
6. `docs/ROADMAP.md`
7. `docs/ARCHITECTURE.md`
8. `docs/AGENT_FIRST_PRINCIPLES.md`

Additional required subsystem reading:

- particles #46 / #47-#53: `docs/PARTICLES.md` plus the exact child contract,
- Sprite #59: `docs/SPRITES.md` **and** `docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md`,
- game production #69-#79 plus #86-#92: `docs/GAME_PRODUCTION.md` **and** `docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md`,
- Mesh2D #60: #60 plus relevant Sprite/Game Production handoff,
- Spine #61: `docs/SPINE.md` before any work; SP0 is a human license gate,
- later breadth #93: read #93 only after an explicit owner promotion; it is not routine core continuation.

Then inspect live GitHub state:

1. open PRs,
2. CI/check status on relevant PRs,
3. issue state for the first incomplete roadmap item,
4. recent merged PRs/commits if prose appears stale.

## Source-of-truth hierarchy

When sources disagree, use this order:

1. compiling code and automated tests,
2. live PR/merge/CI state,
3. explicit owner-approved architecture/product/license decisions and hard constraints,
4. exact active issue acceptance criteria,
5. `PROJECT_STATUS.md`,
6. `docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md` for the frozen production integration seams,
7. active subsystem contract document,
8. `docs/ROADMAP.md`,
9. older issue descriptions/discussion.

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
5. If there is an active owner-governance PR that must finish before routine code progression, finish only that scope first.
6. If there is an active core PR for the first incomplete implementation item:
   - inspect implementation/review/CI,
   - repair only that scope,
   - validate it,
   - update docs/status,
   - do not start a later core item while it remains active.
7. If the active PR is complete and green but merge genuinely requires the owner, report the single merge action instead of jumping ahead.
8. If there is no active core/governance PR, select the first incomplete and unblocked item from `PROJECT_STATUS.md`.
9. If the item is an umbrella with a fixed child order, select the first incomplete child already named by the contract. Do not create a substitute stage or re-open an owner choice.
10. Read the exact issue and affected subsystem documents.
11. Implement one coherent issue/child vertical slice.
12. Add/update automated tests, deterministic fixtures, diagnostics and measurement evidence appropriate to the behavior.
13. Run the strongest practical validation available.
14. Update subsystem contracts when behavior finalizes or changes.
15. Update `PROJECT_STATUS.md` so completed/current/next is obvious.
16. Publish/update one scoped PR using `agent/<short-description>` unless an existing branch/PR owns the work.
17. Do not begin the next core child until the current core PR is merged green.
18. Stop only when the turn's current work is complete, a real external blocker exists, or a recognized human gate is reached.

## Owner-fixed core order

`PROJECT_STATUS.md` is operationally authoritative. After the production architecture freeze #85 is merged, the long-term implementation order is:

```text
#52 -> #53
 -> #59 Sprite
 -> #69 Game/Application
 -> #70 Project/build/package
 -> #71 Scene hierarchy + engine/game typed components
 -> #86 unified resources
 -> #87 scene templates/world lifecycle
 -> #88 Camera2D/Viewport2D
 -> #72 Input Actions
 -> #73 TileMap
 -> #74 production text/localization
 -> #75 practical UI
 -> #89 Material2D/Shader2D
 -> #90 deterministic tween/property animation
 -> #76 Physics2D
 -> #77 Audio
 -> #91 profiler/diagnostics
 -> #78 Linux/non-MSVC hardening
 -> #92 real-GPU conformance
 -> #79 persistence/migration
 -> #12 flagship external game
 -> #60 Mesh2D
 -> #61 Spine SP0
```

Earlier completed items #40/#42/#43/#39/#41/#47/#48/#49/#50/#51 remain historical predecessors.

Issue #93 is **not** part of this fixed lane. It records lighting/shadows, navigation, broader platforms, networking and safe hot reload until an explicit owner decision promotes one area.

Within particles, finish exactly one of #52 -> #53 at a time.

Within #59, follow `docs/SPRITES.md` exactly, plus the frozen integration rules in `docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md`:

```text
S0 -> S1
 -> SR0 -> SR1 -> SR2 -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

The #85 integration rules are non-optional even though they do not add a separate Sprite stage name:

- `SpriteRenderer2D` / `SpriteAnimator2D` must fit the future #71 typed component model,
- fixed-step authoritative current state is separate from interactive previous/current presentation interpolation,
- exact-frame capture renders authoritative current state unless explicit sub-frame alpha is requested,
- Sprite assets remain CPU/project-relative truth separate from GPU handles for #86,
- renderer view input remains compatible with #88 Camera2D/Viewport2D,
- batch compatibility reserves a resolved material/pipeline identity for #89 without allowing global painter-order sorting.

After #59, follow the expanded game-production sequence exactly:

```text
#69
 -> #70
 -> #71
 -> #86
 -> #87
 -> #88
 -> #72
 -> #73
 -> #74
 -> #75
 -> #89
 -> #90
 -> #76
 -> #77
 -> #91
 -> #78
 -> #92
 -> #79
```

Then complete #12 before Mesh2D. #60 completes M0 -> M1. #61 stops at SP0 unless explicit owner license approval exists.

## Production architecture hard rules

The frozen contract in `docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md` exists to prevent later subsystem rework.

### Authoritative vs presentation vs tooling

- authoritative gameplay/world/component/animation/input/tween state stays headless and renderer-independent,
- presentation state may include interpolation, GPU resources, view matrices, batch runs and physical output,
- tooling/Agent/profiler/capture state is built only on explicit request,
- presentation/tooling state never silently becomes gameplay truth.

### User-defined gameplay components

#71 must prove at least one external authored game component, not only engine components.

- stable explicit component type ID and schema version,
- authored vs runtime-only classification,
- explicit parse/validate/serialize/inspect adapters,
- strongly typed external game access,
- setup-time registration/type resolution,
- generation-safe invalidation,
- no generic reflection/property bag requirement,
- one component per type/entity baseline until multiplicity is concretely justified.

### Resource access

#86 establishes project-relative typed asset identity and generation-safe resolved handles.

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
- `docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md` already freezes the integration seam,
- architecture/performance/determinism rules determine a safe narrow solution,
- an implementation detail is reversible and does not change product goals.

When evidence is incomplete for a non-owner decision, prefer the simplest reversible design consistent with existing contracts, add tests/measurement and document the tradeoff.

Do not invent a human gate because a task is difficult.

## Recognized human gates

Human/owner intervention is reserved for real decisions that existing contracts intentionally do not delegate:

1. merge gate when policy/tooling genuinely requires owner merge,
2. explicit architecture/product-goal change,
3. particle CPU/GPU backend choice where the particle contract requires human selection,
4. Spine #61 SP0 license/integration decision,
5. credentials/billing/paid external service authorization,
6. unresolved dependency/distribution/legal decision,
7. promotion of a #93 later-breadth area into the fixed core order.

When a gate is reached, report one concrete required owner action.

## Development workflow

Normal core flow:

```text
Issue
 -> branch
 -> implementation
 -> automated tests / fixtures
 -> local or CI validation
 -> docs/status
 -> draft PR
 -> green CI
 -> merge gate / merge
```

Branch naming:

```text
agent/<short-description>
```

Main uses squash merges. Keep one PR understandable as one coherent change.

## C++ engineering rules

- C++20 baseline.
- RAII and explicit ownership.
- Prefer `std::unique_ptr` for owning heap relationships; avoid unnecessary shared ownership.
- Raw pointers/references are non-owning unless clearly documented otherwise.
- Stable public/runtime identity uses engine IDs/handles, not pointers.
- Keep SDL/backend/protocol types behind their owning boundaries.
- Do not add allocation to known per-frame hot paths without evidence and documentation.
- Reuse persistent/capacity-managed state where steady-state work would otherwise recreate resources.
- Resolve strings/paths/component/material/property names during setup where practical; hot paths use typed/resolved IDs/handles.
- Do not create custom allocators, lock-free queues, generic ECS machinery, generic reflection, job systems, render graphs, material graphs, plugin ABIs or bespoke containers before requirements/measurement justify them.
- Deterministic observable output must not depend on unordered/unspecified iteration order.
- Prefer direct simple O(N) scans over speculative indexing until real workloads justify the index.
- No performance claim becomes fact without a reproducible workload and clear metric boundary.

## Agent-first rules

- MCP is an adapter, not the engine API.
- Engine/Agent APIs remain protocol-independent.
- Headless and windowed execution share authoritative runtime/game logic.
- Tests/agents explicitly control simulation time.
- Interactive render interpolation is presentation only; it does not change Agent/gameplay truth.
- Authored project/scene/component/asset/UI/tile/input/persistence metadata stays text-first and diffable where practical.
- Semantic identity/selectors beat coordinate targeting.
- Structured runtime state beats pixel/audio inference for semantic correctness.
- Machine-facing commands return stable structured diagnostics and predictable exit behavior.
- Expensive snapshots/fingerprints/reports/capture/migration/generation/profiling remain request/setup work, not ordinary frame work.

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
migrate
```

## Validation requirements

Every implementation runs the strongest relevant available checks.

Current Windows baseline:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug
```

CI remains authoritative for hosted-toolchain dependency restore and repository-wide gates.

New behavior should have automated tests whenever practical. Visual/audio/GPU behavior may require local/presented evidence, but semantic/math/order/import/serialization/query behavior should remain headless-CI testable whenever practical.

When #78 is complete, the added non-MSVC platform/toolchain becomes part of normal required validation. When #92 is complete, real-GPU claims must follow its tiered support/conformance contract rather than relying solely on CPU-side renderer tests.