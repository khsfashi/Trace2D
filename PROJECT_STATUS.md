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
 -> #74 production UTF-8 font/text/localization foundation
```

#74 E5 was delivered in bounded slices:

- PR #211 — real `FontResource` / FreeType face ownership, deterministic UTF-8 decoding and measurement foundation,
- PR #213 — bounded reusable CPU glyph atlas/cache with explicit capacity and rasterization metrics,
- PR #215 — bounded deterministic multiline layout, glyph-boundary wrapping and horizontal/vertical alignment,
- PR #217 — deterministic ordered fallback-font layout with explicit `fontSlot` authority,
- PR #219 — glyph atlas -> ordinary `TextureResource` -> existing Sprite presentation/GPU bridge,
- PR #221 — revision-keyed unchanged-text reuse, E3 semantic UI/IME integration, and Agent measured-layout inspection.

The final E5 text source contract is localization-service agnostic: callers provide stable source identity, a revision that changes when resolved display UTF-8 changes, and the resolved UTF-8 bytes. Cache hits compare only bounded source/options/fallback identity and do not rescan the string, rebuild layout, touch the glyph cache, rasterize, allocate, access files, or perform GPU work.

For focused E3 text input, active IME composition participates in **display layout only** while committed value and composition remain separate semantic state. Selection-only IME metadata changes do not relayout. Committing an unchanged visible preedit (for example `A + [한]` -> committed `A한`) also reuses the same layout; only semantic layout evidence is refreshed.

**After PR #221 / #74 merges green, the exact next core-lane item is #75 — practical deterministic UI.** Do not jump to #178/#179 or Benchmark B2 before #75 unless the repository owner explicitly changes the fixed lane.

Detailed E5 closure contract: `docs/TEXT_E5_F5.md`.

## E4 continuity

#73 remains the frozen deterministic TileMap foundation. Authored, terrain-rule, or generated dense setup data converges into the canonical compiled `TileMapDocument`, then semantic handoff plus viewport-window Sprite presentation. Streaming/infinite worlds remain evidence-driven future work; actual physics is #76 and navigation/lighting are #93.

Detailed E4 closure contract: `docs/TILEMAP_E4_T4.md`.

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
#75 practical deterministic UI
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

#178 and #179 remain required before #104 B2 and must not be pulled forward ahead of #75 unless the repository owner explicitly changes the fixed lane.

## Continuation rule

The next `@GitHub Trace2D 다음 진행해줘` must resolve live state first.

- If PR #221 / #74 is still open, continue/fix that exact implementation until its head gates are green and it is merged.
- If #74 is closed and #75 is open with no implementation PR, #75 is the exact next implementation item.
- If #75 has an implementation PR, continue/fix #75 only until its exact-head required gates are green.
- Only after #75 merges green does the core lane advance to #178.
- If live GitHub state conflicts with this handoff, live issue/PR/CI state plus `config/trace2d.core-lane.json` wins.
