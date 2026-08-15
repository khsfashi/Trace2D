# E3 / I4 Input Closure

Issue #72 E3 is completed by preserving one deterministic input authority and composing it with the already-public #88 camera/viewport conversion authority. I4 does not add another runtime input layer, camera abstraction, or backend-specific gameplay API.

## Final authority split

```text
physical host / virtual test or Agent source
 -> engine-owned InputEvent / TextInputEvent
 -> InputSystem fixed-frame state
 -> finalized ActionMap semantic state
 -> gameplay
```

Pointer coordinates remain presentation-space input facts until a caller that owns a resolved #88 view needs viewport/world meaning:

```text
InputSystem::Pointer()
 -> IsPresentationPointInsideViewport(resolvedView, presentationPoint)
 -> PresentationToViewport(resolvedView, presentationPoint)
 -> PresentationToWorld(resolvedView, presentationPoint) when world space is required
```

The input module does not depend on Render and does not cache camera state. The render/camera module does not own pointer state. SDL types remain confined to Platform.

## Pointer routing rule

A physical or virtual pointer reports absolute presentation coordinates through the same `PointerState` contract. For `ViewportScaleMode2D::Fit`, callers must reject letterbox/pillarbox coordinates with `IsPresentationPointInsideViewport` before treating the point as a gameplay/UI target. `Fill` and `Stretch` still require the point to lie inside the actual presentation target according to #88.

`PresentationToViewport` and `PresentationToWorld` remain the only conversion authority. I4 deliberately does not add Input wrappers for these functions because that would duplicate ownership and create an Input -> Render dependency solely for convenience.

The committed integration test injects a pointer with `VirtualInputSource`, observes the canonical `InputSystem::Pointer()` state, converts the presentation center through #88 to the logical viewport and world center, and rejects a point in a fit-mode letterbox bar. It requires no SDL window, GPU, or physical input device.

## Haptics / rumble

Haptics are deliberately deferred from E3.

The implemented input direction is device/user -> deterministic simulation state. Rumble is simulation -> physical device output and needs a separate protocol-independent output-request lifetime/capability/error contract. Treating rumble as another `InputEvent` or semantic action would reverse ownership and make headless behavior misleading.

A later owner-promoted device/platform breadth stage may add haptics when a representative workload justifies it. That contract must keep SDL/device handles private to Platform, report unsupported/device-loss state explicitly, and never make headless gameplay correctness depend on physical rumble execution.

## Touch, gesture, and mobile lifecycle

E3 does not claim mobile platform support. Touch/gesture/mobile lifecycle input is deliberately deferred until broader-platform work is promoted from #93 after the portability foundation in #78.

When mobile exists, touch contacts and gestures must feed the same engine-owned semantic action model and/or established pointer-routing semantics rather than creating a second gameplay input database. #75 may reuse the pointer/event model for UI touch behavior when such a platform exists.

## Multiple-user device routing

The I1 desktop contract remains the supported scope: one canonical primary gameplay gamepad, retained secondary state, deterministic failover on primary disconnect, plus desktop keyboard/mouse input.

Per-player device assignment, split keyboard ownership, or a multiplayer routing framework is deferred until a concrete local-coop/multi-user workload requires it. Ordinary action reads therefore remain free of repeated device-list scans.

## Performance boundary

I4 adds no production frame-path code.

- no new per-frame allocation,
- no filesystem or serialization work,
- no semantic string lookup,
- no camera lookup inside Input,
- no repeated device discovery for ordinary action reads,
- no GPU/window requirement for deterministic pointer conversion acceptance,
- no duplicate retained pointer/camera state.

The only new executable coverage is a headless composition test over existing public APIs.

## E3 completion

With I0-I4, #72 now covers its scoped desktop production contract:

- resolved semantic button and Axis1D actions,
- normalized gamepad buttons/axes and deterministic deadzones,
- mouse/pointer position, delta, and wheel,
- primary-gamepad connection/hotplug/failover semantics,
- versioned project-authored input maps and deterministic rebinding,
- physical/virtual input parity,
- real UTF-8 committed text and IME composition/preedit delivery to focused UI,
- explicit #88 pointer presentation/viewport/world routing,
- explicit haptics/mobile/multi-user support-or-defer decisions.

Production font/glyph rendering remains #74. Rich textbox editing, focus/navigation, and physical pointer event routing/hit testing remain #75. Mobile/platform promotion remains #93. The fixed core lane advances exactly one step to #73 TileSet/TileMap after the I4 closure PR is merged green.
