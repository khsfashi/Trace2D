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
 -> capture pixels only when presentation matters
```

Generation or other external creative inputs may be nondeterministic. Imported canonical state, runtime semantics, assertions, migration, and measurement should be deterministic/machine-readable wherever practical.

## Required reading order

Before changing code:

1. `AGENTS.md`
2. `PROJECT_STATUS.md`
3. exact active issue / PR
4. active subsystem contract
5. `docs/ROADMAP.md`
6. `docs/ARCHITECTURE.md`
7. `docs/AGENT_FIRST_PRINCIPLES.md`

Additional required subsystem reading:

- particles #46 / #47-#53: `docs/PARTICLES.md` plus the exact child contract,
- Sprite #59: `docs/SPRITES.md`,
- open-source game production #67 / #69-#79: `docs/GAME_PRODUCTION.md`,
- Mesh2D #60: #60 plus relevant Sprite/Game Production handoff,
- Spine #61: `docs/SPINE.md` before any work; SP0 is a human license gate.

Then inspect live GitHub state:

1. open PRs,
2. CI/check status on any relevant PR,
3. issue state for the first incomplete roadmap item,
4. recent merged PRs/commits if prose appears stale.

## Source-of-truth hierarchy

When sources disagree, use this order:

1. compiling code and automated tests,
2. live PR/merge/CI state,
3. explicit owner-approved architecture/product/license decisions and hard constraints,
4. exact active issue acceptance criteria,
5. `PROJECT_STATUS.md`,
6. active subsystem contract document,
7. `docs/ROADMAP.md`,
8. older issue descriptions/discussion.

If live code/state has advanced beyond prose, reconcile the documentation in the same work rather than silently relying on stale text.

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
5. If there is an active core PR for the first incomplete item:
   - inspect implementation/review/CI,
   - repair only that scope,
   - validate it,
   - update docs/status,
   - do not start a later core item while it remains active.
6. If the active PR is complete and green but merge genuinely requires the owner, report the single merge action instead of jumping ahead.
7. If there is no active core PR, select the first incomplete and unblocked item from `PROJECT_STATUS.md`.
8. If the item is an umbrella with a fixed child order, select the first incomplete child already named by the contract. Do not create a substitute stage or re-open an owner choice.
9. Read the exact issue and affected subsystem documents.
10. Implement one coherent issue/child vertical slice.
11. Add/update automated tests, deterministic fixtures, diagnostics, and measurement evidence appropriate to the behavior.
12. Run the strongest practical validation available.
13. Update subsystem contracts when behavior finalizes or changes.
14. Update `PROJECT_STATUS.md` so completed/current/next is obvious.
15. Publish/update one scoped PR using `agent/<short-description>` unless an existing branch/PR owns the work.
16. Do not begin the next core child until the current core PR is merged green.
17. Stop only when the turn's current work is complete, a real external blocker exists, or a recognized human gate is reached.

## Owner-fixed core order

`PROJECT_STATUS.md` is operationally authoritative, but the long-term sequence is intentionally fixed as:

```text
#50 -> #51 -> #52 -> #53
 -> #59 Sprite
 -> #67 open-source game-production foundation
      #69 -> #70 -> #71 -> #72 -> #73 -> #74 -> #75 -> #76 -> #77 -> #78 -> #79
 -> #12 flagship external sample game
 -> #60 Mesh2D
 -> #61 Spine SP0
```

Earlier completed items #40/#42/#43/#39/#41/#47/#48/#49 remain historical predecessors.

Within particles, finish exactly one of #50 -> #51 -> #52 -> #53 at a time.

Within #59, follow `docs/SPRITES.md` exactly:

```text
S0 -> S1
 -> SR0 -> SR1 -> SR2 -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Within #67, follow the already-created child issues exactly:

```text
#69 E0 Game/Application boundary
 -> #70 E1 Project manifest + external build/install/package
 -> #71 E2 Scene hierarchy + typed components
 -> #72 E3 Input Actions + gamepad/mouse/text/IME
 -> #73 E4 TileSet/TileMap
 -> #74 E5 production UTF-8 font/text/localization
 -> #75 E6 practical deterministic UI layout/widgets
 -> #76 E7 Physics2D
 -> #77 E8 Audio
 -> #78 E9 Linux/compiler/toolchain hardening
 -> #79 E10 persistence + schema migration
```

After #67, complete #12 before Mesh2D. #60 then completes M0 -> M1. #61 stops at SP0 unless explicit owner license approval exists.

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
- small enhancement that does not preempt a later owner-fixed architecture.

Independent work must not:

- compete with/duplicate the active core implementation,
- silently redefine a future subsystem contract,
- add an unreviewed dependency/license obligation,
- violate determinism/ownership/performance boundaries,
- introduce broad speculative infrastructure,
- change product goals or human-gated decisions.

When overlap exists, prefer coordination/rebase over two competing implementations.

Issue #80 tracks this policy explicitly.

## Do not ask unnecessary owner questions

A continuation request authorizes normal implementation decisions already constrained by repository contracts.

Do not ask the owner to choose when:

- execution order already chooses the next task,
- the active issue has sufficient acceptance criteria,
- architecture/performance/determinism rules determine a safe narrow solution,
- an implementation detail is reversible and does not change product goals.

When evidence is incomplete for a non-owner decision, prefer the simplest reversible design consistent with existing contracts, add tests/measurement, and document the tradeoff.

Do not invent a human gate because a task is difficult.

## Recognized human gates

Human/owner intervention is reserved for real decisions that existing contracts intentionally do not delegate:

1. merge gate when policy/tooling genuinely requires owner merge,
2. explicit architecture/product-goal change,
3. particle CPU/GPU backend choice where #51/#53 require human selection,
4. Spine #61 SP0 license/integration decision,
5. credentials/billing/paid external service authorization,
6. unresolved dependency/distribution/legal decision.

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
- Keep SDL/backend/protocol types behind their owning boundaries.
- Do not add allocation to known per-frame hot paths without evidence and documentation.
- Reuse persistent/capacity-managed state where steady-state work would otherwise recreate resources.
- Do not create custom allocators, lock-free queues, generic ECS machinery, job systems, reflection, render graphs, material graphs, plugin ABIs, or bespoke containers before requirements/measurement justify them.
- Stable observable identity never uses raw pointers or allocation order.
- Deterministic observable output must not depend on unordered/unspecified iteration order.
- Prefer direct simple O(N) scans over speculative indexing until real workloads justify the index.
- No performance claim becomes fact without a reproducible workload and clear metric boundary.

## Agent-first rules

- MCP is an adapter, not the engine API.
- Engine/Agent APIs remain protocol-independent.
- Headless and windowed execution share authoritative runtime/game logic.
- Tests/agents explicitly control simulation time.
- Authored project/scene/component/asset/UI/tile/input/persistence metadata stays text-first and diffable where practical.
- Semantic identity/selectors beat coordinate targeting.
- Structured runtime state beats pixel/audio inference for semantic correctness.
- Machine-facing commands return stable structured diagnostics and predictable exit behavior.
- Expensive snapshots/fingerprints/reports/capture/migration/generation remain request/setup work, not ordinary frame work.

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

New behavior should have automated tests whenever practical. Visual/audio/GPU behavior may require local presentation evidence, but semantic/math/order/import/serialization/query behavior should remain headless-CI testable whenever practical.

When #78 is complete, the added non-MSVC platform/toolchain becomes part of the normal required validation contract.
