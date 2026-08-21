# PERF1 bounded CPU profiling substrate

Issue: #372  
Parent: #91

PERF1 establishes the low-level CPU timing capture contract used by the later unified profiler. It does **not** complete #91 by itself.

## Authority split

Trace2D profiling keeps unlike evidence separate:

```text
deterministic structural counters
!= CPU wall-clock timing
!= GPU timing
!= resource-memory evidence
```

PERF1 owns only bounded CPU scope timing and the shared availability vocabulary. CPU timing is machine/environment evidence; it is never deterministic gameplay authority and must not become a portable CI timing threshold on arbitrary hosted hardware.

The availability values are explicit:

- `available` — the metric was actually measured;
- `not_supported` — the active backend/platform cannot provide the metric;
- `not_enabled` — the capability exists but capture is disabled;
- `not_measured` — capture was active, but this metric/scope did not participate in the retained sample.

A numeric zero is therefore not overloaded to mean unavailable.

## Setup and hot-path contract

`CpuProfiler2D` is default-disabled. A caller explicitly:

1. calls `Prepare(scopeCapacity, frameCapacity, stackCapacity)`;
2. registers every scope name and retains the returned `ProfileScopeId2D`;
3. enables capture;
4. supplies monotonic timestamps to frame/scope operations.

The first enabled `BeginFrame()` freezes the scope registry. Ordinary measured operations never resolve a scope by name.

After `Prepare()`:

- scope definitions are fixed-capacity records;
- the active-frame per-scope table is retained;
- frame history is a fixed-size ring;
- each retained frame owns one fixed scope-timing region;
- the nested-scope stack is fixed-capacity;
- `EnterScope()` / `ExitScope()` do not resize containers, hash/compare names, build strings/JSON, query files, or query a clock.

`SteadyProfileTimestamp2D()` is only a convenience for real workloads. The recorder itself accepts caller-supplied `std::chrono::nanoseconds`, which keeps timeline semantics directly testable with exact synthetic timestamps.

## Nested timing semantics

PERF1 is single-threaded and uses strict stack nesting.

For each scope in a frame:

- `callCount` counts completed invocations;
- `inclusiveTime` is the sum of each invocation's full duration;
- `exclusiveTime` subtracts strictly nested child duration from each invocation;
- recursive use of the same scope ID is valid;
- a completed zero-duration scope is `available` with `callCount > 0` and zero time;
- a registered scope never entered in the frame is `not_measured` with `callCount == 0`.

No address/container iteration order affects scope identity. IDs are registration indices frozen before capture.

## Failure semantics

PERF1 does not publish partial or structurally suspect timing frames.

The active frame is rejected when capture observes:

- invalid scope identity;
- scope-stack capacity exhaustion;
- an exit with an empty stack;
- mismatched LIFO scope exit;
- decreasing timestamps;
- duration accumulation overflow;
- unclosed scopes at frame end.

A rejected frame increments `invalidFrameCount` and is not committed to the retained ring. A later frame may start normally.

When retained frame history is full, the next successful frame intentionally overwrites the oldest ring slot. This does not grow capacity; `overwrittenFrameCount` records the loss of old history.

## Reference decisions

Reviewed 2026-08-21.

### Perfetto Track Events

Official docs: <https://perfetto.dev/docs/instrumentation/track-events>

**ADAPT.** Perfetto recommends statically defined categories/event names for low overhead and models nested work as scoped time-bounded events. Trace2D similarly resolves bounded names before the measured path, but retains compact engine-owned IDs and metrics rather than importing Perfetto's trace protocol/runtime into PERF1.

### Tracy

Repository: <https://github.com/wolfpld/tracy>

**ADAPT / REJECT direct PERF1 dependency.** Tracy demonstrates compact source-location identity and production-grade nested zones, but its remote telemetry, sampling, GPU, memory, lock and callstack system is substantially broader than the first #91 requirement. Trace2D keeps the useful ID/zone pattern without adding that dependency or duplicate telemetry authority.

### Godot profiling/performance tools

Official profiling docs: <https://docs.godotengine.org/en/latest/engine_details/development/profiling/index.html>

**ADAPT.** Godot combines engine performance monitoring with external sampling/tracing profilers. Trace2D likewise does not try to replace platform profilers; its engine surface exists to expose stable Agent/CI-readable counters and bounded captures. Trace2D preserves explicit unavailable states rather than encoding them as numeric zero.

## Deferred to later #91 slices

PERF1 intentionally does not add:

- a JSON or CLI report;
- subsystem structural aggregation;
- Renderer/Particle/Physics/Audio adapters;
- GPU timestamps;
- multi-thread event collection;
- lock-free queues;
- allocation interception;
- call stacks or sampling;
- remote telemetry/UI.

PERF2 should compose the already-existing cheap subsystem `Metrics()` structures into a versioned structural snapshot and explicit report vocabulary without making those subsystems generate strings every frame.
