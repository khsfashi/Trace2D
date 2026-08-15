# Trace2D Project Status

Last explanatory handoff update: **2026-08-15**

This file is context, not live state authority. Operational next action is derived from committed `config/trace2d.core-lane.json` plus live GitHub issue/PR/CI state through the repository-state tooling. When they disagree, live repository state and the committed lane win.

## Product rule

Trace2D is an AI-first / AI-operated C++20 2D engine with the product rule:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Verification rule:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

## Current core lane

The external-game production foundation has completed:

```text
#69 Game/Application
 -> #70 external project / SDK package
 -> #71 Scene hierarchy + typed authored components
 -> #86 unified typed resource lifecycle
 -> #87 scene templates + world lifecycle
 -> #88 Camera2D + Viewport2D
 -> #72 Input Actions / device input / text-IME
 -> #73 deterministic TileSet / TileMap / TileLayer
```

#73 E4 was delivered in bounded slices:

- T0 / PR #201 — versioned TileSet/TileMap authoring, compact 8-byte compiled cells, deterministic semantic inspection,
- T1 / PR #203 — arithmetic viewport-window culling and Sprite-path presentation/batching with measured zero-chunk baseline,
- T2 / PR #205 — finite collision/navigation/occlusion handoff metadata plus independent semantic markers,
- T3 / PR #207 — deterministic cardinal terrain-rule authoring compiled at setup into ordinary TileMap cells,
- T4 / issue #208 — deterministic generated/dense TileMap companion conversion, representative large-map evidence, and final E4 chunk-policy closure.

The T4 generated format remains import/setup data only. It converts into the existing canonical `TileMapDocument` and does not create a second steady-state renderer/runtime authority. For the current bounded dense runtime, T1 already proves visible-window-proportional frame work on a 1024 x 1024 layer, and T4 adds no evidence requiring retained chunk metadata. Streaming/infinite worlds remain a future evidence-driven promotion.

**After #208/#73 merges green, the exact next core-lane item is #74 — production UTF-8 font/text/localization.** Do not jump to #75 or the later Agent-authoring/benchmark work while #74 remains open or has an active implementation PR.

## #73 final authority boundary

```text
text-authored TileMap
terrain-rule authoring
or generated dense companion
 -> deterministic setup-time canonical TileMapDocument
 -> compact compiled TileMap runtime
 -> semantic cell/marker handoff
 -> viewport-window Sprite presentation
```

Normal compiled cell access remains O(1) resolved-layer/indexed storage with no filesystem access, string lookup, per-cell heap object, generated-format parsing, GPU readback or per-tile draw API. Terrain preprocessing and generated companion conversion remain explicit setup/offline work.

Actual physics remains #76. Actual navigation/lighting remain #93. Animated tiles, richer diagonal/blob/Wang terrain rules, runtime terrain rebuild scheduling and streaming/infinite worlds are explicit deferrals rather than hidden E4 behavior.

Detailed T4 closure contract: `docs/TILEMAP_E4_T4.md`.

## Frozen Sprite continuity

The completed production Sprite program remains frozen continuity for later work. In particular:

- #144 / SA0 deterministic Sprite animation timing/frame/event contract remains complete and frozen,
- SR8 renderer conformance and SA4 animation conformance remain trusted,
- SPERF performance evidence remains the production Sprite baseline.

Later work must not weaken these contracts.

## Benchmark B1 — immutable baseline

B1 remains immutable pre-improvement evidence:

- exact scored head: `6d6904e99ad7060341861cb3823e04591a579bf7`,
- owner scored workflow: `31763107941`,
- scored artifact id: `9206626314`,
- artifact SHA-256: `74ab53220927f557621c96ee7b8df7395010e60c191d3959705ab7ba09f8d4d6`,
- Godot Generic: 5/9,
- Trace2D Agent: 3/9,
- selected Godot Agent: 0/9.

The merged #175 postmortem freezes the important interpretation: all six scored-unsuccessful Trace2D slots exceeded the 100k input-token budget while their final workspaces independently verified successfully. Do not rerun, replace, repair, or retrospectively alter the scored B1 cohort.

## Agent Complexity Budget

For the demonstrated single-resource deterministic repair class, keep the product-surface target:

- one discoverable public `trace2d` authoring root,
- no mandatory raw text edit or Git metadata,
- at most one primary semantic mutation,
- at most one deterministic validation call after mutation,
- expected authored revision count one,
- zero visual-feedback calls required for deterministic acceptance,
- compact structured output by default.

Do not solve context pressure by merely increasing prompts/budgets or proliferating benchmark-shaped tools.

## Fixed continuation lane

The committed continuation remains:

```text
#74 production UTF-8 font/text/localization
 -> #75 practical deterministic UI
 -> #178 Sprite transactional Agent authoring
 -> #179 Particle transactional Agent authoring
 -> #104 Benchmark B2
 -> #89 Material2D / Shader2D
 -> #90 deterministic tween
 -> #76 Physics2D
 -> #77 Audio
 -> #91 profiler
 -> #78 Linux / non-MSVC toolchain
 -> #92 tiered GPU QA
 -> #79 save / migration
 -> #12 flagship external game
 -> #60 Mesh2D
 -> #177 Asset Intelligence / Asset IR
 -> #176 native deterministic skeleton
 -> #61 Spine license gate
```

#178 and #179 remain required before #104 B2 and must not be pulled forward ahead of #74-#75 unless the repository owner explicitly changes the fixed lane.

## Continuation rule

The next `@GitHub Trace2D 다음 진행해줘` must resolve live state first.

- If the T4/#208 implementation PR is still open, continue/fix that PR until its exact-head required gates are green and #73 is closed.
- If #73 is closed and #74 is open with no implementation PR, #74 is the exact next implementation item.
- If #74 has an implementation PR, continue/fix #74 only until its exact-head required gates are green.
- Only after #74 merges green does the core lane advance to #75.
- If live GitHub state conflicts with this handoff, live issue/PR/CI state plus `config/trace2d.core-lane.json` wins.
