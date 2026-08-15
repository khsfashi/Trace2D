# Deterministic Input

Trace2D input is an engine-level contract that is independent of SDL event objects and physical devices.

The low-level input foundation lets gameplay, tests, and coding agents feed one deterministic `InputSystem`. #72 I0 added resolved semantic actions, I1 added normalized gamepad/pointer/device state, and I2 adds versioned project-authored input maps plus an explicit deterministic rebinding boundary.

## Authority chain

Runtime input remains one authority:

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

Authored input configuration feeds that authority only at setup/rebind boundaries:

```text
project-relative TOML input map
        |
        v
ParseInputMapToml / InputMapStore::Load
        |
        v
validated InputMapDocument
        |
        +---- explicit RebindControl / RebindAnalogAxis
        |
        v
BuildActionMap
        |
        v
finalized compact ActionMap
        |
        v
Application::CommitActions
```

There is no second gameplay input database. Parsing, serialization, filesystem access, and semantic rebinding are not part of fixed-step resolution.

## Public runtime types

The engine-facing runtime API lives in `engine/input`:

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
- `ActionMap` — finalized compact semantic bindings plus current fixed-frame state.

I2 additionally exposes setup/persistence types:

- `InputMapDocument` — validated authored button/Axis1D definitions,
- `InputMapDiagnostic` — structured parse/schema/reference/stale-write diagnostics,
- `InputMapStore` — explicit project-root + project-relative file load/save boundary,
- `BuildActionMap` — deterministic authored-document -> finalized runtime-map build,
- `RebindControl` / `RebindAnalogAxis` — explicit authored-state edits with stale-write preconditions.

SDL types do not appear in these APIs or in the authored file format.

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

The result is then multiplied by the binding scale. Negative scale may be used for explicit inversion. Deadzone/scale are frozen by `Finalize()` and are not dynamically discovered or recalculated from device metadata during gameplay.

### Programmatic setup

Definitions may still be authored directly during `Game::OnStart`:

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

## I2 authored input-map format

I2 uses versioned TOML with engine-owned control/axis names:

```toml
format_version = 1

[[buttons]]
id = "jump"
controls = ["Space", "GamepadSouth"]

[[axes1d]]
id = "move_x"
negative = "KeyA"
positive = "KeyD"
analog = [
  { axis = "GamepadLeftX", deadzone = 0.2, scale = 1.0 },
]
```

The format can express:

- button actions with one or more controls,
- digital Axis1D negative/positive pairs,
- analog Axis1D bindings,
- frozen deadzone and scale,
- digital + analog composition on the same semantic Axis1D action.

It deliberately does not serialize SDL scancodes, SDL gamepad enum values, runtime device IDs, platform paths, or discovered device metadata.

### Validation and diagnostics

Parsing/validation rejects or diagnoses:

- unsupported format versions,
- unknown fields,
- missing/empty semantic IDs,
- duplicate semantic IDs across button and Axis1D actions,
- unknown controls or axes,
- duplicate button controls,
- incomplete or identical digital Axis1D pairs,
- duplicate analog axes,
- deadzones outside `[0, 1)`,
- zero or non-finite/out-of-range scales/numbers,
- invalid project-relative references,
- stale rebinding preconditions.

Diagnostics carry an error code, semantic/schema path, message, and TOML source line/column when available. File-backed diagnostics also carry the canonical project-relative reference and resolved path.

### Stable compact runtime IDs

`BuildActionMap` canonicalizes semantic definitions before constructing the runtime map:

- button actions sort by semantic ID,
- Axis1D actions sort by semantic ID,
- button controls sort by canonical engine control name,
- analog bindings sort by canonical engine axis name.

Compact runtime IDs therefore do not depend on incidental authored table order. This keeps save diffs and Agent edits stable while gameplay retains direct indexed reads.

### Canonical serialization

`SaveInputMapToml` emits one deterministic representation using the same canonical ordering and locale-independent numeric formatting. The contract is:

