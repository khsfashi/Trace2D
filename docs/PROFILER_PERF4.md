# Profiler PERF4 — explicit GPU timing evidence contract

PERF4 completes the GPU-timing availability portion of #91 without violating Trace2D's renderer/backend authority boundary.

## Reference decision

### Vulkan timestamp queries

Source: <https://docs.vulkan.org/guide/latest/profiling.html>

Decision: **ADAPT**. Native Vulkan timestamp query pools are the correct mechanism for measuring GPU execution rather than CPU submission time. Results are backend/device dependent and should normally be consumed after GPU work completes rather than by stalling ordinary rendering.

### SDL3 GPU Query API status

Source: <https://github.com/libsdl-org/SDL/issues/11696>

Decision: **REJECT private backend escape hatch / DEFER physical capture**. The public SDL3 GPU API used by Trace2D does not currently expose timestamp/query-pool capture. SDL upstream tracks a 3.x Query API proposal specifically including timestamp measurements. Trace2D therefore does not extract private Vulkan/D3D12/Metal handles, fork SDL, or add backend-specific ownership beneath SDL.

### Godot GPU profiling precedent

Source: <https://docs.godotengine.org/en/latest/tutorials/editor/command_line_tutorial.html>

Decision: **ADAPT**. GPU profiling is explicit diagnostic behavior and remains distinct from ordinary rendering and deterministic structural metrics.

## Public contract

`trace2d.profile.report.v1` now always contains a `gpu` section.

The PERF3-compatible `BuildProfileReportJson(...)` overload reports GPU timing as `not_supported`. This is the truthful state for the current SDL3 GPU path because Trace2D has no public timestamp/query source to consume.

The PERF4 overload accepts a caller-owned `GpuProfileEvidence2D` containing:

- explicit timing availability;
- optional device identity with its own availability;
- optional driver identity with its own availability;
- timing-source identity;
- a bounded caller-owned span of `(frame_index, duration_ns)` samples.

This is an evidence-composition API, not another GPU profiler implementation. A future public backend integration can gather timestamps under its own correct synchronization rules and pass already-resolved samples to Profile without creating a Renderer -> Profile dependency.

## Availability and zero semantics

`available` requires at least one frame timing sample. `not_supported`, `not_enabled`, and `not_measured` require an empty timing span. This prevents an unavailable state from silently carrying numeric evidence.

A measured `0 ns` duration remains a real available zero and participates in aggregate min/max exactly as zero. Unknown/unavailable values are represented by availability state, never by numeric zero.

## Aggregation and failure behavior

GPU aggregation occurs only during explicit report construction. It computes bounded sample count, total, min and max timing values with checked unsigned addition.

Aggregation overflow returns `aggregation_overflow`. Invalid availability/sample combinations return `invalid_gpu_evidence`. Both failure paths leave the caller's previous output string unchanged.

## Performance / ownership boundary

PERF4 adds no work to `Renderer::RenderFrame`, GPU command recording, presentation, `CpuProfiler2D` measured scopes, or subsystem structural counters.

It adds no:

- GPU fence wait to normal rendering;
- SDL private API or native backend-handle extraction;
- Vulkan/D3D12/Metal dependency in Profile;
- per-frame allocation or string work in renderer hot paths;
- hosted-CI GPU timing threshold;
- claim about driver-owned GPU memory.

Report parsing/recomposition in the GPU-evidence overload is explicit out-of-band diagnostic work and is bounded by the already-bounded report/sample inputs.

## Continuation

#91 remains open after PERF4. The remaining profiler slice should prove the complete machine-readable surface through a representative external workload/CLI workflow, add structural regression assertions suitable for CI, and then close #91 only if every parent acceptance criterion is demonstrated. Actual SDL GPU timestamps remain deferred until a public reliable query API exists.
