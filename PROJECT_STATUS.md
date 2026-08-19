# Trace2D Project Status

Last explanatory handoff update: **2026-08-19**

This file is context, not live state authority. Operational next action is derived from committed `config/trace2d.core-lane.json` plus live GitHub issue/PR/CI state.

## Product rule

Trace2D is an AI-first / AI-operated C++20 2D engine with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Product optimization target:

> **Author -> Run -> Observe -> Verify -> Revise through a compact semantic surface.**

A second owner rule now governs breadth:

> **Subsystem existence is not enough. Repeatedly prove that public Trace2D surfaces can make an actual game/product workflow.**

See `docs/PRODUCT_PROOF_POLICY.md`.

## Global engineering benchmark rule

All non-trivial Trace2D work must follow `docs/EXTERNAL_REFERENCE_PROTOCOL.md` before the design is frozen.

The default is:

```text
recover Trace2D decisions
 -> benchmark mature current GitHub/public implementations
 -> check whether direct reuse is practical
 -> ADOPT / ADAPT / REJECT / DEFER
 -> implement only the smallest missing Trace2D-owned part
 -> verify the borrowed lesson with Trace2D evidence
```

Do not independently rebuild an already-solved engineering capability merely because custom implementation is possible. Prefer mature, maintained, well-tested implementations or proven contracts when their license, dependency weight, runtime cost, portability and authority model fit Trace2D.

TraceResearch owns controlled comparative claims about Agent efficiency. Trace2D may retain representative product/Agent evidence, but must not grow benchmark-specific architecture merely to improve a research score.

## Completed production foundation

The minimum external-game sequence through B2 and the first playable proof is complete:

```text
#69 Game/Application
 -> #70 external project / SDK package
 -> #71 Scene hierarchy + typed authored components
 -> #86 unified typed resource lifecycle
 -> #87 scene templates + world lifecycle
 -> #88 Camera2D + Viewport2D
 -> #72 Input Actions / device input / text-IME
 -> #73 deterministic TileSet / TileMap / TileLayer
 -> #74 production UTF-8 font/text/localization
 -> #75 practical deterministic UI
 -> #178 Sprite transactional Agent authoring
 -> #179 Particle transactional Agent authoring
 -> #104 Benchmark B2
 -> #315 tiny external playable product proof
```

#315 is consumed evidence. Do not reopen it merely to obtain a preferred presentation result.

## Frozen Sprite continuity

The completed production Sprite program remains frozen continuity for later work. In particular:

- **#144 / SA0** deterministic Sprite animation timing/frame/event contract remains complete and frozen,
- SA1-SA4 remain the authoritative deterministic Sprite animation continuation built on SA0,
- SR0-SR8 remain the production Sprite renderer/presentation authority,
- SPP0-SPP5 remain the deterministic processing, extraction, quality/repair, import, generator-manifest and provider-neutral generation-orchestration authority,
- SE2E/SPERF remain end-to-end and performance evidence continuity.

Later systems must compose these contracts rather than create parallel Sprite truth or weaken frozen animation/runtime semantics.

## Immutable benchmark lessons

### B1

B1 remains immutable pre-improvement evidence. Its important product lesson is that runtime correctness and Agent usability are separate: several Trace2D final workspaces verified successfully despite Agent-budget failure. Do not rerun B1 to obtain a preferred score.

### B2

B2 and acceptance-v1-v5 remain consumed evidence. V5 ended in Agent/CLI completion-result timeout after partial workspace side effects; no gameplay-verifier or presentation-quality defect was demonstrated by that run. Do not create V6 merely to force a pass.

These results reinforce the need for real product proofs and compact Agent surfaces instead of benchmark-shaped architecture growth.

## Asset Studio checkpoint — minimal foundation complete

#318 remains the long-term Asset Studio umbrella, but it is not an operational blocker.

### #320 — production intent / Art Profile — COMPLETE

Merged via PR #325.

### #321 — bounded candidate comparison substrate — COMPLETE

Merged via PR #326. It remains intentionally minimal: candidate grouping/references compose existing WorkResult/Workspace authority and do not own provider config, lifecycle/approval state, aesthetic score, lineage DB or runtime truth.

### #322 / #323 — DEFERRED

Approved-library lineage and one-provider batch proof stay behind the broader production foundation and the new product-proof checkpoints. Do not resume Asset Studio depth early.

## Fixed continuation lane — owner amendment 2026-08-19

Operational order is now:

```text
#89  Material2D / Shader2D
 -> #90  Tween / Sequence
 -> #327 Presentation Product Proof
 -> #76  Physics2D
 -> #77  Audio
 -> #329 Combat Product Proof
 -> #91  Profiler
 -> #78  Linux / non-MSVC toolchain
 -> #92  tiered GPU QA
 -> #330 Cross-platform Stress Product Proof
 -> #79  Save / Migration
 -> #331 Save Product Proof
 -> #322 approved asset lineage
 -> #323 one-provider Asset Studio batch proof
 -> #12  broad flagship external game
 -> #60  Mesh2D
 -> #177 Asset Intelligence / Asset IR
 -> #176 native deterministic skeleton
 -> #61  Spine license gate
```

