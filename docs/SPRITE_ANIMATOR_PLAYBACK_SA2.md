# Sprite Animator Deterministic Playback Contract — SA2

Status: **active implementation contract for #148 under #59**.  
Predecessor: SA1 / #146 / PR #147 / squash `dc8909b24dc5f67e8bec2506263d7b433c6fb2f4`.  
Owning implementation: branch `agent/sprite-sa2-playback-events`.  
Exact successor after SA2 merges green: **SA3 — Agent/MCP inspection, actions, and exact-frame assertions**.

SA2 executes the timeline semantics frozen by SA0 using the typed authoritative state implemented by SA1. It remains renderer-independent and does not add Agent/MCP adapters or animation workload/conformance infrastructure.

## 1. Authority

The execution chain is:

```text
prepared SpriteAnimationClip2D
 + SpriteAnimator2D authoritative state
 + explicit fixed-step delta
 -> exact speed scaling + retained remainder
 -> ordered timeline traversal
 -> authored/structural emissions
 -> authoritative time/frame/direction/completion
 -> renderer later consumes current semantic Sprite region
```

Hard rules:

- time is integer `std::chrono::nanoseconds`,
- fixed-step/runtime calls are the only advancement authority,
- render cadence, wall clock, interpolation alpha, GPU state and pixels never advance animation truth,
- SA0 frame ownership and event crossing intervals are unchanged,
- seek/reset/inspection do not replay skipped historical events,
- failed advancement is transactional with respect to authoritative animator state.

## 2. Prepared authored events

`SpriteAnimationClip2D::Prepare` accepts an optional ordered input set of typed authored events.

Each `SpriteAnimationEvent2D` contains:

- a setup-resolved numeric semantic event ID,
- integer-nanosecond offset,
- authored ordinal.

Event offsets must satisfy:

```text
0 <= event.offset < clip.duration
```

Preparation sorts the retained event table by:

```text
offset ascending
then authoredOrdinal ascending
```

The pair `(offset, authoredOrdinal)` must be unique. This makes equal-time event order explicit instead of relying on input container behavior.

Preparation remains setup work and may allocate. The resulting event table is retained and reused; playback performs no event-name/path lookup.

## 3. Exact speed and retained remainder

SA1 stores canonical speed as:

```text
numerator / denominator
numerator >= 0
denominator > 0
gcd-reduced
```

SA2 adds `speedRemainder` to authoritative state.

For non-negative input `delta`, advancement preserves the exact rational fraction between calls. The implementation deliberately avoids the potentially overflowing expression:

```text
delta_ns * numerator
```

Instead it decomposes:

```text
whole = delta_ns / denominator
part  = delta_ns % denominator

whole contribution = whole * numerator
fraction numerator  = part * numerator + prior remainder

scaled whole ns = whole contribution + fraction numerator / denominator
next remainder  = fraction numerator % denominator
```

All intermediate operations are checked against the signed nanosecond representation. This keeps the implementation portable to the supported MSVC C++20 path without relying on `__int128`.

Rules:

- pause/resume preserves the remainder,
- an idempotent speed assignment preserves it,
- an actual speed change resets it to zero,
- seek, stop/reset, restart and an actual direction change reset it to zero,
- non-loop completion resets it to zero,
- zero speed is canonical `0/1` with zero remainder,
- restored state is invalid when `remainder >= denominator` or a zero-speed state has non-zero remainder.

## 4. Runtime emission model

`Advance` writes to caller-owned bounded `std::span<SpriteAnimationEmission2D>` storage.

Emission kinds are finite:

```text
AuthoredEvent
Loop
Bounce
Completed
```

Authored emissions carry semantic event ID, authored ordinal, event timeline offset and traversal direction.

Structural markers are distinct typed kinds. They never consume or masquerade as an authored event ID.

### Transactional capacity rule

SA2 never silently drops a crossing.

Advancement first executes a count-only pass against the caller's capacity. If another emission would exceed that capacity:

- return `OutputCapacityExceeded`,
- do not modify authoritative animator state,
- do not write a partial authoritative output sequence.

Only after the count pass succeeds is the same deterministic traversal replayed into the output buffer and committed.

This intentionally trades bounded duplicate traversal work for a simple allocation-free transactional contract. The work remains proportional to crossings/structural transitions that the caller has capacity to receive.

## 5. Frozen event crossing execution

SA2 implements SA0 literally.

Forward segment `a -> b`:

```text
a < event_time <= b
order: increasing time, then authored ordinal
```

Reverse segment `a -> b`:

```text
b <= event_time < a
order: decreasing time, with authored ordinal ascending inside one equal-time group
```

Therefore a boundary event is emitted exactly once for one crossing, and equal-time authored events keep the same authored order in both directions.

## 6. Once playback

Forward `Once` completion:

- traverses to `duration`,
- emits all authored events crossed before the terminal edge,
- emits one `Completed` marker,
- retains `time == duration`,
- retains the last authored frame,
- sets `completed = true`,
- sets playback to `Paused`,
- clears retained speed remainder.

Reverse completion is symmetric at `time == 0` and retains the first frame.

Extra scaled delta after the completion crossing is ignored. A completed `Once` state cannot be resumed with `Play`; `Restart`, `Seek`, or an actual direction change establishes a new non-completed state explicitly.

## 7. Linear loop execution

Looping is composed from traversal segments; modulo-only resolution is forbidden because it would erase crossings.

### Forward wrap

