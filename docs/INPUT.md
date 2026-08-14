# Deterministic Input

Trace2D input is an engine-level contract that is independent of SDL event objects and physical devices.

The low-level input foundation lets gameplay, tests, and coding agents feed one deterministic `InputSystem`. #72 I0 adds a resolved semantic action layer on top so gameplay does not need to treat `KeyW`, `Space`, or `MouseLeft` as game meaning.

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

- `InputControl` — stable low-level engine control identifiers
- `InputEvent` — press/release event for one low-level control
- `InputControlState` — low-level held/pressed/released state
- `InputSystem` — authoritative low-level state and deterministic frame scheduler
- `VirtualInputSource` — test/Agent convenience source that feeds the same `InputSystem`
- `ButtonActionId` — resolved semantic button identity
- `Axis1DActionId` — resolved semantic 1D axis identity
- `ButtonActionState` — semantic held/pressed/released state
- `ActionMap` — setup-time semantic definitions/bindings plus fixed-frame resolved state

SDL types do not appear in these APIs.

## Low-level frame semantics

After reset, the input frame is `0` and all controls are clear.

Scheduled events must target a frame strictly later than the current input frame. An event scheduled for frame `N` is applied when input advances to frame `N`, before gameplay for that frame consumes the state.

`AdvanceToFrame` may advance across multiple frames. Intermediate scheduled events still update held state in deterministic order, while `pressed` and `released` describe transitions on the final current frame only. Deterministic gameplay runners should normally advance one simulation frame at a time so every transient transition can be observed.

Input frames never move backwards.

For a low-level control that is not held:

- press -> `held=true`, `pressed=true`
- release -> no state change

For a control that is held:

- repeated press -> no new transition
- release -> `held=false`, `released=true`

Transient `pressed` and `released` flags are cleared when advancing to the next input frame.

If multiple scheduled events target the same frame, insertion order is preserved. A press followed by a release on the same frame therefore ends not held with both transition flags visible for that frame.

## I0 semantic actions

`ActionMap` separates game meaning from low-level controls.

A button action has a stable semantic ID such as `jump` or `attack` and may bind more than one low-level control. Its held state is the OR of its bindings. Semantic press/release transitions are derived from the aggregate held state so releasing one binding while another remains held does not create a false release. A complete press+release tap within one fixed frame preserves both semantic edge flags.

A 1D axis has one negative and one positive low-level binding. I0 digital composition is exact:

```text
negative only -> -1
neither       ->  0
both          ->  0
positive only -> +1
```

Values are clamped to `[-1, 1]`.

### Setup and finalization

Definitions and bindings are setup-time state:

```cpp
auto jump = actions.AddButtonAction("jump");
actions.BindButton(jump, trace2d::input::InputControl::Space);

auto moveX = actions.AddAxis1DAction(
    "move_x",
    trace2d::input::InputControl::KeyA,
    trace2d::input::InputControl::KeyD);

actions.Finalize();
```

Semantic names may be resolved explicitly through `FindButtonAction` / `FindAxis1DAction` during setup or inspection. Normal gameplay should retain the returned compact IDs and read by ID.

`Finalize()` rejects incomplete button actions and freezes action definitions/bindings. Mutating definitions after finalization is an error.

### Application fixed-step delivery

`Application` lets `Game::OnStart` define actions and finalizes them when startup completes. Every fixed frame then follows this order:

```text
InputSystem::AdvanceToFrame(nextFrame)
 -> apply pending host input
 -> ActionMap::Resolve(InputSystem)
 -> FixedStepRuntime::Step()
 -> Game::OnFixedUpdate
```

`GameContext::Actions()` therefore exposes the semantic state for the exact authoritative fixed frame seen by `OnFixedUpdate`.

This ordering is shared by headless scheduled input and host-delivered low-level events.

## Physical SDL input

`engine/platform` translates supported SDL events into engine-owned input events before they reach gameplay.

The currently implemented physical boundary maps:

- keyboard A-Z
- arrow keys
- Space, Enter, Escape
- left, middle, and right mouse buttons

A windowed caller pumps `PlatformEvent`; an input event is queued for the next fixed frame through `Application::ApplyInput` or equivalent host integration.

I0 does not add SDL types to `ActionMap` or gameplay code.

## Virtual input

`VirtualInputSource` supports immediate press/release and frame-indexed scheduled press/release operations against the same `InputSystem` used by physical input.

For application-level deterministic tests, scheduling low-level events and allowing `Application` to resolve the semantic map is the preferred proof that Agent/test and physical-style sources converge before gameplay.

## Reset and replay semantics

`InputSystem::Reset`:

- clears all low-level held/pressed/released state,
- returns the current input frame to `0`,
- clears all scheduled events,
- resets the scheduler cursor.

`ActionMap::ResetState` clears only derived semantic button/axis runtime state and preserves finalized definitions and retained storage. An explicit replay/reset path resets both authorities before replaying the same low-level event sequence.

## Performance policy

Low-level gameplay state is a fixed-size `std::array` indexed by `InputControl`.

`InputSystem::ApplyEvent`, low-level state reads, and per-frame transient clearing allocate no memory. `AdvanceToFrame` only walks the already-authored schedule and fixed control state; scheduling itself is setup/scenario work and may grow its retained vector.

For `ActionMap`:

- semantic strings and binding vectors exist only in setup/inspection state,
- definitions are frozen before fixed-step gameplay,
- semantic reads are direct vector indexing by resolved ID,
- `Resolve` performs no semantic string lookup or filesystem work,
- `Resolve` performs no heap allocation after finalization,
- button resolution is O(total button bindings),
- digital axis resolution is O(number of defined 1D axes),
- there is no per-frame map reconstruction, generic reflection, JSON, or SDL conversion in gameplay.

This is intentionally a compact resolved representation rather than an `unordered_map<string, Variant>`-style hot-path API.

## Remaining #72 slices after I0

I0 is not the full E3/#72 completion claim. The following remain explicit later #72 work:

- physical gamepad buttons and analog axes with frozen deadzone semantics,
- mouse position, delta, and wheel delivery,
- project-authored/rebinding persistence contract,
- text input and IME composition/event boundaries,
- controller connect/hotplug/reconnect and multi-device policy,
- pointer presentation-to-viewport/world conversion through the existing #88 contract,
- explicit haptics/rumble support or documented defer decision,
- touch/gesture/mobile lifecycle mapping when a supported mobile platform exists.

These must extend the same semantic action authority rather than introduce parallel gameplay input paths.
