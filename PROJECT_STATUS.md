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

Do not independently rebuild an already-solved engineering capability merely because custom implementation is possible. Prefer mature, maintained, well-tested implementations or proven contracts when their license, dependency weight, runtime cost, portability and authority model fit Trace2D. GitHub popularity is a discovery signal, not proof; relevance, maintenance, real use and evidence matter more.

While doing this mandatory benchmark pass, agents should also notice adjacent **already-proven** capabilities that could materially improve Trace2D. When one is genuinely compelling, finish/report the active task first and then ask the owner whether to adapt it. Such a suggestion is advisory only. It must not create an issue, reorder the core lane, add a dependency or start implementation until the owner explicitly approves it.

## Current product interpretation

Trace2D is not only the runtime engine. It also owns the **human-facing AI-operated production experience** built on the same canonical project/Agent/WorkResult state.

For visual assets, that product direction is explicit as **Asset Studio** / issue #318:

```text
asset-production request
 -> replaceable generator candidates
 -> existing deterministic Sprite processing/import
 -> Workspace showroom / review queue
 -> owner choose / feedback / approve
 -> approved project asset library
 -> canonical SpriteAsset / animation
 -> game use
```

Asset Studio is a Trace2D responsibility, not a standalone TracePixel editor product. Umbrella contract: `docs/ASSET_STUDIO.md`. The set-level production-intent contract promoted by #320 is documented in `docs/ASSET_PRODUCTION_SPEC.md`.

TracePixel may remain a separate deterministic raster R&D lab and may upstream a technique only after matched evidence shows a concrete Trace2D benefit.

## Completed production foundation

The minimum external-game sequence through B2 is complete:

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

#315 is complete. Do not reopen it merely to obtain a different presentation result; use its retained evidence and owner feedback as the product-proof checkpoint.

## Frozen Sprite continuity

The completed production Sprite program remains frozen continuity for Asset Studio and later work. In particular:

- **#144 / SA0** deterministic Sprite animation timing/frame/event contract remains complete and frozen,
- SA1-SA4 remain the authoritative deterministic Sprite animation continuation built on SA0,
- SR0-SR8 remain the production Sprite renderer/presentation authority,
- SPP0-SPP5 remain the deterministic processing, extraction, quality/repair, import, generator-manifest and provider-neutral generation-orchestration authority,
- SE2E/SPERF remain end-to-end and performance evidence continuity.

Asset Studio must reuse these contracts rather than creating parallel Sprite truth or weakening frozen animation/runtime semantics.

## Immutable benchmark lessons

### B1

B1 remains immutable pre-improvement evidence. Its important product lesson is that runtime correctness and Agent usability are separate: several Trace2D final workspaces verified successfully despite Agent-budget failure. Do not rerun B1 to obtain a preferred score.

### B2

B2 and acceptance-v1-v5 remain consumed evidence. V5 ended in Agent/CLI completion-result timeout after partial workspace side effects; no gameplay-verifier or presentation-quality defect was demonstrated by that run. Do not create V6 merely to force a pass.

These results reinforce the need for real product proofs and compact Agent surfaces instead of benchmark-shaped architecture growth.

## Current core lane — #318 Asset Studio

The exact current product checkpoint is:

> **#318 — Asset Studio: AI-operated sprite production, showroom and asset library**

AS0 contract/responsibility gap analysis is complete. It found four genuinely missing product slices while preserving existing authorities:

```text
#320 set-level production spec + Art Profile references
 -> #321 bounded Candidate Set + Workspace showroom
 -> #322 approved asset lineage/library metadata
 -> #323 one-provider end-to-end batch proof
```

Existing #97 WorkSpec remains intent/Definition-of-Done/completion authority; #98 WorkResult remains result/revision evidence authority; #99 Workspace remains human review authority; SPP5 remains provider-neutral generation-call authority; canonical Sprite/resource contracts remain runtime authority.

## Active implementation slice — #320

#320 owns only committed set-level production intent and Art Profile references.

Required boundary:

- stable production-set identity,
- explicit requested Sprite items/count/class,
- exact dimensions,
- required animation/direction deliverables,
- structural constraints,
- bounded candidate/provider-call budget intent,
- explicit owner-review intent,
- compact Art Profile identity plus approved canonical project-asset references.

#320 must not add:

- another completion state machine,
- provider/model/workflow configuration,
- autonomous aesthetic scoring,
- candidate/showroom lifecycle,
- lineage/history database,
- runtime/frame-loop dependency.

The representative committed proof is 10 coherent 64x64 forest-monster Sprites with idle/walk/attack requirements and a separate #97 human-approval gate.

## Exact successor — #321 Candidate Set / showroom

After #320 merges green, the exact next implementation slice is:

> **#321 — bounded Candidate Set + Workspace showroom**

#321 should compose generated candidates, deterministic findings and owner review presentation without moving completion truth into a GUI-only database. Candidate status/revision evidence belongs in the existing WorkResult/Workspace authority boundary.

Do not jump to #322/#323 or #89 while #321 is incomplete unless live GitHub state shows a real blocker and the owner explicitly promotes a detour.

## Asset Studio target experience

The product north star is set-level asset production, for example:

```text
"현재 프로젝트 스타일로 64x64 숲 몬스터 10종을 만들고
idle / walk / attack까지 준비해줘."
```

Expected flow:

```text
AI creates a bounded candidate set
 -> objective failures are rejected cheaply
 -> Workspace shows surviving candidates and evidence
 -> owner picks / rejects / asks for alternatives or targeted changes
 -> revisions remain bound to exact results
 -> approved assets enter the project library
 -> game can use canonical assets immediately
```

Aesthetic approval remains human. Do not implement `generate until aesthetic_score >= threshold`.

## Fixed continuation lane

The owner-fixed continuation is now:

```text
#318 Asset Studio (#320 -> #321 -> #322 -> #323)
 -> #89 Material2D / Shader2D
 -> #90 deterministic tween
 -> #76 Physics2D
 -> #77 Audio
 -> #91 profiler
 -> #78 Linux / non-MSVC toolchain
 -> #92 tiered GPU QA
 -> #79 save / migration
 -> #12 broad flagship external game
 -> #60 Mesh2D
 -> #177 Asset Intelligence / Asset IR
 -> #176 native deterministic skeleton
 -> #61 Spine license gate
```

#312 Semantic Project Graph / Project Index remains research-gated and does not enter this lane automatically.

## Continuation rule

The next `@GitHub Trace2D 다음 진행해줘` must resolve live state first.

- If a #320 PR is open, finish its review/CI/merge before starting #321.
- Once #320 closes green, #321 is the exact next Asset Studio slice.
- Once #321 closes green, continue #322, then #323.
- Only after the #318 checkpoint/slices close does normal breadth resume at #89.
- Live issue/PR/CI state plus `config/trace2d.core-lane.json` wins if explanatory prose becomes stale.
