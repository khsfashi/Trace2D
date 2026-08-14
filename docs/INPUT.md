# Deterministic Input

Trace2D input is an engine-level contract that is independent of SDL event objects and physical devices.

The low-level input foundation lets gameplay, tests, and coding agents feed one deterministic `InputSystem`. #72 I0 added resolved semantic button/digital-axis actions. I1 extends the same authority with normalized gamepad axes/buttons, pointer motion/wheel, and explicit gamepad connection state.

## Authority chain

```text
physical / test / Agent input
        |
        v
engine-owned InputEvent
        |
        v
InputSystem fixed-frame state
        |
        v
ActionMap::Resolve
        |
        v
semantic button / axis state
        |
        v
gameplay / Agent inspection
```

There is no second input authority. Physical and virtual sources converge before semantic gameplay state is derived.

## Public types

The engine-facing API lives in `engine/input`:

- `InputControl` — stable low-level button/key identifiers,
- `InputAxis` — stable normalized low-level analog identifiers,
- `InputDeviceId` — runtime connection identity for supported explicit devices,
- `InputEvent` — engine-owned button/axis/pointer/device event,
- `InputControlState` — low-level held/pressed/released state,
- `PointerState` — absolute pointer position plus fixed-frame delta/wheel accumulation,
- `InputSystem` — authoritative low-level state and deterministic frame scheduler,
- `VirtualInputSource` — test/Agent convenience source that feeds the same `InputSystem`,
- `ButtonActionId` — resolved semantic button identity,
- `Axis1DActionId` — resolved semantic 1D axis identity,
- `ButtonActionState` — semantic held/pressed/released state,
- `ActionMap` — setup-time semantic definitions/bindings plus fixed-frame resolved state.

SDL types do not appear in these APIs.

## Low-level frame semantics

After reset, the input frame is `0` and all controls/axes/pointer/device state are clear.

Scheduled events must target a frame strictly later than the current input frame. An event scheduled for frame `N` is applied when input advances to frame `N`, before gameplay for that frame consumes the state.

`AdvanceToFrame` may advance across multiple frames. Intermediate scheduled events still update persistent state in deterministic order, while button `pressed`/`released` and pointer delta/wheel describe transitions on the final current frame only. Deterministic gameplay runners should normally advance one simulation frame at a time so every transient transition can be observed.

Input frames never move backwards.

For a low-level button/control that is not held:

- press -> `held=true`, `pressed=true`,
- release -> no state change.

For a held control:

- repeated press -> no new transition,
- release -> `held=false`, `released=true`.

Transient `pressed` and `released` flags, pointer delta, and pointer wheel are cleared when advancing to the next input frame. Pointer absolute position persists.

If multiple scheduled events target the same frame, insertion order is preserved. This also permits deterministic device-connect -> axis/button sequences on one fixed frame.

## Semantic actions

`ActionMap` separates game meaning from low-level controls.

A button action has a stable semantic ID such as `jump` or `attack` and may bind more than one low-level control. Its held state is the OR of its bindings. Semantic press/release transitions are derived from the aggregate held state so releasing one binding while another remains held does not create a false release. A complete press+release tap within one fixed frame preserves both semantic edge flags.

A 1D axis may have a digital binding, one or more analog bindings, or both. Digital composition is exact:

```text
negative only -> -1
neither       ->  0
both          ->  0
positive only -> +1
```

Analog bindings carry setup-time `deadzone` and `scale`. All contributions are accumulated and the final semantic value is clamped to `[-1, 1]`.

### Deadzone semantics

For a normalized input value `v` and frozen deadzone `d` in `[0, 1)`:

```text
abs(v) <= d -> 0
otherwise   -> sign(v) * (abs(v) - d) / (1 - d)
```

The result is then multiplied by the binding scale. Negative scale may be used for an explicit axis inversion. Deadzone/scale are frozen by `Finalize()` and are not dynamically discovered or recalculated from device metadata during gameplay.

### Setup and finalization

Definitions and bindings are setup-time state:

```cpp
auto jump = actions.AddButtonAction("jump");
actions.BindButton(jump, trace2d::input::InputControl::Space);
actions.BindButton(jump, trace2d::input::InputControl::GamepadSouth);

auto moveX = actions.AddAxis1DAction(
    "move_x",
    trace2d::input::InputControl::KeyA,
    trace2d::input::InputControl::KeyD);
actions.BindAxis1DAnalog(
    moveX,
    trace2d::input::InputAxis::GamepadLeftX,
    0.2F);

actions.Finalize();
```

Analog-only actions may be created with `AddAxis1DAction("look_x")` and then receive one or more analog bindings before finalization.

Semantic names may be resolved explicitly through `FindButtonAction` / `FindAxis1DAction` during setup or inspection. Normal gameplay should retain the returned compact IDs and read by ID.

`Finalize()` rejects incomplete actions and freezes action definitions/bindings. Mutating definitions after finalization is an error.

## Application fixed-step delivery

`Application` lets `Game::OnStart` define actions and finalizes them when startup completes. Every fixed frame then follows this order:

```text
InputSystem::AdvanceToFrame(nextFrame)
 -> apply pending host input
 -> ActionMap::Resolve(InputSystem)
 -> FixedStepRuntime::Step()
 -> Game::OnFixedUpdate
```

`GameContext::Input()` and `GameContext::Actions()` therefore expose low-level/semantic state for the same authoritative fixed frame seen by `OnFixedUpdate`.