```text
(a, duration)
 -> Loop marker at duration
 -> set time to 0
 -> emit all authored offset-zero events once
 -> continue with (0, b]
```

The ordinary forward segment after re-entry excludes zero, so zero-time events do not duplicate.

### Reverse wrap

```text
[b, a)
 -> arrival at 0 already includes offset-zero authored events
 -> Loop marker at 0
 -> set time to duration
 -> continue reverse traversal
```

There is no synthetic event at `duration` because authored events cannot live there.

Large advances repeat these segments and preserve every emitted crossing/loop marker until caller capacity is exhausted.

## 8. Ping-pong execution

At an endpoint:

```text
finish directional segment
 -> Bounce marker at endpoint
 -> flip direction
 -> continue from the same endpoint
```

The endpoint frame/time is not duplicated.

Reverse arrival at zero may emit offset-zero authored events. The following forward departure excludes zero, so the same boundary is not emitted twice.

A delta that lands exactly on an endpoint still performs the bounce and leaves authoritative direction set for the next traversal.

## 9. Playback/control commands

SA2 freezes the minimum renderer-independent command behavior.

### `Play`

- `Playing` is idempotent,
- `Paused` or non-completed `Stopped` becomes `Playing`,
- completed `Once` rejects with `InvalidPlaybackTransition`; use an explicit restart/seek/direction change.

### `Pause`

- `Playing` becomes `Paused`,
- `Paused` is idempotent,
- `Stopped` rejects with `InvalidPlaybackTransition`,
- time and speed remainder are preserved.

### `Stop` / `Reset`

Both establish the current traversal start:

```text
Forward -> 0
Reverse -> duration
```

They set `Stopped`, clear completion, clear speed remainder and update the authoritative frame. They emit no historical events.

### `Restart`

Establishes the same traversal start as stop/reset, clears completion/remainder, and sets `Playing`. It emits no authored event merely because the cursor was repositioned.

### `Seek`

Selects a valid timeline time directly, updates frame selection, clears completion/remainder, preserves current playback mode, and emits no skipped historical events.

### `SetSpeed`

Normalizes the requested exact rational. An actual speed change clears the remainder; assigning the already-current canonical speed is idempotent and preserves it.

### `SetDirection`

An actual valid direction change clears completion/remainder and retains the current timeline position. Assigning the current direction is idempotent.

SA2 does not introduce a generic animation-state graph, blend tree or cross-fade system. “Transitions” in this stage are these finite typed playback state/direction/loop structural transitions.

## 10. Errors and state safety

New execution failures are explicit:

- no animator state,
- invalid retained speed remainder,
- invalid playback transition,
- negative delta,
- scaled-advance overflow,
- output-capacity exhaustion.

Existing clip/state validation remains authoritative.

Control methods validate the existing state first. `RestoreState` remains validate-before-commit. `Advance` commits only after both the capacity-count pass and output-writing pass succeed.

## 11. Complexity and hot-path policy

After clip preparation:

- current state/frame/region access remains O(1),
- arbitrary frame lookup remains O(log frame_count),
- event segment entry uses binary search over the prepared event table,
- emitted traversal work is O(crossed events + structural transitions), plus logarithmic range location,
- no mandatory heap allocation is performed by fixed-step advancement,
- no filesystem, JSON, diagnostic formatting, semantic-name lookup, renderer/GPU initialization or synchronization occurs.

The caller controls output capacity and can reuse it across ticks.

## 12. External-reference decisions

Reviewed on **2026-08-12** before SA2 implementation.

### Godot `AnimatedSprite2D` — ADAPT / REJECT

ADAPT:

- explicit play/pause/stop behavior,
- distinct loop and finish notifications,
- pause preserving current playback position.

REJECT / narrow:

- floating `frame_progress` / `speed_scale` as authoritative progress,
- signal delivery as the only source of animation truth,
- implicit behavior that would replace Trace2D's already-frozen SA0 crossing rules.

Trace2D keeps exact integer time and structured returned emissions as the deterministic authority.

### Aseprite official format — ADOPT / ADAPT

ADOPT/ADAPT:

- integer per-frame durations,
- explicit forward/reverse/ping-pong direction metadata,
- repeat metadata as useful authoring/import precedent.

Aseprite-specific identity terminates at import/setup. Runtime playback uses Trace2D-owned typed events, integer nanoseconds and loop state. No runtime dependency is added.

## 13. Deliberate SA3/SA4 non-goals

SA2 does not add:

- Agent/MCP action or inspection adapters,
- JSON animation snapshots,
- exact-frame Agent assertion commands,
- animation benchmark/conformance workload infrastructure,
- renderer motion/capture QA,
- cross-fade/blend trees/state-machine graphs.

SA3 owns Agent/MCP verification. SA4 owns conformance/determinism/performance workloads.

## 14. Verification and handoff

Focused SA2 tests must cover:

- prepared event order/validation,
- retained rational remainder,
- overflow rejection,
- pause/resume and remainder reset boundaries,
- forward/reverse equal-time event ordering,
- forward/reverse loop zero-event semantics,
- multiple wraps,
- ping-pong bounces,
- directional once completion,
- transactional output-capacity exhaustion,
- zero-speed no-op behavior.

SA2 is backend-independent and adds no presentation-GPU behavior, so no new local real-GPU acceptance gate is required.

After merge:

```text
SA2 [complete]
 -> SA3 Agent/MCP inspection/actions/exact-frame assertions
```

Do not implement SA3 inside the SA2 PR.
