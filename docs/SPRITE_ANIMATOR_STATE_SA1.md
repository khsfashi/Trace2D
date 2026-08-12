# Sprite Animator Authoritative State Contract — SA1

Status: **active implementation contract for #146 under #59**.  
Predecessor: SA0 / #144 / PR #145 / squash `d9955d4c987a627f0009a018b9b5293c6f3d8e73`.  
Exact successor after SA1 merges green: **SA2 — deterministic playback, event emission, and transitions**.

SA1 implements the typed renderer-independent state that SA0 deliberately deferred. It does not advance animation time or implement playback commands/events/transitions.

## 1. Authority

`SpriteAnimator2D` is authoritative simulation state.

```text
prepared Sprite animation clip
        + authoritative SpriteAnimator2D state
        -> current semantic frame / Sprite region index
        -> later renderer extraction consumes the selection
```

The animator does not require a renderer, SDL object, OS window, GPU resource, wall-clock render time, presentation interpolation alpha, JSON/MCP type, or filesystem path.

## 2. Prepared clip ownership

`SpriteAnimationClip2D::Prepare` is an explicit setup-time operation. It accepts already-resolved numeric Sprite region indices and ordered frame durations, validates them, and caches:

- the frame records,
- cumulative frame boundaries,
- the checked exact clip duration.

Setup is O(frame_count) and may allocate. A failed preparation leaves the previous prepared output unchanged so callers can safely cache/reuse a valid clip.

Every frame duration is a strictly positive `std::chrono::nanoseconds` value. Total duration is checked for `nanoseconds::rep` overflow. Region indices must be less than the setup-supplied canonical Sprite region count, so ordinary animation state never performs semantic region-name lookup.

`SpriteAnimationClip2D` is movable but intentionally non-copyable. Runtime owners should prepare/cache a clip once and keep its address stable while animator states reference it.

## 3. Frozen frame lookup

SA1 implements SA0's exact half-open rule over cached cumulative boundaries:

```text
frame i owns [bi, b(i+1))
```

`ResolveFrameIndex(time)` is O(log frame_count) using the prepared boundary table.

- `time < 0` is invalid,
- `time > duration` is invalid,
- exact internal boundaries select the following frame,
- `time == duration` resolves to the final authored frame so completed non-loop presentation never invents frame `N`.

Current-frame access from an already-valid `SpriteAnimator2DState` is O(1); it uses the authoritative cached `frameIndex` rather than re-searching the timeline.

## 4. Authoritative state

`SpriteAnimator2DState` contains only fixed-size typed state:

- non-owning pointer to one prepared `SpriteAnimationClip2D`,
- integer-nanosecond `time`,
- authoritative `frameIndex`,
- `Stopped` / `Playing` / `Paused` playback state,
- `Once` / `Loop` / `PingPong` loop policy,
- `Forward` / `Reverse` traversal direction,
- explicit `completed` flag,
- exact non-negative rational speed magnitude.

Traversal direction is separate from speed magnitude. This leaves SA2 one unambiguous direction bit to update at ping-pong endpoints without encoding bounce state into floating-point sign conventions.

The state is trivially copyable. It owns no container, string, shared ownership handle, renderer object or diagnostic buffer.

The clip pointer is deliberately non-owning: the prepared clip must outlive every animator state that references it. This avoids per-state reference-count traffic and hidden ownership work in the fixed-step hot path. Future #86 may generalize resource lifetime without changing animation-time authority.

## 5. Exact speed representation

SA1 freezes only the state representation needed for deterministic SA2 advancement:

```text
speed = numerator / denominator
numerator   >= 0
denominator > 0
```

The representation is canonical/reduced by `gcd`; zero normalizes to `0/1`.

`MakeSpriteAnimator2DState` accepts a reducible requested ratio and stores its canonical form. `ValidateSpriteAnimator2DState` rejects a non-canonical or zero-denominator raw restored state rather than silently changing persisted/runtime truth.

No floating-point progress or speed accumulator is authoritative. SA2 still owns the exact retained-remainder advancement algorithm and public play/pause/seek/restart behavior.

## 6. State validation

`ValidateSpriteAnimator2DState` is allocation-free and rejects:

- null clip,
- unprepared clip,
- invalid enum values,
- invalid/non-canonical exact speed,
- time outside `[0, duration]`,
- `frameIndex` inconsistent with the frozen SA0 time boundary rule,
- completed loop/ping-pong state,
- completed `Once` state away from its directional endpoint.

A forward completed `Once` state is at `duration`; a reverse completed `Once` state is at `0`.

`completed == false` may still exist at an endpoint. SA1 does not invent SA2 command semantics for whether a newly started, paused, restored or just-arrived state immediately transitions there.

`SpriteAnimator2D::RestoreState` validates first and commits only a valid state. Failure preserves the prior animator state.

## 7. Hot-path and complexity contract

After clip preparation:

- current state access: O(1), no allocation,
- current frame / region index access: O(1), no allocation,
- arbitrary time -> frame lookup: O(log frame_count), no allocation,
- state validation: O(log frame_count), no allocation.

Ordinary state access performs no:

- filesystem work,
- JSON serialization/parsing,
- diagnostic string formatting,
- semantic name/path lookup,
- renderer/GPU initialization or synchronization,
- mandatory heap allocation.

Clip preparation owns its vectors once; runtime callers reuse the prepared object instead of rebuilding cumulative boundaries every tick.

## 8. External-reference decisions

Reviewed for SA1 on **2026-08-12**.

### Godot `AnimatedSprite2D` — ADAPT / REJECT

The current official API exposes explicit animation/frame/playing/speed state and preserves frame/progress on pause. Trace2D adapts the useful explicit state separation.

Trace2D rejects floating `frame_progress` and floating `speed_scale` as authoritative deterministic state. It also keeps direction explicit instead of relying on negative floating speed, because SA2 must represent ping-pong traversal deterministically.

### Aseprite `.ase/.aseprite` format — ADOPT / ADAPT

The official format provides explicit integer per-frame duration and animation direction metadata. Trace2D keeps the explicit-duration idea and canonical direction vocabulary, while adapting time to integer nanoseconds and terminating Aseprite-specific identity at import/setup.

These references add no runtime dependency.

## 9. Deliberate SA2+ non-goals

SA1 does **not** implement:

- fixed-step time advancement,
- retained fractional speed remainder,
- play / pause / stop / restart / seek commands,
- loop wrapping,
- ping-pong bounce execution,
- authored event storage/emission/caller-owned output buffers,
- transition/blend state machines,
- Agent/MCP actions or semantic inspection JSON,
- animation conformance workloads or presentation QA.

Those remain SA2, SA3 and SA4 in the fixed Sprite lane.

## 10. Verification and handoff

Focused `SpriteAnimator2DTests` lock:

- cached variable-duration boundaries,
- exact internal/terminal frame selection,
- empty/zero-duration/out-of-range-region/duration-overflow rejection,
- exact rational speed normalization/validation,
- typed authoritative state creation and current region selection,
- mismatched frame rejection with prior-state preservation,
- explicit directional completion endpoint validation,
- invalid enum/speed/unprepared-clip rejection.

SA1 is backend-independent and introduces no new presentation-GPU behavior, so it has no new local real-GPU acceptance gate.

SA1 is complete only after the final implementation head is green in normal hosted CI/audits, documentation agrees with the implementation, PR #147 (or the actual owning PR number if different) merges, and #146 closes.

After merge:

```text
SA1 [complete]
 -> SA2 deterministic playback / events / transitions
```

Do not implement SA2 inside the SA1 PR.
