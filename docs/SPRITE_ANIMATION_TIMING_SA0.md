# Sprite Animation Timing Contract — SA0

Status: **active contract freeze for #144 under #59**.  
Predecessor: SR8 / #142 / PR #143, merged as `2108122dad5ac2dcbb964f7ada0e80f7afa21003`.  
Exact successor after SA0 merges green: **SA1 — `SpriteAnimator2D` authoritative runtime state**.

SA0 freezes animation-time, frame-boundary and event-crossing semantics only. It does **not** implement the full animator state machine, playback commands, transitions, Agent/MCP actions or animation workloads.

## 1. Authority

Sprite animation is authoritative simulation state, not renderer state.

```text
canonical Sprite animation clip/timeline
        + fixed-step simulation advance
        -> authoritative animation time/frame/event crossings
        -> renderer consumes current frame selection
        -> optional derived presentation/capture evidence
```

Hard rules:

- authoritative animation advancement occurs only from explicit simulation/runtime advancement,
- wall-clock render time does not advance animation truth,
- presentation interpolation alpha does not advance animation truth,
- GPU state, captured pixels and renderer-selected resources are derived consumers/evidence,
- exact-frame Agent/gameplay inspection observes authoritative current animation state,
- seek/reset/inspection do not implicitly reconstruct or replay historical events.

This preserves the authority model frozen by #85/S0: gameplay and animation state remain usable without a renderer, OS window or GPU.

## 2. Exact time domain

Trace2D's existing `FixedStepRuntime` uses `std::chrono::nanoseconds` for fixed timestep, simulation time and accumulated wall time. SA0 keeps Sprite animation in the same integer-time domain.

Canonical rules:

- clip/frame/event timeline unit: **integer nanoseconds**,
- each frame duration is strictly greater than zero,
- each event offset is an integer nanosecond offset from clip start,
- clip duration is the checked exact sum of frame durations,
- zero-duration clips, zero/negative frame durations and duration overflow are invalid,
- an accumulated floating-point seconds cursor is never authoritative animation state.

Future SA2 speed scaling may expose convenient numeric input, but deterministic advancement must resolve to exact integer-time progress with retained remainder. A reduced rational or an equivalently exact integer-preserving representation is acceptable. Dropping fractional progress every tick or repeatedly accumulating binary floating-point time is not.

SA0 freezes the semantic requirement, not the final public SA2 speed API spelling.

## 3. Clip frame boundaries

For `N` frames with positive durations `d0..d(N-1)`, define cumulative boundaries:

```text
b0 = 0
b(i+1) = bi + di
duration = bN
```

Frame `i` owns the half-open interval:

```text
[bi, b(i+1))
```

Therefore:

- `t == bi` selects frame `i`,
- `t == b(i+1)` selects the following frame when one exists,
- ordinary in-range frame lookup is defined for `0 <= t < duration`,
- no boundary is owned by two frames,
- no zero-time synthetic frame is inserted at a boundary.

For a non-looping clip, the runtime may retain the terminal time `t == duration` after completion. Completion is explicit state; presentation at that terminal edge remains the final authored frame rather than inventing frame `N`.

### Example

Frame durations `100 ms, 50 ms, 150 ms` become boundaries `0, 100, 150, 300 ms`:

```text
[0,100)   -> frame 0
[100,150) -> frame 1
[150,300) -> frame 2
300        -> completed terminal edge, still presents frame 2
```

The example uses milliseconds only for readability; canonical Trace2D storage/advancement is integer nanoseconds.

## 4. Authored event identity and ordering

An authored named event has:

- a timeline offset in `[0, duration)`,
- a stable authored ordinal used to break equal-offset ties,
- later SA1/SA2-resolved semantic identity/payload as required by those stages.

An authored event at `duration` is invalid because `duration` is the terminal edge, not a point inside the canonical clip timeline.

Equal-offset events preserve authored ordinal in both forward and reverse traversal. Reverse playback reverses timeline order, not the author's ordering of events attached to one exact instant.

Completion, loop and bounce notifications are structural runtime markers. They are not disguised authored named events and do not consume authored event ordinals.

## 5. Event crossing rule

Events are produced by **crossing timeline boundaries**, not by sampling whichever frame happens to be visible after an update.

For a forward segment from `a` to `b`, with `a <= b`:

```text
emit events where a < event_time <= b
order: increasing event_time, then authored ordinal
```

For a reverse segment from `a` down to `b`, with `b <= a`:

```text
emit events where b <= event_time < a
order: decreasing event_time, then authored ordinal
```

Consequences:

- an event at the starting cursor is not emitted again merely because an update begins there,
- an event at the newly reached boundary is emitted exactly once,
- a large fixed-step advance may cross many frames/events and must preserve all crossings in deterministic order,
- repeatedly sampling only the final frame/event after a large step is incorrect,
- every actual traversal of an event boundary may emit that event again (for example on a later loop), but one crossing may not emit it twice.

### Equal-time example

If events `hit_a` and `hit_b` share offset `100 ms` with authored ordinals 2 and 5, advancing `50 -> 120 ms` emits:

```text
hit_a
hit_b
```

Reversing `120 -> 50 ms` also emits the equal-time pair in authored ordinal order after selecting the `100 ms` timeline instant.

## 6. Linear looping

A loop wrap is represented as ordered traversal segments, not modulo arithmetic that erases crossed boundaries.

Forward example for duration `300 ms`, advancing from `250 ms` through one wrap to `40 ms`:

```text
forward segment: (250, 300)
loop marker
enter timeline start at 0
forward segment: (0, 40]
```

