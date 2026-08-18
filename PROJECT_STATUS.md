# Trace2D Project Status

Last explanatory handoff update: **2026-08-18**

This file is context, not live state authority. Operational next action is derived from committed `config/trace2d.core-lane.json` plus live GitHub issue/PR/CI state.

## Product rule

Trace2D is an AI-first / AI-operated C++20 2D engine with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Product optimization target:

> **Author -> Run -> Observe -> Verify -> Revise through a compact semantic surface.**

## Current product interpretation

Trace2D is not only the runtime engine. It also owns the **human-facing AI-operated production experience** built on the same canonical project/Agent/WorkResult state.

For visual assets, that product direction is now explicit as **Asset Studio** / issue #318:

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

Asset Studio is a Trace2D responsibility, not a standalone TracePixel editor product. Contract: `docs/ASSET_STUDIO.md`.

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
```

The complete Sprite program through SPP5/SE2E/SPERF remains frozen production continuity. In particular, Asset Studio must reuse existing Sprite processing, extraction, quality/repair, import, generator interop, provider-neutral generation orchestration, canonical SpriteAsset, animation and renderer authority rather than creating parallel systems.

## Immutable benchmark lessons

### B1

B1 remains immutable pre-improvement evidence. Its important product lesson is that runtime correctness and Agent usability are separate: several Trace2D final workspaces verified successfully despite Agent-budget failure. Do not rerun B1 to obtain a preferred score.

### B2

B2 and acceptance-v1-v5 remain consumed evidence. V5 ended in Agent/CLI completion-result timeout after partial workspace side effects; no gameplay-verifier or presentation-quality defect was demonstrated by that run. Do not create V6 merely to force a pass.

These results reinforce the need for real product proofs and compact Agent surfaces instead of benchmark-shaped architecture growth.

## Current core lane — #315 playable product proof

The exact current product item is:

> **#315 — tiny external playable product proof**

Required owner loop:

```text
human intent
 -> Agent authors an external game
 -> build/run
 -> deterministic verification
 -> presentation evidence
 -> owner actually plays/reviews it
 -> one concrete owner feedback request
 -> Agent revision
 -> deterministic re-verification
 -> owner approval or preserved rejection
```

#315 intentionally uses current capabilities. New broad engine subsystems remain blocked unless #315 proves a concrete blocker and the owner promotes the minimum general fix.

## Exact successor — #318 Asset Studio

After #315 closes, the next product checkpoint is now:

> **#318 — Asset Studio: AI-operated sprite production, showroom and asset library**

This comes **before #89 Material2D/Shader2D**.

The first #318 stage is AS0 contract/responsibility gap analysis. It must map the desired production experience against existing Trace2D SPP/Sprite/Workspace/WorkResult/#178 contracts before adding new implementation.

AS0 should answer:

- what Trace2D already solves,
- what production-orchestration state is genuinely missing,
- what Workspace/showroom state is genuinely missing,
- what project asset-library metadata is genuinely missing,
- whether one external generator adapter is required for a real proof,
- which TracePixel/external-tool ideas are research-only or redundant.

Do not begin by adding multiple providers, another raster QA stack or a GUI-only asset database.

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
#315 tiny external playable product proof
 -> #318 Asset Studio AS0 / owner-approved slices
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

- While #315 is open, continue #315; do not jump to #318.
- Once #315 closes, #318 AS0 is the exact next product checkpoint.
- AS0 is a contract/gap-analysis stage, not authorization for a large Asset Studio implementation.
- Only after #318's owner-approved checkpoint/slices close does normal breadth resume at #89.
- Live issue/PR/CI state plus `config/trace2d.core-lane.json` wins if explanatory prose becomes stale.
