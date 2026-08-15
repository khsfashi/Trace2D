# E6 U7 — bounded deterministic modal focus scope

Parent: #75  
Implementation issue: #238

## Purpose

U4-U6 established one interaction authority for pointer capture, authored-order focus traversal, directional focus, focused activation, and IME focus changes. U7 adds the smallest bounded modal/focus-scope layer above that authority so a modal subtree can trap interaction without mutating authored visibility/enabled state or creating a second event system.

## Scope authority

`UiDocument` owns zero or one active modal root. Membership is defined only by the resolved U0-U3 hierarchy:

```text
active modal root
 + root itself
 + every element whose retained parentIndex chain reaches the root
 = interaction-eligible subtree
```

`SetModalScope(id)` is an explicit state transition. It resolves the semantic root once, validates that root, and rebuilds one retained byte-per-element membership array. `ClearModalScope()` removes the filter without changing authored element state.

A modal stack, generic event/callback graph, and independent focus graph are deliberately not introduced.

## Interaction convergence

The existing authorities remain canonical:

- semantic `Focus(id)` delegates to `FocusIndex()`,
- semantic `Activate(id)` delegates to `ActivateIndex()`,
- forward/backward traversal keeps authored order,
- directional traversal keeps U6 integer geometry ranking,
- pointer press/release keeps U4 direct-index capture and activation,
- text/IME focus changes keep the existing focus/composition path.

U7 adds only a direct-index eligibility gate. Targets outside the active subtree return `outside_modal_scope` for direct focus/activation/text actions and are skipped by traversal/hit testing.

When a new scope is activated, focus outside it is cleared through the existing focus authority (therefore active IME composition is cleared), and outside hover/capture state is cancelled deterministically.

## Pointer consumption

While a modal scope is active, valid logical pointer input is considered consumed even when no modal child is hit. This prevents a click outside the modal from falling through to background UI/gameplay. Hit testing itself continues to use reverse authored/painter order, but only among active-scope members.

Physical presentation coordinates still pass through #88 viewport gating/conversion before reaching `UiDocument`; U7 adds no renderer/window dependency.

## Performance boundary

Scope activation/change is explicit state-management work and may allocate only when the retained membership capacity has not already been prepared by `ReserveElements()`.

For `N` retained elements and hierarchy depth `D`, a scope rebuild is bounded by O(N * D) parent-index walking. This cost does not occur on ordinary frames.

Steady interaction adds only direct-index membership checks:

- no semantic string lookup beyond explicit `SetModalScope` / semantic action entry points,
- no hierarchy discovery during pointer or focus scans,
- no allocation/container growth per event,
- no TOML/filesystem work,
- no layout rebuild,
- no renderer/GPU work,
- no retained spatial/focus graph.

Existing U4/U5/U6 scans remain O(N) on explicit pointer/navigation work; U7 does not change their asymptotic complexity.

## Structural boundary

Production-authored hierarchy is finalized before interactive modal use. If setup code adds elements after a modal membership array was prepared, it must explicitly call `SetModalScope` again to rebuild membership before those new elements can participate. This keeps `AddElement` free of modal-time allocation/bookkeeping and preserves the setup/runtime ownership boundary.

## Deliberate deferrals

U7 does not add:

- nested modal stacks,
- arbitrary event bubbling/capture callback graphs,
- clipping/scissor or scroll translation,
- image/progress/scroll widgets,
- render-time dimming/backdrop policy,
- a retained spatial index.

Clipping/scroll and practical widgets remain later #75 slices.
