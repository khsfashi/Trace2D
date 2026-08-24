# Trace2D representative profiler

`trace2d-profile` is the supported PERF5 workload for producing one bounded, machine-readable Trace2D profile through the public profiling contracts.

## Usage

```text
trace2d-profile --headless --frames 120 --warmup 8 --seed 42 --json
trace2d-profile --windowed --frames 120 --warmup 8 --seed 42 --output profile.json
```

The workload exercises the public fixed-step runtime, `ResourceRegistry`, and deterministic `ParticleReferenceEmitter` path. Windowed mode additionally creates a public renderer, uploads one tracked texture, renders a stable sprite workload, and records the renderer-owned GPU residency bytes known to Trace2D.

The output schema is `trace2d.profile.report.v1`. Structural subsystem metrics come from `Trace2D::ProfileAdapters`; this tool does not define a second renderer, resource, or particle metric vocabulary.

## Timing policy

CPU timing uses `std::chrono::steady_clock` and is retained in the bounded `CpuProfiler2D` history. The report records build, OS, architecture, compiler, warmup, sample count, and timing-source context. CPU identity remains explicitly `not_measured` when the tool cannot provide trustworthy identity without platform-specific discovery.

GPU timing is explicitly `not_supported` in this workload. The pinned public SDL3 GPU surface does not currently expose a trustworthy timestamp/query-pool capture path used by Trace2D, so the profiler does not infer GPU time from CPU submission or presentation time.

Hosted CI validates only stable schema, availability states, bounded storage, and deterministic structural evidence. It intentionally does **not** gate wall-clock CPU or GPU timing. Timing thresholds require dedicated stable hardware and a separately documented noise/tolerance policy.

## Storage and report boundary

Warmup frames run before CPU timing capture. Sample-frame history is capped at 240 retained frames even when a longer bounded workload is requested. Resource inspection, structural composition, aggregation, JSON construction, and file output happen only after the measured frame loop.
