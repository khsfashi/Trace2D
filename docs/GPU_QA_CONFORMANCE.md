# Trace2D GPU QA conformance contract

Status: **GPUQA1 active via #387**  
Parent: **#92 GPUQA0**

Trace2D separates backend-independent correctness from explicit real-GPU presentation evidence. GPU QA never moves gameplay truth, readback, fence waits, image comparison, JSON, or filesystem work into ordinary frames.

## Tiers

### Tier A — hosted/backend-independent

Pure math, serialization, shader/toolchain validation, Sprite geometry/order/culling, camera/viewport mapping, Material2D layout, particle CPU-reference semantics, and canonical image encoding remain normal hosted-CI responsibilities wherever they do not require a presentation GPU.

### Tier B — maintained real GPU

The current maintained target is `owner-windows-primary` from `config/gpu-qa-support-matrix.json`. It is a trusted interactive Windows x64 self-hosted runner. A green run proves only the exact machine environment recorded in that run's manifest; it does not imply another GPU vendor, driver, backend, or platform is equivalent.

### Tier C — release matrix

GPUQA1 intentionally has no Tier C targets. `tier_c_claim` remains `not_established` until additional maintained targets have explicit owners and repeatable infrastructure.

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
- fixture policy metadata for representative conformance suites;
- SHA-256 of the verbose CTest log;
- the committed support-matrix schema/path.

The environment probe emits one marker:

```text
TRACE2D_GPUQA_ENV_V1 backend=<actual> viewport_width=64 viewport_height=64 capture_format=rgba8 comparison=metadata_exact
```

The gate requires this marker. It is produced only while `TRACE2D_RUN_GPU_SMOKE=1` and therefore does not add production logging.

## Comparison policy

There is no global screenshot tolerance.

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

Other selected `GpuSmokeTests` remain fixture-local assertions. GPUQA1 does not invent a universal tolerance or imply that smoke coverage is a cross-vendor golden comparison.

## Failure and skip policy

GPUQA1 records the gate phase and one of these top-level categories when the generic gate itself fails:

- `platform_or_dependency_failure` — unsupported host, missing vcpkg/toolchain;
- `configure_failure`;
- `build_failure`;
- `test_discovery_failure`;
- `gpu_fixture_failure` — a selected real-GPU fixture failed; fixture logs remain the detailed authority;
- `unsupported_or_skipped_fixture` — selected fixture skipped even though the maintained Tier B target promises it;
- `environment_probe_failure` — actual SDL backend/capture identity could not be recorded;
- `evidence_generation_failure`.

This taxonomy is deliberately not a heuristic parser for arbitrary assertion text. Later #92 slices may add structured fixture-owned categories for shader compilation, resource creation, device loss, readback/capture and comparison mismatch. GPUQA1 does not guess those categories from strings.

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

Timing evidence continues to use #91's environment-aware profiling vocabulary. GPUQA1 adds no timing threshold.

## External API reference decision

SDL3 exposes `SDL_GetGPUDeviceDriver` as the backend identity for a created GPU device. Trace2D already exposes that backend through `Renderer::DriverName()`, so GPUQA1 reuses the existing renderer surface instead of adding another backend probe or private native-handle escape hatch.

## Completion boundary

GPUQA1 is complete when exact-head hosted CI and the trusted owner GPU gate are green with the new environment fixture and `trace2d.gpu-gate.v2` manifest.

#92 remains open afterward for broader representative camera/material/particle evidence, structured fixture-level failure categories, and any future Tier C targets. No wider support claim is made by this slice.
