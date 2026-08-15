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
```

#72 E3 was delivered in bounded slices:

- I0 / PR #191 — resolved semantic button and Axis1D actions,
- I1 / PR #193 — normalized gamepad, pointer and hotplug input,
- I2 / PR #195 — versioned authored input maps and deterministic rebinding,
- I3 / PR #197 — real UTF-8 committed text and IME composition/preedit delivery,
- I4 / issue #198 — pointer conversion/platform-scope closure using the existing #88 authority.

I4 adds no production frame-path code. `PointerState` presentation coordinates compose directly with `IsPresentationPointInsideViewport`, `PresentationToViewport`, and `PresentationToWorld`; no Input -> Render dependency or duplicate camera state is introduced. Haptics/rumble, mobile touch/gesture lifecycle, and broader per-player device routing are explicit deferred platform/device-breadth work rather than accidental omissions. The detailed closure contract is `docs/INPUT_E3_I4_CLOSURE.md`.

**The exact next core-lane item is #73 — deterministic TileSet / TileMap / TileLayer authoring, rendering and semantic QA.** Do not jump to #74 while #73 is open or has an active implementation PR.

## #72 final authority boundary

```text
physical / virtual / Agent source
 -> engine-owned input or text event
 -> InputSystem fixed-frame state
 -> finalized ActionMap semantic state
 -> gameplay
```

Pointer conversion remains a composition boundary rather than an input-owned renderer dependency:

```text
InputSystem::Pointer()
 -> #88 presentation-inside-viewport gate
 -> presentation -> logical viewport
 -> presentation -> world when required
```

Production text glyph/font rendering remains #74. Rich textbox editing, hierarchy/focus/navigation, pointer hit testing/capture/event routing and practical widgets remain #75. Mobile platform promotion remains governed by #93 after the portability foundation.

## Performance boundary

Normal fixed-step work must not acquire filesystem parsing, semantic string lookup, generic reflection/property-bag dispatch, tracing GC, hidden shared-ownership churn, mandatory GPU readback, or repeated device discovery. I4 preserves this by adding only headless integration coverage over existing public APIs.

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

The merged #175 postmortem also freezes the important interpretation: all six scored-unsuccessful Trace2D slots exceeded the 100k input-token budget while their final workspaces independently verified successfully. Do not rerun, replace, repair, or retrospectively alter the scored B1 cohort.

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
#73 TileMap
 -> #74 production UTF-8 font/text/localization
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

#178 and #179 remain required before #104 B2 and must not be pulled forward ahead of #73-#75 unless the repository owner explicitly changes the fixed lane.

## Continuation rule

The next `@GitHub Trace2D 다음 진행해줘` must resolve live state first.

- If a #73 implementation PR is open, continue/fix #73 only until its exact-head required gates are green.
- If #73 is open with no implementation PR, #73 is the exact next implementation item.
- Only after #73 merges green does the core lane advance to #74.
- If live GitHub state conflicts with this handoff, live issue/PR/CI state plus `config/trace2d.core-lane.json` wins.
