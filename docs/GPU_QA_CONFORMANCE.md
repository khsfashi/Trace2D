# Trace2D GPU QA conformance contract

Status: **GPUQA2 active via #389; GPUQA1 complete via #387/#388**  
Parent: **#92 GPUQA0**

Trace2D separates backend-independent correctness from explicit real-GPU presentation evidence. GPU QA never moves gameplay truth, readback, fence waits, image comparison, JSON, or filesystem work into ordinary frames.

## Tiers

### Tier A — hosted/backend-independent

Pure math, serialization, shader/toolchain validation, Sprite geometry/order/culling, camera/viewport mapping, Material2D layout, particle CPU-reference semantics, and canonical image encoding remain normal hosted-CI responsibilities wherever they do not require a presentation GPU.

### Tier B — maintained real GPU

The current maintained target is `owner-windows-primary` from `config/gpu-qa-support-matrix.json`. It is a trusted interactive Windows x64 self-hosted runner. A green run proves only the exact machine environment recorded in that run's manifest; it does not imply another GPU vendor, driver, backend, or platform is equivalent.

### Tier C — release matrix

GPUQA2 intentionally has no Tier C targets. `tier_c_claim` remains `not_established` until additional maintained targets have explicit owners and repeatable infrastructure.

## Evidence schema

`scripts/gpu_gate.ps1` writes `trace2d.gpu-gate.v2` to `artifacts/gpu-gate/manifest.json`.

The manifest records:

- exact Trace2D commit;
- gate status and explicit gate phase/failure category;
- selected CTest fixtures;
- runner, Windows, CPU, physical GPU controller and driver metadata;
- actual SDL GPU backend reported by `Renderer::DriverName()` from the real rendering process;
- capture viewport and normalized capture format from the GPUQA environment probe;
- CMake/vcpkg/configuration identity;
- fixture policy metadata for representative camera/viewport, Material2D, Sprite and particle conformance suites;
- SHA-256 of the verbose CTest log;
- the committed support-matrix schema/path.

The environment probe emits one marker:

```text
TRACE2D_GPUQA_ENV_V1 backend=<actual> viewport_width=64 viewport_height=64 capture_format=rgba8 comparison=metadata_exact
```

The gate requires this marker. It is produced only while `TRACE2D_RUN_GPU_SMOKE=1` and therefore does not add production logging.

## Comparison policy

There is no global screenshot tolerance.

### Camera2D / Viewport2D

`CameraViewportGpuConformanceTests` reuses the #88 CPU authority instead of introducing a second GPU camera implementation. The fixture resolves a non-zero interpolated camera and a 32x16 logical viewport into a 64x64 Fit presentation, feeds the resulting `ResolvedPresentationView2D::rendererCamera` through the ordinary Renderer, and performs one explicit capture.

The CPU contract predicts the interpolated world center at presentation pixel `(32,32)`. The GPU fixture checks that semantic center and checks the top/bottom Fit letterbox remains clear with maximum per-channel absolute tolerance 8. Before explicit capture, validation readback/fence counters remain zero. The gate records this policy as `camera_viewport` / `tolerant_semantic_pixels`.

### Material2D

Existing `MaterialGpuSmokeTests` already prove real shader compilation/pipeline preparation, cache reuse, deterministic material prepare errors and representative semantic pixels. GPUQA2 makes that evidence explicit in the generic manifest instead of leaving it under `fixture_local`.

Across MAT3 and MAT4, the retained pixel assertions bound the representative white/red/half-flash outputs to a maximum per-channel absolute difference of 8 from their intended values; exact prepare-error/cache/resource assertions remain exact where the fixtures already require them. The gate records this as `mixed_exact_diagnostics_and_tolerant_semantic_pixels`.

### Sprite SR8

