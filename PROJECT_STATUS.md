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

All non-trivial Trace2D work must follow `docs/EXTERNAL_REFERENCE_PROTOCOL.md` before design is frozen:

```text
recover Trace2D decisions
 -> benchmark mature current public implementations
 -> check direct reuse
 -> ADOPT / ADAPT / REJECT / DEFER
 -> implement only the smallest missing Trace2D-owned part
 -> verify with Trace2D evidence
```

Do not independently rebuild an already-solved capability merely because a custom implementation is possible.

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

The completed Sprite program remains authoritative continuity:

- SA0-SA4 own deterministic Sprite animation,
- SR0-SR8 own production Sprite renderer/presentation,
- SPP0-SPP5 own deterministic processing/import/provider-neutral generation orchestration,
- SE2E/SPERF remain end-to-end/performance evidence.

Later systems must compose these contracts rather than create parallel Sprite truth.

## Asset Studio checkpoint — deliberately split

#318 remains the long-term Asset Studio product umbrella, but it is no longer an operational blocker after the minimal current foundation.

### #320 — production intent / Art Profile — COMPLETE

Merged via PR #325. `AssetProductionSpec` records set/item intent, candidate/provider budgets and approved style references while reusing exact #97 human-review acceptance authority. It owns no provider config, candidate lifecycle, aesthetic score or runtime state.

### #321 — bounded candidate comparison substrate — CURRENT

#321 is intentionally the final immediate Asset Studio slice before engine breadth resumes.

It adds only the missing comparison grouping:

- production-set/item identity,
- exact WorkResult/work revision binding,
- stable candidate id + bounded ordinal,
- references to artifacts already owned by that exact revision,
- Workspace composition that rejects stale revisions and unknown artifacts.

It must not add candidate approval/rejection state, provider/model/workflow fields, provider cost DB, aesthetic scores, approved-library lineage, live generation, GUI-only truth or runtime work.

External-reference decision for #321:

- InvokeAI Gallery/Boards: **ADAPT** output grouping/recallable metadata; **REJECT** its application DB as project truth.
- ComfyUI history/output references: **ADAPT** stable output references; **REJECT** workflow-graph/queue identity as Trace2D candidate truth.
- Trace2D #98/#99: **ADOPT** existing revision/artifact/review/approval/stale-action authority.

See `docs/ASSET_CANDIDATE_SET.md`.

### #322 / #323 — DEFERRED

Do not continue directly to Asset Studio lineage/provider depth after #321.

#322 approved-library lineage and #323 one-provider batch proof are postponed until the broader production foundation through #79 is complete. This avoids freezing the production model around today's Sprite-heavy capability set.

## Fixed continuation lane — owner amendment 2026-08-19

Operational order is now:

```text
#321 minimal candidate comparison substrate
 -> #89 Material2D / Shader2D
 -> #90 deterministic Tween
 -> #76 Physics2D
 -> #77 Audio
 -> #91 Profiler
 -> #78 Linux / non-MSVC toolchain
 -> #92 tiered GPU QA
 -> #79 Save / Migration
 -> #322 approved asset lineage
 -> #323 one-provider Asset Studio batch proof
 -> #12 broad flagship external game
 -> #60 Mesh2D
 -> #177 Asset Intelligence / Asset IR
 -> #176 native deterministic skeleton
 -> #61 Spine license gate
```

#318 stays open as a long-lived product umbrella but is intentionally absent from the operational lane so it cannot prevent #89 after #321.

#312 Semantic Project Graph / Project Index remains research-gated and does not enter this lane automatically.

## Material / presentation direction already reserved

#89 is the exact next breadth step after #321. Its existing contract intentionally provides a compact programmable 2D rendering surface for practical effects such as hit flash, outline, dissolve, grayscale, palette/UV processing and custom fragment color effects without creating a material graph or public render graph.

#90 follows with deterministic tweening suitable for transforms, colors, material parameters, camera motion and UI transitions.

These low-level primitives should support higher-level reusable presentation recipes without promoting every visual trick into a bespoke engine subsystem.

## Continuation rule

Every future `@GitHub Trace2D 다음 작업 진행해줘` resolves live state first.

- If a #321 PR is open, finish its review/CI/merge.
- Once #321 closes green, **#89 is exact next**.
- Do not start #322/#323 before the lane reaches them after #79.
- Live issue/PR/CI state plus `config/trace2d.core-lane.json` wins if explanatory prose becomes stale.
