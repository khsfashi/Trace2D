# Trace2D GPU QA conformance contract

Status: **GPUQA3 active via #391; GPUQA1/#387 and GPUQA2/#389 complete**  
Parent: **#92 GPUQA0**

Trace2D separates backend-independent correctness from explicit real-GPU presentation evidence. GPU QA never moves gameplay truth, readback, fence waits, image comparison, JSON, or filesystem work into ordinary frames.

## Tiers

### Tier A — hosted/backend-independent

Pure math, serialization, shader/toolchain validation, Sprite geometry/order/culling, camera/viewport mapping, Material2D layout, particle CPU-reference semantics, and canonical image encoding remain normal hosted-CI responsibilities wherever they do not require a presentation GPU.

### Tier B — maintained real GPU

The current maintained target is `owner-windows-primary` from `config/gpu-qa-support-matrix.json`. It is a trusted interactive Windows x64 self-hosted runner. A green run proves only the exact machine environment recorded in that run's manifest; it does not imply another GPU vendor, driver, backend, or platform is equivalent.

### Tier C — release matrix

GPUQA3 intentionally has no Tier C targets. `tier_c_claim` remains `not_established` until additional maintained targets have explicit owners and repeatable infrastructure.

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
- a frozen `trace2d.gpuqa.fixture-outcome.v1` vocabulary plus final structured outcomes for required representative fixtures;
- SHA-256 of the verbose CTest log;
- the committed support-matrix schema/path.

The environment probe emits one marker:

```text
TRACE2D_GPUQA_ENV_V1 backend=<actual> viewport_width=64 viewport_height=64 capture_format=rgba8 comparison=metadata_exact
```

Required representative fixtures emit one final outcome marker through the test-only `GpuQaFixtureOutcome` helper:

```text
TRACE2D_GPUQA_FIXTURE_V1 test=<gtest-name> phase=<phase> failure_category=<category>
```

On success the required final state is always `phase=complete failure_category=none`. On a fatal assertion or thrown exception the helper preserves the most recently declared validation phase. The helper exists only in `tests/render`; no runtime API or ordinary-frame logging is added.

The gate requires the environment marker and every required structured fixture outcome. It runs with `TRACE2D_RUN_GPU_SMOKE=1`; none of these markers are production logging.

## Comparison policy

There is no global screenshot tolerance.

### Camera2D / Viewport2D

`CameraViewportGpuConformanceTests` reuses the #88 CPU authority instead of introducing a second GPU camera implementation. The fixture resolves a non-zero interpolated camera and a 32x16 logical viewport into a 64x64 Fit presentation, feeds the resulting `ResolvedPresentationView2D::rendererCamera` through the ordinary Renderer, and performs one explicit capture.

The CPU contract predicts the interpolated world center at presentation pixel `(32,32)`. The GPU fixture checks that semantic center and checks the top/bottom Fit letterbox remains clear with maximum per-channel absolute tolerance 8. Before explicit capture, validation readback/fence counters remain zero. The gate records this policy as `camera_viewport` / `tolerant_semantic_pixels`.

### Material2D

Existing `MaterialGpuSmokeTests` prove real shader compilation/pipeline preparation, cache reuse, deterministic material prepare errors and representative semantic pixels. GPUQA3 additionally maps the typed `MaterialGpuPrepareError2D` values into the shared structured categories instead of inferring shader/pipeline failure from assertion text.

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

Other selected `GpuSmokeTests` remain fixture-local assertions. GPUQA3 does not invent a universal tolerance or imply that smoke coverage is a cross-vendor golden comparison.

## Structured failure and skip policy

Generic gate/infrastructure failures remain distinct:

- `platform_or_dependency_failure` — unsupported host, missing vcpkg/toolchain;
- `configure_failure`;
- `build_failure`;
- `test_discovery_failure`;
- `gpu_fixture_failure` — fallback only when a failing selected fixture has no structured representative outcome;
- `environment_probe_failure`;
- `fixture_outcome_contract_failure` — a required representative fixture was not selected or did not produce exactly one final outcome;
- `evidence_generation_failure`.

Representative fixture-owned outcomes use this frozen vocabulary:

- `unsupported_capability`;
- `gpu_device_initialization_failure`;
- `shader_compile_or_reflection_failure`;
- `pipeline_or_resource_creation_failure`;
- `render_submit_or_device_loss_failure`;
- `readback_or_capture_failure`;
- `comparison_mismatch`.

The gate does not parse arbitrary assertion strings to invent these categories. Each representative test declares its current failure point before the relevant operation. Material2D additionally converts its existing typed prepare error directly into the shader/pipeline category.

A selected Tier B fixture may not silently skip. A structured `unsupported_capability` outcome remains a failing maintained-target result unless the support matrix is explicitly narrowed first. A skip without a structured declaration is separately reported as `unexpected_skipped_fixture`.

Required structured outcomes currently cover the GPU environment probe, Camera2D/Viewport2D, representative Material2D, Sprite SR8 and particle CPU/GPU conformance plus the outcome-contract self-tests.

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

Timing evidence continues to use #91's environment-aware profiling vocabulary. GPUQA3 adds no timing threshold.

## External reference decision

Current references continue to follow the repository external-reference protocol:

- **ADAPT — wgpu GPU testing:** keep backend-independent validation separate from real-GPU integration tests and exercise normal public GPU-facing APIs on actual hardware; retain fixture-local expectations rather than a global tolerance.
- **ADAPT — Godot regression-test project:** use small reproducible rendering workloads that expose regressions without depending on broad manual visual inspection.
- **REUSE — SDL3 / Trace2D Renderer:** SDL3 exposes `SDL_GetGPUDeviceDriver` as the backend identity for a created GPU device, already surfaced by `Renderer::DriverName()`; no second backend probe is added.
- **REJECT — assertion-string failure inference:** fixture code owns failure phase/category because parsing human assertion text is brittle and can silently misclassify failures.
- **REJECT — new image comparison dependency:** existing bounded semantic-pixel assertions are sufficient for the current representative evidence.

## Completion boundary

GPUQA3 is complete when exact-head hosted CI and the trusted owner GPU gate are green, the manifest contains `complete/none` structured outcomes for every required representative fixture, and failure/skip categories remain distinct without runtime-path instrumentation.

#92 remains open afterward only for a final acceptance/support audit against its eight acceptance criteria. Tier C remains `not_established` unless separately owned infrastructure is added; lack of Tier C does not block the narrower maintained Tier B claim.