Machine authority is `config/trace2d.core-lane.json`.

#328 Presentation Recipes is a long-lived product vocabulary/backlog, not a blocking lane item. Recipes are pulled into concrete product proofs as needed rather than implemented speculatively.

#312 Semantic Project Graph / Project Index remains research-gated and does not enter this lane automatically.

## Material / Tween / presentation direction

### #89 Material2D / Shader2D — COMPLETE

#89 is complete through MAT1-MAT4: typed fixed-capacity parameters, canonical #86 Shader2D/Material2D ownership, lifecycle-safe cached custom fragment execution, exact contiguous painter-order batching, deterministic failure/cache evidence and explicit retained pipeline/attachment costs. It adds no public render graph, material graph or effect-specific C++ vocabulary.

Finished effects do not become engine-core features merely because they use a shader.

### #90 Tween / Sequence — COMPLETE

#90 completed the DOTween-class practical surface while keeping Trace2D semantics explicit and allocation-disciplined:

- explicit `simulation` time domain for authoritative fixed-step tweening,
- explicit `presentation` time domain advanced only by supplied/manual delta with no hidden wall-clock query,
- resolved typed property/material bindings,
- finite stable easing vocabulary,
- absolute/relative and capture-current-on-start semantics,
- first-class `Append`, `Join`, `Insert`, `Interval`, repeat/yoyo and deterministic conflict policies,
- stale-safe reusable pooled storage/generational identity or equivalent,
- no steady-step string lookup/heap allocation at retained capacity,
- presentation completion/callbacks cannot become hidden gameplay authority.

#327 subsequently consumed this surface in the bounded Presentation Product Proof; do not reopen #90 for speculative tween breadth.

### #327 Presentation Product Proof — COMPLETE

#327 is consumed product evidence for Material2D + Tween composition. Continue to use #328 as a recipe vocabulary/backlog rather than promoting one-off visual effects into engine-core systems.

## Physics2D V1 — complete through PHYS3

#76 is complete through the bounded PHYS1/PHYS2/PHYS3 program on pinned Box2D 3.1.1:

- PHYS1 established typed body/collider authoring, fixed-step Scene authority, stable raycasts, lifecycle pruning and the private Box2D backend;
- PHYS2 added stable contact/sensor events plus bounded circle/box overlap queries;
- PHYS3 adds runtime linear/angular velocity control, dynamic center force/impulse, coherent teleport, and bounded stable circle/rotated-box shape casts.

The V1 production-depth decisions are explicit in `docs/PHYSICS2D_PHYS3.md`: bullet/continuous-collision policy is supported through the pinned backend, shape casts are supported, and friction/restitution remain finite typed collider scalars. Compound colliders, joints/constraints, one-way platforms, a Trace2D-owned character controller, hierarchy-aware rigid bodies, debug geometry, custom hit thresholds and extra force/torque conveniences are deferred until representative product evidence demonstrates a common need.

Do not open a speculative PHYS4 merely to exhaust the wishlist. The next core-lane subsystem is #77 Audio; #329 Combat Product Proof is where any concrete Physics2D gap should be rediscovered and minimized.

## Official Presentation Recipes

Trace2D owns an **Official Presentation Recipe** layer above engine primitives and below game-specific art direction. See `docs/PRESENTATION_RECIPES.md` and umbrella #328.

Representative recipe vocabulary may include hit flash, outline, dissolve, palette/UV effects, button punch, panel slide, damage-number motion, screen flash and composite hit impact.

Binding promotion rule:

```text
external technique / real game need
 -> first reproduce with existing Trace2D primitives as a recipe
 -> prove it in a real sample + retain cost/quality evidence
 -> only if multiple independent recipes repeatedly need the same missing capability,
    or measured performance/quality cannot be achieved,
    promote the smallest common primitive into engine core
```

Do not implement `one cool effect = one C++ subsystem`.

## Recurring Product Proof rule

Product proofs are deliberately inserted between groups of substantial subsystems:

- #89 + #90 -> #327 presentation playground,
- #76 + #77 -> #329 small combat/game-feel loop,
- #91 + #78 + #92 -> #330 same-workload cross-platform/stress proof,
- #79 -> #331 real exit/restart/migration proof.

A proof should primarily compose public capabilities. If it exposes a concrete gap, fix the smallest demonstrated gap. Preserve owner-rejected/failed evidence rather than rerunning solely to erase an unfavorable result.

## Continuation rule

Every future `@GitHub Trace2D 다음 작업 진행해줘` resolves live state first.

- #321 is complete; do not return to Asset Studio depth.
- #89, #90 and #327 are complete and consumed.
- **#76 Physics2D V1 is complete through PHYS3; #77 Audio is the exact next core-lane item.**
- Do not create speculative PHYS4 breadth; let #329 expose any smallest concrete follow-up after Audio.
- Follow the later proof checkpoints in `config/trace2d.core-lane.json`.
- Do not start #322/#323 before #331 is accepted.
- Live issue/PR/CI state plus `config/trace2d.core-lane.json` wins if explanatory prose becomes stale.
