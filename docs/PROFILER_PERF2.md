# PERF2 versioned structural profile snapshot

Issue: #374  
Parent: #91  
Previous slice: #372 / PERF1

PERF2 composes the cheap structural counters already owned by Trace2D subsystems into one bounded, versioned Agent/CI-readable snapshot. It deliberately keeps structural evidence separate from CPU timing, GPU timing and environment metadata.

## Dependency and authority boundary

The dependency direction is intentionally split:

```text
Trace2D::Profile
  - availability vocabulary
  - PERF1 CPU timing substrate
  - PERF2 neutral structural snapshot + explicit JSON formatting

Trace2D::ProfileAdapters
  -> Trace2D::Profile
  -> Assets / Audio / Particles / Physics / Render
```

`Trace2D::Profile` does **not** depend upward on Renderer, Physics2D, Audio, resources or particles. Those systems can therefore use the profiler later without creating a dependency cycle.

Subsystems remain the owners of their native counters. They do not construct profile names, JSON or another duplicate metrics representation during their normal frame/update path. The adapter reads their existing public metric structs only when an explicit diagnostic capture is requested.

## Stable structural schema

The current schema identifier is:

```text
trace2d.profile.structural.v1
```

Each record contains:

- stable hierarchical metric name;
- `gauge` or `counter` kind;
- explicit unit;
- explicit availability (`available`, `not_supported`, `not_enabled`, `not_measured`);
- unsigned 64-bit value.

A measured numeric zero is `available` with value `0`. A missing optional subsystem input is `not_measured` with value `0`. Consumers must inspect availability rather than assigning special meaning to the numeric value.

Metric names and units are copied into fixed-size records in already-prepared storage. Metric insertion order is deterministic. Duplicate names and capacity exhaustion fail explicitly.

## Storage and hot-path contract

`StructuralProfileSnapshot2D::Prepare()` allocates the complete record storage once. `Clear()` resets the logical count and reuses the same retained capacity.

`AddMetric()` performs no heap allocation, hash-table growth, JSON construction, filesystem work or clock query. Duplicate detection is a bounded linear scan; composition is explicit diagnostic work, not an every-frame engine loop.

`BuildStructuralProfileJson()` is an explicit out-of-band formatting boundary and may allocate. It must not be called from deterministic fixed-step simulation or ordinary render/audio/physics hot paths.

Resource memory aggregation calls `ResourceRegistry::InspectAll()` outside this module and consumes the resulting snapshots only during explicit diagnostic work. PERF2 does not move resource inspection allocation into normal frames.

## Adapter coverage

PERF2 publishes 97 stable structural records across these evidence groups:

- Renderer frame/draw/Sprite/material/upload/retained-target counters;
- Physics2D bodies, retained capacities, fixed steps, events, queries and capacity failures;
- semantic Audio voices/events/commands/limits;
- physical AudioOutput retained memory, streams, queue/refill, device/recovery and backend failures;
- ResourceRegistry stats plus aggregated known CPU/container/renderer-GPU bytes;
- CPU ParticleReference alive/spawn/update/drop counters and prepared memory evidence.

Resource byte sums are checked before the current snapshot is cleared. Overflow therefore returns `value_overflow` without replacing the previously committed snapshot. Likewise, insufficient adapter capacity is detected before publication begins.

PERF2 does not invent one universal performance score. Structural counters remain structurally comparable; CPU/GPU time remains separate machine evidence.

## External benchmark/reference decisions

Reviewed 2026-08-21.

### OpenTelemetry Metrics data model and semantic conventions

Sources:

- <https://opentelemetry.io/docs/specs/otel/metrics/data-model/>
- <https://opentelemetry.io/docs/specs/semconv/general/metrics/>
- <https://opentelemetry.io/docs/specs/otel/versioning-and-stability/>

**ADAPT / REJECT direct dependency.** OpenTelemetry's stable metric identity, kind/unit semantics, hierarchical naming and distinction between absent data and numeric values are useful precedents. Trace2D adapts those contract lessons into a bounded local snapshot. It does not import OTLP exporters, collectors, networking or the full telemetry SDK because PERF2 is local engine/CI evidence rather than distributed observability infrastructure.

### Godot performance monitors and profiling guidance

Sources:

- <https://docs.godotengine.org/en/4.6/classes/class_performance.html>
- <https://docs.godotengine.org/en/4.6/engine_details/development/profiling/index.html>

**ADAPT.** Godot demonstrates that engine-readable structural monitors complement deeper external CPU/GPU profilers and that monitoring work itself has a cost. Trace2D keeps cheap native counters in their owning systems and performs cross-system formatting only on explicit request.

## Deterministic regression coverage

PERF2 tests assert:

- prepared capacity is reused across `Clear()`;
- duplicate metric names fail without overwriting the existing record;
- measured zero differs from missing/unmeasured source data;
- representative Renderer/Physics/Audio/AudioOutput/Resource/Particle values map to stable metric identities;
- resource byte-sum overflow preserves the previous snapshot;
- insufficient adapter capacity publishes no partial snapshot;
- explicit JSON contains the schema, context, kind, unit, availability and escaped content.

The adapter target is also exported as `Trace2D::ProfileAdapters` and consumed by the installed-SDK external-game gate.

## Deferred after PERF2

PERF2 intentionally does not add:

- CPU timing/environment report composition from PERF1 captures;
- GPU timestamp/query implementation;
- thread-aware event recording;
- a sampling profiler or call stacks;
- allocation interception;
- network telemetry/exporters;
- universal hosted-CI wall-clock thresholds.

The next #91 slice should compose environment metadata and PERF1 CPU capture into the report while keeping machine timing explicitly non-portable. GPU timing remains a later optional capability with `not_supported` where the active backend cannot provide reliable timestamps.
