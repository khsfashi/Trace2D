# Deterministic Input

Trace2D input is an engine-level contract that is independent of SDL event objects and physical devices.

The P4 input foundation exists so gameplay code, tests, and coding agents can all consume the same `InputSystem` state while deterministic tests schedule input against exact simulation frames.

## Public types

The engine-facing API lives in `engine/input`:

- `InputControl` — stable engine control identifiers
- `InputEvent` — press/release event for one control
- `InputControlState` — held/pressed/released state
- `InputSystem` — authoritative gameplay-facing state and frame scheduler
- `VirtualInputSource` — test/agent convenience source that feeds the same `InputSystem`

SDL types do not appear in these APIs.

## Frame semantics

After reset, the input frame is `0` and all controls are clear.

Scheduled events must target a frame strictly later than the current input frame. A normal deterministic loop advances input and runtime together:

```text
schedule virtual input
        |
        v
InputSystem::AdvanceToFrame(nextFrame)
        |
        v
FixedStepRuntime::Step()
        |
        v
gameplay reads Held / Pressed / Released
```

An event scheduled for frame `N` is applied when input advances to frame `N`, before gameplay for that frame consumes the state.

`AdvanceToFrame` may advance across multiple frames. Intermediate scheduled events still update held state in deterministic order, while `pressed` and `released` describe transitions on the final current frame only. Deterministic gameplay runners should normally advance one simulation frame at a time so every transient transition can be observed.

Input frames never move backwards.

## Transition rules

For a control that is not held:

- press -> `held=true`, `pressed=true`
- release -> no state change

For a control that is held:

- repeated press -> no new transition
- release -> `held=false`, `released=true`

Transient `pressed` and `released` flags are cleared when advancing to the next input frame.

If multiple scheduled events target the same frame, insertion order is preserved. A press followed by a release on the same frame therefore ends not held with both transition flags visible for that frame.

## Physical SDL input

`engine/platform` translates supported SDL keyboard and mouse events into the same engine-owned `InputEvent` type used by virtual input.

The platform boundary currently maps:

- keyboard A-Z
- arrow keys
- Space, Enter, Escape
- left, middle, and right mouse buttons

A windowed caller pumps `PlatformEvent`; when its type is `PlatformEventType::Input`, the contained `InputEvent` is passed to `InputSystem::ApplyEvent`.

Gameplay code does not need to know whether the event came from SDL, a test, or an agent.

## Virtual input

`VirtualInputSource` supports immediate press/release and frame-indexed scheduled press/release operations.

Example:

```cpp
trace2d::input::InputSystem input{};
trace2d::input::VirtualInputSource virtualInput{input};

virtualInput.SchedulePress(1, trace2d::input::InputControl::KeyW);
virtualInput.ScheduleRelease(4, trace2d::input::InputControl::KeyW);

for (std::uint64_t frame = 1; frame <= 4; ++frame)
{
    input.AdvanceToFrame(frame);
    runtime.Step();
    // gameplay reads input here
}
```

This produces a press transition on frame 1, held state on frames 1-3, and a release transition on frame 4.

## Reset semantics

`InputSystem::Reset`:

- clears all held/pressed/released state
- returns the current input frame to `0`
- clears all scheduled events
- resets the scheduler cursor

The schedule vector retains its capacity after `clear`, so repeated test scenarios can reuse previously allocated storage.

## Performance policy

The gameplay-facing state is a fixed-size `std::array` indexed by `InputControl`.

`ApplyEvent`, state reads, and per-frame transition clearing allocate no memory. `AdvanceToFrame` only walks the already-authored schedule and fixed control state; it does not allocate while stepping.

Scheduling can allocate or move vector elements and is intentionally treated as scenario/setup work rather than a gameplay hot path. The initial implementation keeps this simple deterministic representation until profiling demonstrates a need for a different queue structure.
