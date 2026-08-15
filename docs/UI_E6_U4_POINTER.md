# E6 U4 — deterministic pointer hit testing and capture

Parent: #75  
Implementation issue: #231

## Purpose

U0-U3 established one engine-owned hierarchy/layout authority and resolved logical-canvas rectangles. U4 consumes those rectangles directly for physical pointer interaction instead of adding a second UI geometry or callback model.

## Coordinate authority

`UiDocument::ApplyPointer()` accepts **logical UI-canvas coordinates** only. Physical/window coordinates remain owned by the already-merged #88 presentation mapping:

```text
InputSystem::Pointer()                  presentation coordinates
 -> IsPresentationPointInsideViewport  reject fit bars / outside target
 -> PresentationToViewport             #88 logical coordinates
 -> UiDocument::ApplyPointer            UI hit/event state
```

UI does not reproduce camera/viewport math and does not link against Render merely to route logical input. Hosts that already operate in logical coordinates can call `ApplyPointer()` directly.

The point-in-rectangle rule is half-open on both axes:

```text
x <= point.x < x + width
y <= point.y < y + height
```

This matches the continuous pixel-edge convention frozen by #88.

## Interactive and overlap rules

U4 deliberately keeps the pointer vocabulary small:

- `Button` and `TextInput` are pointer-interactive.
- `Panel` and `Label` are pointer-transparent in this slice.
- among overlapping interactive elements, the **last authored element wins**, matching reverse painter/authored order,
- invisible elements are skipped,
- a visible disabled interactive element still occupies the top hit target and therefore does not click through to an element underneath; it cannot focus or capture.

This is an O(N) reverse contiguous scan over the already-retained `UiElement` array. No spatial tree is retained until representative UI workloads demonstrate a need.

## Hover, press and capture

`UiDocument` retains direct runtime indices for the hovered and captured elements. `UiElement` exposes semantic `hovered` and `pointerPressed` state for headless inspection.

Primary-button routing is deterministic:

1. resolve the topmost logical hit,
2. update the single hovered element,
3. on press, an enabled interactive hit receives focus and direct-index capture,
4. capture persists while the pointer leaves the element,
5. on release, capture ends,
6. a captured `Button` activates only when release occurs inside that same element,
7. release outside cancels activation,
8. a captured `TextInput` keeps focus but never receives Button activation semantics.

The aggregate input state may contain `pressed=true` and `released=true` in one fixed frame. U4 processes press first and release second, producing exactly one deterministic click.

If a captured target becomes hidden, disabled, or non-interactive before the next sample, capture is cancelled before release handling. If a caller misses a release-transition sample and later observes the primary button up, stale capture is also cleared.

## Shared activation authority

Physical pointer release and semantic Agent activation converge on the same internal `ActivateIndex()` mutation:

```text
physical pointer release inside captured Button
 -> ActivateIndex(index)
 -> UiElement::activationCount

AgentFacade::ActivateUi(selector)
 -> setup/inspection selector query
 -> UiDocument::Activate(id)
 -> ActivateIndex(index)
 -> UiElement::activationCount
```

The Agent path may resolve a semantic selector because it is an explicit automation operation. Normal pointer movement/press/release never performs semantic-ID lookup.

Agent UI snapshots/assertions expose:

- `hovered`,
- `pointerPressed`,
- `pointerCaptured`,
- the existing focus and activation state.

Pixels are not required to verify pointer semantics.

## Performance boundary

For an already-loaded UI document, normal pointer routing is allocation-free and performs:

- one O(N) reverse scan for the topmost hit,
- O(1) direct-index hover/capture/focus/activation mutations,
- finite scalar geometry checks.

It performs no:

- TOML/filesystem work,
- hierarchy or layout reconstruction,
- semantic selector/string lookup,
- per-event object allocation,
- GPU submission/readback,
- renderer/camera lookup.

This bounded linear baseline is intentionally simpler and cheaper in retained memory than a speculative spatial index. Promote an acceleration structure only if measured practical UI workloads justify it.

## Deliberate deferrals

U4 does not add:

- capture/bubble callback graphs,
- arbitrary pointer listeners on panels/labels,
- clipping/scissor or scroll routing,
- modal/focus scopes,
- keyboard/gamepad focus traversal,
- drag/drop widgets,
- image/progress/scroll widgets,
- touch/mobile claims before a supported mobile platform exists.

Those remaining #75 requirements build on the same resolved hierarchy, logical coordinates, direct indices and semantic state established by U0-U4.