```text
valid document
 -> save canonical text A
 -> load A
 -> save canonical text B
 -> A == B
```

Serialization is setup/persistence work and may allocate. It is never called by `ActionMap::Resolve`.

## I2 deterministic rebinding

Rebinding edits `InputMapDocument`, not the active finalized `ActionMap`.

Both rebinding APIs require the caller to provide the expected current binding. If the authored state has changed and that binding is no longer present, the edit fails with `stale_binding` rather than overwriting newer state.

A successful flow is:

```text
load/retain InputMapDocument
 -> RebindControl or RebindAnalogAxis
 -> BuildActionMap
 -> persist canonical document if desired
 -> Application::CommitActions(finalized replacement)
```

The old finalized runtime map is never incrementally mutated.

### Rebinding edge semantics

`Application::CommitActions` calls `ActionMap::Synchronize` against the authoritative current `InputSystem` before replacing the active map.

`Synchronize`:

- computes current button held state from the new bindings,
- computes current Axis1D state from the new bindings,
- clears semantic `pressed` and `released` flags.

This prevents a newly rebound action from synthesizing a press merely because its replacement physical control was already held when the map changed. A later real release is still observed normally.

`CommitActions` accepts only a finalized map and is intended for explicit host safe boundaries: before `Start`, or between `StepFrames` calls on the owning application thread. I2 does not introduce concurrent input-map mutation.

## Project reference and persistence boundary

I2 does **not** silently widen or version-bump `trace2d.project.json`.

`InputMapStore` receives an explicit project root, and every load/save receives an explicit project-relative reference such as:

```text
config/gameplay.input.toml
```

Absolute paths and `..` traversal are rejected. There is no implicit filesystem search or convention-based discovery in I2.

If/when the canonical project manifest gains an input-map field, that remains an explicit compatibility/versioning decision rather than an accidental side effect of #72.

File save validates the complete document, writes canonical text through a sibling temporary, and replaces only at this explicit persistence boundary. Broader crash-safe/general save-system guarantees remain owned by #79 rather than being duplicated inside input.

## Application fixed-step delivery

Every fixed frame follows this runtime order:

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

Normal already-connected gameplay input processing does not perform semantic string lookup, filesystem work, device discovery, TOML parsing, serialization, report generation, or ordinary heap allocation. `ActionMap::Resolve` and `ActionMap::Synchronize` are allocation-free after finalization.

I2 setup/rebind work is allowed to allocate because it is explicit structural work outside the fixed-step hot path:

- parse/validate is O(authored document size),
- canonicalization sorts setup vectors,
- rebuilding allocates a new compact map,
- serialization/filesystem work occurs only on explicit persistence calls.

Gamepad connect/disconnect remains an explicit structural/hotplug boundary and may grow/compact the retained connected-device vector. Ordinary canonical gameplay reads never scan the device list.

For finalized `ActionMap` runtime resolution:

- semantic strings and binding vectors exist only in retained setup/inspection state,
- definitions are frozen before fixed-step gameplay,
- `Resolve` performs no semantic string lookup or filesystem work,
- `Resolve` performs no heap allocation after finalization,
- button resolution is O(total button bindings),
- axis resolution is O(total digital axis definitions + analog bindings),
- there is no per-frame map reconstruction, reflection/property-bag dispatch, TOML/JSON, SDL conversion, or device discovery.

## Remaining #72 slices after I2

I2 is not the full E3/#72 completion claim. The following remain explicit later #72 work:

- real UTF-8 text input and IME composition/editing event boundaries for focused UI,
- pointer presentation-to-viewport/world convenience APIs through the existing #88 contract where needed by #72/#75,
- explicit haptics/rumble ownership/support or a documented defer decision,
- any broader multiplayer/multi-user device routing only when a representative workload justifies it,
- touch/gesture/mobile lifecycle mapping when a supported mobile platform exists.

These must extend the same `InputSystem` / `ActionMap` authority rather than introduce parallel gameplay input paths. Do not advance the committed core lane to #73 until the live #72 acceptance contract is actually complete.
