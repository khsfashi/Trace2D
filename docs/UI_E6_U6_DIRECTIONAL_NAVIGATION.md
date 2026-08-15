# E6 U6 — deterministic directional focus navigation

Parent: #75  
Implementation issue: #236

## Purpose

U5 established forward/backward authored-order focus traversal and focused activation. U6 adds the smallest spatial navigation layer required by practical keyboard/gamepad menus without creating a retained focus graph, moving physical binding policy into UI, or changing the existing focus mutation authority.

## Input ownership

Physical controls remain owned by #72 Input Actions. A host resolves semantic actions once during setup and maps their fixed-step edges to the protocol-independent UI operation, for example:

```text
ArrowLeft / GamepadDpadLeft
  -> ActionMap "ui.left"
  -> pressed edge
  -> UiDocument::FocusDirectional(UiNavigationDirection::Left)
```

The same applies to right/up/down. `UiDocument` never needs to know which physical device produced the semantic edge.

## Geometry authority

Directional navigation consumes the already-resolved absolute logical-canvas `UiElement::bounds` produced by U0-U3. It does not rerun layout or resolve semantic IDs.

Rectangle centers use doubled integer coordinates:

```text
center2_x = 2 * x + width
center2_y = 2 * y + height
```

This preserves exact half-pixel center distinctions for odd extents without floating-point rounding.

## Candidate and ranking contract

A candidate must:

- not be the currently focused element,
- be visible,
- be enabled,
- have a focusable kind (`Button` or `TextInput`),
- lie in the requested strict center half-plane.

For the requested axis, define the positive directional distance as `primary` and the absolute perpendicular offset as `secondary`.

Candidates are ranked deterministically by:

1. inside the 45-degree directional cone (`secondary <= primary`) before outside,
2. smaller squared center distance (`primary^2 + secondary^2`),
3. smaller `secondary`,
4. smaller `primary`,
5. smaller retained authored element index.

The cone preference makes directional intent dominant over a slightly closer but strongly off-axis control. If no in-cone candidate exists, the best candidate in the requested half-plane is still selected. If no directional candidate exists, focus is preserved and `not_focusable` is returned.

Successful selection delegates to the existing `FocusIndex()` authority. Therefore pointer focus, authored-order navigation, directional navigation, and IME composition clearing all share the same focus-change semantics.

## Performance boundary

Directional navigation runs only on an explicit navigation edge.

For `N` retained UI elements:

- exactly one O(N) contiguous scan is sufficient,
- all geometry/ranking math is integer,
- no allocation or container growth occurs,
- no semantic-ID/string lookup occurs,
- no hierarchy/layout recomputation occurs,
- no TOML/filesystem work occurs,
- no renderer/GPU work occurs,
- no retained spatial index/focus graph is maintained.

A retained directional graph would add setup memory and invalidation complexity without current workload evidence.

## Deliberate deferrals

U6 does not add:

- modal/focus scopes or focus trapping,
- clipping/scissor,
- scrolling/content translation,
- image/progress/scroll widgets,
- arbitrary callback/event graphs.

Those remaining #75 slices should continue to consume the same resolved hierarchy, logical rectangles, authored order, and direct runtime indices.