Because authored events may legally exist at offset `0`, **entering timeline start because of a forward loop wrap emits all offset-zero events once**, after the loop marker and before positive-offset events in the new segment. The ordinary forward segment `(0, 40]` excludes zero, preventing a duplicate.

Reverse looping uses the same canonical timeline. Reaching offset `0` through reverse traversal already includes offset-zero events through `b <= event_time < a`; wrapping to `duration` does not emit a synthetic duration event because authored events cannot exist at `duration`.

An advance large enough for multiple wraps composes the same segments repeatedly and reports every crossing in traversal order. SA2 may impose an explicit bounded-output policy, but it must report overflow/failure rather than silently dropping events.

## 7. Ping-pong traversal

Ping-pong does not create a second reversed clip and does not duplicate endpoint frames at zero time.

At a terminal endpoint:

1. finish the current directional segment,
2. emit the structural bounce marker,
3. reverse traversal direction,
4. continue from the same endpoint without re-entering a duplicate endpoint frame.

At offset `0`, reverse arrival can emit offset-zero authored events once because the reverse crossing interval includes its destination. The subsequent forward segment begins at zero and excludes its starting point, so those events are not duplicated.

At `duration`, no authored named event can exist. The terminal frame remains the frame owning the interval immediately before `duration`; reversing direction continues through the same canonical frame-duration table.

This matches the user-visible expectation of ping-pong playback while keeping one authoritative timeline.

## 8. Seek, reset, restart and inspection boundary

SA0 distinguishes changing the cursor from traversing elapsed time.

- **seek** selects a target timeline position without synthesizing events for skipped history,
- **reset/load** establishes authoritative state without historical event replay,
- **inspection/assertion** is observational and emits nothing,
- **play/restart behavior** belongs to SA2 and must explicitly define whether a new playback action emits any start marker/event; it may not inherit accidental behavior from cursor assignment.

This prevents an Agent exact-frame query, scene load or repair operation from causing gameplay side effects merely by observing/repositioning animation state.

## 9. Renderer and presentation boundary

The renderer consumes the authoritative selected animation frame/region; it does not own time progression.

- animation frame/event state is available headlessly,
- interactive transform interpolation from SR1/SR6 remains separate from animation-time advancement,
- a screenshot/capture cannot override deterministic frame/event failure,
- exact-frame capture uses current authoritative animation state unless an owning future tool explicitly requests a different documented sub-frame presentation mode,
- future renderer resource caches may resolve the current frame's Sprite region, but handles/UVs/GPU resources never become animation identity.

SA1 must therefore remain renderer-independent and attach to the future #71 component model through the already-frozen typed semantic seam.

## 10. Setup and hot-path policy

Clip setup/import may perform explicit validation and precomputation outside the steady-state update path, including:

- checked cumulative frame boundaries,
- sorted/indexed event offsets while preserving authored ordinal,
- resolved Sprite region identities,
- validation of duration/event bounds.

Steady-state animation advancement must not require:

- filesystem access,
- JSON generation/parsing,
- diagnostic string formatting,
- semantic-name/path lookup,
- renderer/GPU initialization,
- mandatory heap allocation per fixed tick.

Runtime work should be proportional to the timeline boundaries actually crossed, not to the numeric count of nanoseconds and not to unrelated assets. Event output uses caller-owned/reused bounded storage or an equivalently allocation-free runtime seam. Capacity exhaustion is explicit and must never silently discard authoritative event crossings.

The exact data structure and search strategy are deferred to SA1/SA2; SA0 freezes observable complexity/ownership constraints, not premature implementation detail.

## 11. External-reference decisions

Reviewed on **2026-08-12** before freezing SA0:

### Aseprite `.ase/.aseprite` format — ADOPT / ADAPT

Official format documentation stores per-frame duration as an integer millisecond value and animation tags support forward/reverse/ping-pong directions.

Trace2D adopts explicit per-frame duration and one canonical timeline. It adapts the unit to integer nanoseconds to match `FixedStepRuntime`. Aseprite remains an offline import source, never runtime authority.

### Godot `SpriteFrames` / `AnimatedSprite2D` — ADOPT / ADAPT / REJECT

Useful current semantics include variable frame durations, linear/ping-pong looping, reverse playback, and distinct frame/loop/finish notifications. Ping-pong endpoint frames are not duplicated.

Trace2D adopts/adapts these observable concepts where they fit the owner-fixed Sprite program. It rejects float duration/progress/speed accumulation as authoritative deterministic time.

### Godot fixed-timestep interpolation guidance — ADOPT

The separation between fixed simulation updates and decoupled rendered frames reinforces Trace2D's already-frozen #85 rule: animation/gameplay truth advances on the deterministic simulation side while presentation stays derived.

Primary references:

- <https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md>
- <https://docs.godotengine.org/en/stable/classes/class_spriteframes.html>
- <https://docs.godotengine.org/en/stable/classes/class_animatedsprite2d.html>
- <https://docs.godotengine.org/en/stable/tutorials/physics/interpolation/using_physics_interpolation.html>

These references inform Trace2D-owned contracts/tests; they add no runtime dependency.

## 12. SA0 verification and handoff

SA0 is locked by `docs/contracts/sprite-animation-sa0.json` and `scripts/test_sprite_animation_sa0_contract.py`.

SA0 is complete only when the contract check and normal repository CI are green on the final PR head and project/status documentation agrees with the final contract.

After merge:

```text
SA0 [complete]
 -> SA1 SpriteAnimator2D authoritative runtime state
```

Do **not** implement SA1 inside the SA0 PR.