---
name: semantic-menu-flow
description: Use when a retained semantic UI button or menu activation must drive exactly-once game state or screen-flow transitions.
---

# Semantic menu flow

## Canonical workflow

1. Author a retained `UiDocument` button with stable semantic `id`/`name`; normal pointer/navigation and Agent automation must converge on the same semantic element.
2. Treat `UiElement::activationCount` as retained activation evidence. Game logic owns a consumed-activation cursor for each transition source.
3. During deterministic game/update work, consume each new activation exactly once (`while consumed < activationCount`) and mutate canonical game/screen state.
4. Render/present the resulting state after the game transition. The UI activation is an input fact; canonical game state remains the transition authority.
5. Agent workflows select the element semantically (`id`, role, name), activate it, then verify both UI activation state and resulting game state.

## Authority and ordering

`semantic UI activation -> exactly-once consumption -> canonical game/screen state -> presentation`

## Do not

- Do not use screen coordinates when a stable semantic selector exists.
- Do not make the automation call itself a hidden second transition authority.
- Do not replay the same activation side effect every frame; retain a consumed count/cursor.
- Do not infer successful gameplay transition from a screenshot alone when engine-owned state is inspectable.

## Discovery handoff

Load `ui`, `gameplay`, and `scene` surfaces. Exact UI mutation semantics live in `trace2d/ui/Ui.hpp`; Agent runtime primitives remain discoverable through MCP `tools/list`.

## Deterministic verification

Query the button, record/expect its activation count, activate once, advance the required deterministic game step, and assert the intended game state changed once. Re-run the consumer without another activation and prove state is unchanged. Useful primitives: `trace2d.ui.query`, `trace2d.ui.activate`, `trace2d.ui.assert`, `trace2d.runtime.step`, `trace2d.query`, `trace2d.assert_float`.