`SpriteRendererGpuConformanceTests` keeps the already-reviewed fixture-local policy from `SPRITE_RENDERER_CONFORMANCE_SR8.md`: semantic probe pixels use a maximum per-channel absolute tolerance of 8 for the SR8 rotated/trimmed reconstruction fixture. The gate records this as `tolerant_semantic_pixels`; it does not reinterpret unrelated Sprite fixtures or auto-update a golden image.

### Particle CPU/GPU conformance

`ParticleGpuConformanceTests` remains mixed evidence:

- discrete CPU/GPU semantic properties are exact where the particle contract is exact;
- projected bright-region center uses the existing 2-pixel tolerance;
- lifetime/step/resource counters remain explicit assertions;
- normal GPU frames must retain zero validation readbacks/fence waits.

The gate records this as `mixed_exact_and_tolerant_semantics` rather than an exact image claim.

### Other GPU smoke fixtures

Other selected `GpuSmokeTests` remain fixture-local assertions. GPUQA2 does not invent a universal tolerance or imply that smoke coverage is a cross-vendor golden comparison.

## Failure and skip policy

GPUQA1/2 records the gate phase and one of these top-level categories when the generic gate itself fails:

- `platform_or_dependency_failure` — unsupported host, missing vcpkg/toolchain;
- `configure_failure`;
- `build_failure`;
- `test_discovery_failure`;
- `gpu_fixture_failure` — a selected real-GPU fixture failed; fixture logs remain the detailed authority;
- `unsupported_or_skipped_fixture` — selected fixture skipped even though the maintained Tier B target promises it;
- `environment_probe_failure` — actual SDL backend/capture identity could not be recorded;
- `evidence_generation_failure`.

This taxonomy is deliberately not a heuristic parser for arbitrary assertion text. A later #92 slice may add structured fixture-owned categories for shader compilation, resource creation, device loss, readback/capture and comparison mismatch. GPUQA2 does not guess those categories from test names or assertion strings.

A selected Tier B fixture may not silently skip. If a capability is intentionally unsupported, the support matrix must first narrow the claim and a later fixture contract must represent that state explicitly.

## Performance boundary

Allowed only in explicit GPU QA/capture workloads:

- GPU fences/waits;
- readback;
- normalized RGBA8 capture;
- comparison/assertion work;
- JSON/evidence formatting;
- filesystem writes;
- environment discovery.

Forbidden in ordinary gameplay/render frames solely for validation:

- duplicate CPU reference execution;
- screenshot/golden hashing;
- hidden readback/fence waits;
- filesystem or JSON reporting;
- timing gates tied to an unstable machine.

Timing evidence continues to use #91's environment-aware profiling vocabulary. GPUQA2 adds no timing threshold.

## External reference decision

Current references for GPUQA2 follow the repository external-reference protocol:

- **ADAPT — wgpu GPU testing:** keep backend-independent validation separate from real-GPU integration tests and exercise normal public GPU-facing APIs on actual hardware; retain fixture-local expectations rather than a global tolerance.
- **ADAPT — Godot regression-test project:** use a small reproducible rendering workload that exposes regressions without depending on broad manual visual inspection.
- **REUSE — SDL3 / Trace2D Renderer:** SDL3 exposes `SDL_GetGPUDeviceDriver` as the backend identity for a created GPU device, already surfaced by `Renderer::DriverName()` and GPUQA1. GPUQA2 adds no second backend probe.
- **REJECT — new image comparison dependency:** existing bounded semantic-pixel assertions are sufficient for this narrow camera/material evidence gap.

## Completion boundary

GPUQA2 is complete when exact-head hosted CI and the trusted owner GPU gate are green with the new Camera2D/Viewport2D fixture selected with no skips, and the uploaded `trace2d.gpu-gate.v2` manifest explicitly classifies representative camera/viewport, Material2D, Sprite and particle policies alongside the exact GPU environment.

#92 remains open afterward for structured fixture-owned failure categories and a final acceptance audit. Tier C remains `not_established` unless separately owned infrastructure is added; lack of Tier C does not block the narrower maintained Tier B claim.