This ordering is shared by headless scheduled input and host-delivered physical-style input.

## I1 gamepad contract

The current desktop baseline deliberately exposes one canonical gameplay gamepad at a time rather than pretending to provide a multiplayer device-routing framework.

- Each connected gamepad has a runtime `InputDeviceId`.
- Connection order is retained.
- The first connected gamepad is the primary gameplay gamepad.
- A later connection does not steal primary ownership.
- Secondary gamepad state is retained for predictable failover.
- When the primary disconnects, its canonical held controls release and axes clear; the next connected gamepad becomes primary using its already-retained state.
- A reconnect delivered by the host as a new runtime connection identity is treated as a new connection and joins the end of connection order.

Keyboard and mouse remain the combined desktop host devices for this slice. I1 does not claim independent keyboard/mouse hotplug identity or multiplayer keyboard/gamepad routing.

### Normalized gamepad axes

Platform normalization feeds the engine domain:

- left/right stick axes: `[-1, 1]`,
- left/right triggers: `[0, 1]`.

`InputSystem` clamps events to the appropriate domain before storing canonical state. `ActionMap` consumes only this engine-owned normalized representation; no SDL axis enum/value leaks into gameplay.

## I1 pointer contract

`PointerState` contains:

- absolute `x`, `y`,
- accumulated fixed-frame `deltaX`, `deltaY`,
- accumulated fixed-frame `wheelX`, `wheelY`.

Multiple host events in one fixed frame accumulate delta/wheel deterministically. Absolute pointer position persists across frames; delta/wheel reset at the next `AdvanceToFrame`.

The input subsystem does **not** convert pointer coordinates to world space. Presentation -> logical viewport -> world conversion remains owned by the #88 `Camera2D` / `Viewport2D` contract, including fit letterbox/pillarbox exclusion. Later #72/#75 pointer helpers must consume that authority rather than duplicate it.

## Physical SDL input

`engine/platform` translates supported SDL3 events into engine-owned `InputEvent` values before they reach gameplay.

The current physical boundary maps:

- keyboard A-Z,
- arrow keys,
- Space, Enter, Escape,
- left, middle, and right mouse buttons,
- mouse absolute position/delta/wheel,
- standard gamepad face/back/guide/start/stick/shoulder/d-pad buttons,
- standard gamepad left/right stick axes and triggers,
- gamepad added/removed connection events.

Opening/closing SDL gamepad handles and interpreting SDL event structures remain Platform responsibilities. Gameplay sees only engine-owned controls, axes, pointer values, and runtime device identities.

A windowed caller pumps `PlatformEvent`; an input event is queued for the next fixed frame through `Application::ApplyInput` or equivalent host integration.

## Virtual input

`VirtualInputSource` supports the same canonical low-level path for:

- immediate/scheduled control press/release,
- gamepad connect/disconnect,
- gamepad button press/release,
- normalized gamepad axis state,
- pointer movement,
- pointer wheel.

No SDL/window/GPU is required for deterministic input acceptance tests.

## Reset and replay semantics

`InputSystem::Reset`:

- clears all low-level held/pressed/released state,
- clears normalized axes and pointer state,
- clears connected gamepads,
- returns the current input frame to `0`,
- clears all scheduled events,
- resets the scheduler cursor while retaining vector capacity where the standard container permits reuse.

`ActionMap::ResetState` clears only derived semantic button/axis runtime state and preserves finalized definitions and retained storage. An explicit replay/reset path resets both authorities before replaying the same low-level event sequence.

## Performance policy

Canonical button and axis gameplay state use fixed-size arrays indexed by resolved enum identity. Semantic reads use direct vector indexing by resolved action ID.

Normal already-connected gameplay input processing does not perform semantic string lookup, filesystem work, device discovery, JSON/report generation, or ordinary heap allocation. `ActionMap::Resolve` is allocation-free after finalization.

Gamepad connect/disconnect is an explicit structural/hotplug boundary and may grow/compact the retained connected-device vector. Secondary-device event normalization may locate that connected device in the small connection list, but ordinary canonical gameplay reads never scan the device list. This avoids imposing hotplug/multi-device bookkeeping on every action read.

For `ActionMap`:

- semantic strings and binding vectors exist only in setup/inspection state,
- definitions are frozen before fixed-step gameplay,
- `Resolve` performs no semantic string lookup or filesystem work,
- `Resolve` performs no heap allocation after finalization,
- button resolution is O(total button bindings),
- axis resolution is O(total digital axis definitions + analog bindings),
- there is no per-frame map reconstruction, generic reflection, JSON, SDL conversion, or device discovery in gameplay.

## Remaining #72 slices after I1

I1 is not the full E3/#72 completion claim. The following remain explicit later #72 work:

- project-authored input-map/rebinding persistence with stable serialization and stale/invalid-binding diagnostics,
- real UTF-8 text input and IME composition/editing event boundaries for focused UI,
- pointer presentation-to-viewport/world convenience APIs through the existing #88 contract where needed by #72/#75,
- explicit haptics/rumble ownership/support or a documented defer decision,
- any broader multi-player/multi-user device routing only when a representative workload justifies it,
- touch/gesture/mobile lifecycle mapping when a supported mobile platform exists.

These must extend the same `InputSystem` / `ActionMap` authority rather than introduce parallel gameplay input paths.
