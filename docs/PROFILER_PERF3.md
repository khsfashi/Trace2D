# Profiler PERF3 — Unified CPU capture report

PERF3 composes the bounded CPU capture from PERF1 with the versioned structural snapshot from PERF2. It does not add a new timing recorder, clock source, telemetry transport or GPU profiler.

## Authority

`CpuProfiler2D` remains the only CPU scope-capture authority. Scope names are resolved during setup, timestamps are supplied by the caller, and retained frame/scope/stack storage is prepared before capture.

`StructuralProfileSnapshot2D` remains the structural metric authority introduced by PERF2. Subsystems keep owning their native counters; they do not build profile JSON in ordinary frame paths.

`BuildProfileReportJson()` is an explicit out-of-band composition boundary. It reads committed retained CPU history plus one prepared structural snapshot and emits `trace2d.profile.report.v1`.

## Environment metadata

PERF3 deliberately accepts environment/build metadata from the workload instead of performing hidden platform discovery. This keeps profile collection free of filesystem, CPUID, process-spawn or clock-query side effects.

The report carries:

- engine version and source revision;
- workload identity;
- build configuration, OS, architecture and compiler;
- optional CPU identity with explicit availability;
- renderer backend and timing-source identity;
- fixed timestep, warmup-frame count and requested sample-frame count.

An unavailable CPU identity is `not_measured`; it is never replaced by an invented or guessed machine string.

## CPU report semantics

Committed retained frames are emitted oldest-to-newest, including after the PERF1 ring wraps. Every frame contains the recorded frame timestamps/duration and every registered scope's call/inclusive/exclusive data.

The report also creates one aggregate per registered scope:

- availability;
- measured frame count;
- total calls;
- inclusive/exclusive total nanoseconds;
- minimum/maximum inclusive nanoseconds across measured frames.

A measured 0 ns sample remains `available` and contributes a real zero to min/max. A registered scope never entered in retained frames remains `not_measured`.

If the profiler has no committed frames, report-level CPU availability is:

- `not_enabled` when capture is disabled;
- `not_measured` when capture is enabled but no frame has committed.

## Performance boundary

No PERF3 report/aggregation work runs from `BeginFrame()`, `EnterScope()`, `ExitScope()` or `EndFrame()`.

Report generation may allocate because it is explicit diagnostic work. CPU aggregation is bounded by the already-retained frame and scope counts. Checked unsigned additions reject aggregation overflow. Report construction uses a temporary result and swaps it into the caller output only on success, so a reported failure cannot replace a previous valid report with partial evidence.

PERF3 adds no:

- per-scope string lookup/hash in the measured path;
- profiler-owned clock query;
- unbounded frame history;
- custom allocator or allocation interception;
- thread recorder or lock-free queue;
- network/OTLP transport;
- sampling profiler or call stacks;
- GPU timestamp/query implementation;
- hosted-CI wall-clock threshold.

## External references

### Perfetto Track Events

Source: <https://perfetto.dev/docs/instrumentation/track-events>

Decision: **ADAPT / REJECT direct dependency**. Trace2D keeps the proven nested monotonic-slice model but retains its existing compact scope IDs and bounded local capture instead of importing the Perfetto runtime/protocol for PERF3.

### Godot profiling guidance

Source: <https://docs.godotengine.org/en/4.6/engine_details/development/profiling/index.html>

Decision: **ADAPT**. CPU timing remains contextual machine evidence and stays separate from deterministic structural counters.

### Tracy

Source: <https://github.com/wolfpld/tracy>

Decision: **ADAPT / REJECT direct dependency**. Tracy's zone-oriented profiling validates the usefulness of named bounded timing regions, but remote transport, sampling, call stacks, allocator interception and broad thread instrumentation are outside this measured need.

## Continuation

#91 remains open after PERF3. The next bounded profiler slice may add GPU timing only where the active backend can supply reliable timestamps and must preserve explicit `not_supported` elsewhere. A final representative workload/CLI proof should then demonstrate the complete #91 machine-readable surface before advancing the core lane.
