# E6 U5 — deterministic focus navigation and focused activation

Parent: #75  
Implementation issue: #234

## Purpose

U0-U4 established one engine-owned UI hierarchy/layout authority, logical-canvas pointer routing, and direct-index focus/capture state. U5 adds the smallest keyboard/gamepad-compatible navigation layer on top of that same runtime state instead of introducing a second focus graph or physical-key policy inside UI.

## Input ownership

Physical controls remain owned by #72 Input Actions. A host resolves semantic actions once during setup and maps their fixed-step edges to UI operations, for example:

```text
ArrowDown / GamepadDpadDown
  -> ActionMap "ui.next"
  -> pressed edge
  -> UiDocument::FocusNext()

Enter / GamepadSouth
  -> ActionMap "ui.accept"
  -> pressed edge
  -> UiDocument::ActivateFocused()
```

`UiDocument` does not know which physical keyboard/gamepad controls produced those semantic actions. Agent operations may continue to address semantic element IDs explicitly because they are inspection/automation operations, while ordinary navigation uses only retained indices and authored order.

## Traversal contract

Focus eligibility is exactly the existing finite UI rule:

- element is visible,
- element is enabled,
- `IsFocusable(kind)` is true (`Button` or `TextInput`).

`FocusNext()` scans forward in retained authored/painter order. `FocusPrevious()` scans backward. Both wrap exactly once.

When no element is focused:

- forward traversal starts at the first authored element,
- backward traversal starts at the last authored element.

Hidden, disabled, `Panel`, and `Label` elements are skipped. If no eligible element exists, traversal returns `not_focusable` and does not create focus.

All successful traversal ends in the existing `FocusIndex()` authority. Therefore moving away from an active `TextInput` clears IME composition through the already-established focus-change rule. If only one eligible element exists, wrap selects the same index and composition is not cleared because focus did not actually change.

## Focused activation

`ActivateFocused()` requires an existing focused index and delegates directly to the existing `ActivateIndex()` authority.

This preserves one Button activation counter regardless of how activation arrived:

```text
pointer release inside captured Button
AgentFacade::ActivateUi(id)
UiDocument::ActivateFocused()
        -> ActivateIndex(index)
        -> UiElement::activationCount
```

A focused `TextInput` remains focusable but not activatable and returns `not_activatable`.

## Performance boundary

Navigation work occurs only on an explicit focus-move edge.

For `N` retained UI elements:

- `FocusNext()` / `FocusPrevious()` perform at most one O(N) contiguous scan,
- `ActivateFocused()` is O(1),
- no semantic-ID/string lookup occurs in normal navigation,
- no allocation or container growth occurs,
- no hierarchy/layout recomputation occurs,
- no TOML/filesystem work occurs,
- no renderer/GPU work occurs.

A retained focus-order cache or directional spatial graph would add setup/memory/invalidation complexity without current workload evidence, so U5 deliberately keeps the contiguous scan.

## Deliberate deferrals

U5 does not add:

- geometric left/right/up/down focus search,
- modal/focus scopes,
- focus trapping,
- clipping/scissor,
- scrolling/content translation,
- image/progress/scroll widgets,
- arbitrary callback/event graphs.

Those remaining #75 slices should continue to consume the same hierarchy, authored order, logical coordinates, and direct runtime indices rather than creating parallel UI state.
