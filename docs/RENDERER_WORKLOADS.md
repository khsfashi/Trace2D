# Renderer Workloads

Trace2D keeps renderer optimization evidence reproducible before adding more rendering complexity.

This document defines the committed Issue #41 workload contract and the difference between deterministic structural evidence and machine-dependent wall-clock timing.

## Goals

The workload surface exists to answer concrete questions such as:

- how many sprites are authored, visible, and culled,
- how fragmented the visible painter-ordered texture sequence is,
- how many draws and sprites were actually committed after successful GPU submission,
- what local CPU wall-clock cost was observed for `Renderer::RenderFrame`,
- which machine/GPU/driver/build produced a timing result.

It is not a generic benchmark framework and it does not justify percentage claims without a recorded report.

## Committed deterministic workloads

All workloads use a 1280x720 target, camera center `(0, 0)`, and `verticalSize = 24`.

| Workload | Authored | Visible | Culled | Contiguous visible texture runs | Purpose |
| --- | ---: | ---: | ---: | ---: | --- |
| `dense_single_texture` | 400 | 400 | 0 | 1 | Best-case texture locality with a dense visible grid. |
| `alternating_two_textures` | 400 | 400 | 0 | 400 | Same visible count with intentionally worst-case two-texture alternation while preserving painter order. |
| `interleaved_culling` | 600 | 400 | 200 | 1 | Culled sprites are interleaved between visible sprites and must not split the visible same-texture run. |

The generated sprite order is authoritative painter order. Texture identity never participates in a global reorder.

`RendererWorkload` construction is setup/test/tool work and may allocate its committed sprite vector once. It is not part of the ordinary renderer frame path.

## Deterministic structural report

Build the repository normally and run:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel --target trace2d_renderer_workload
.\build\windows-msvc\tools\renderer_workload\Debug\trace2d_renderer_workload.exe --list
```

The command emits one JSON object. Its `metric_source` is `deterministic_structure`.

Structural fields are CPU-testable and stable across machines:

- target dimensions,
- orthographic camera values,
- authored sprite count,
- visible sprite count,
- culled sprite count,
- contiguous visible texture-run count.

The headless structural path does **not** report `draw_calls` or `submitted_sprites` as if they were GPU facts. Those names are reserved for actual renderer metric deltas after successful submission.

The full CTest suite regression-tests the workload definitions and their exact structural counts without requiring a hosted-runner GPU.

## Local timing and successful-submission report

Timing is optional and deliberately local. Build Release first:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-release --parallel --target trace2d_renderer_workload
```

Example:

```powershell
.\build\windows-msvc\tools\renderer_workload\Release\trace2d_renderer_workload.exe `
  --workload alternating_two_textures `
  --timing `
  --warmup 60 `
  --iterations 600 `
  --machine-label "desktop-2026-08" `
  --gpu-model "<exact GPU model>" `
  --driver-version "<exact installed driver version>"
```

`--timing` requires all three user-supplied environment labels so a pasted report cannot silently omit the machine/GPU/driver identity.

The tool also records:

- operating-system family,
- compiler ID/version,
- CMake build configuration,
- SDL GPU renderer backend from `Renderer::DriverName()`.

### Successful GPU-submission metrics

The timing path snapshots `Renderer::Metrics()` after warmup, runs the requested measured frames, then reports the delta.

The following values therefore come from the renderer's existing post-submit metric commit path rather than speculative workload math:

- `submitted_frames`,
- `presented_frames`,
- `render_passes`,
- `draw_calls`,
- `submitted_sprites`,
- `culled_sprites`.

For a stable available presentation target, the measured delta must match the deterministic workload structure for every requested iteration. A mismatch returns non-zero and reports `status = submission_mismatch`.

## Timing semantics

The timing field is explicitly:

```text
cpu_wall_clock_renderframe_submission
```

It measures wall-clock time around repeated `Renderer::RenderFrame` calls after warmup. The normal non-capture renderer submits GPU work asynchronously, so this is **not** claimed to be GPU completion time, scan-out time, or a universal frame-time number.

Do not compare timing reports unless workload name, iteration policy, machine, GPU, driver, build configuration, and relevant source revision are recorded.

Hosted CI does not enforce wall-clock thresholds.

## Allocation and hot-path boundary

Issue #41 does not add benchmark-only work to `Renderer::RenderFrame`.

The workload tool reuses the renderer's existing behavior:

- visibility/run measurement already used by submission,
- persistent/capacity-managed instance GPU/upload buffers,
- post-success cumulative `RenderMetrics`,
- no renderer-owned visible-sprite vector,
- no global texture sort,
- no per-frame benchmark JSON construction inside the renderer.

JSON formatting, workload-vector creation, command-line parsing, environment labels, and wall-clock measurement exist only in the explicit tool path.

## Evidence required for future renderer optimization

A future renderer optimization PR must identify at least one committed workload or add a narrowly justified new workload before making a performance claim.

The PR should record:

1. the workload name and exact source revision,
2. the bottleneck being targeted,
3. deterministic before/after structural metrics where applicable,
4. local timing reports when a wall-clock claim is made,
5. machine/GPU/driver/build metadata for every timing comparison,
6. whether painter order, culling semantics, output pixels, resource lifetime, or memory behavior changed.

A claim such as "30% faster" without this context is not acceptable evidence.

Optimization complexity remains measurement-driven. A workload showing many contiguous runs may justify work on texture/material organization or submission; a workload dominated by culling or instance upload may justify a different proposal. The benchmark does not prescribe the optimization in advance.
