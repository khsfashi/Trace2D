# Owner real-GPU gate automation

Status: **active maintained Tier B target via #92/#387**  
Bootstrap tracking: **#140**  
Support matrix: `config/gpu-qa-support-matrix.json`

Trace2D's real presentation-GPU tests are intentionally separate from ordinary GitHub-hosted CI. The repository has one generic owner GPU gate that runs all opt-in real-GPU smoke/conformance fixtures on a trusted Windows self-hosted runner. #92 now treats this runner as the first maintained Tier B environment rather than creating another GPU-runner policy.

## Trust boundary

Trace2D is public. A self-hosted runner can execute repository code with the permissions of its local Windows account, so arbitrary public pull-request code must never be routed to the owner's machine.

The GPU workflow therefore:

- has **no** `pull_request` or `pull_request_target` trigger,
- runs automatically only for pushes inside `khsfashi/Trace2D` to `main` or `agent/**`,
- routes only to `[self-hosted, windows, x64, trace2d-gpu]`,
- uses `contents: read`,
- disables persisted checkout credentials,
- serializes the physical GPU runner and cancels stale queued/running branch revisions.

Fork PRs remain hosted-CI-only. A green owner GPU run proves only the exact environment recorded by its evidence manifest; it does not imply unowned GPU vendors, drivers, SDL backends, or platforms are covered.

## Presentation-session requirement

Trace2D's current real-GPU fixtures create a windowed presentation path rather than only exercising headless compute. The owner runner therefore runs in the owner's **interactive Windows logon session**, not as a Windows service in Session 0.

For this gate:

- configure the GitHub Actions runner **without** Windows service mode,
- bootstrap it with `run.cmd` from the logged-in Windows account,
- keep `run.cmd` starting automatically at owner logon using Windows Task Scheduler with an interactive token,
- the machine must be powered on and the owner Windows account must have logged in before queued presentation-GPU work can execute,
- do not claim service-mode presentation support unless #92 later proves it on a maintained backend/environment matrix.

This is deliberately stricter than generic GitHub runner guidance because the Trace2D acceptance contract includes real window/presentation behavior.

## Test selection contract

`scripts/gpu_gate.ps1` selects CTest cases with:

```text
Gpu(Smoke|Conformance)Tests
```

This includes real presentation-GPU smoke/conformance fixtures such as particle, Sprite, Material2D, UI and the GPUQA environment probe while intentionally excluding CPU-only contract fixtures such as `ParticleGpuRuntimeTests`.

A new real-GPU test must follow that fixture naming convention. The gate fails if zero tests are selected, any selected test is skipped, CTest fails, or the GPUQA environment marker is absent/duplicated.

The gate sets:

```text
TRACE2D_RUN_GPU_SMOKE=1
```

for the selected CTest run only.

## GPUQA environment probe

`GpuQaEnvironmentConformanceTests` creates the ordinary Trace2D windowed Renderer, verifies validation readback/fence counters remain zero before explicit capture, captures one 64x64 frame, and emits the actual renderer backend through the existing `Renderer::DriverName()` surface:

```text
TRACE2D_GPUQA_ENV_V1 backend=<actual> viewport_width=64 viewport_height=64 capture_format=rgba8 comparison=metadata_exact
```

The marker exists only in explicit test execution. No production frame path gains logging or environment discovery.

## Evidence

Each run writes `artifacts/gpu-gate/` and uploads it as a GitHub Actions artifact. `manifest.json` now uses `trace2d.gpu-gate.v2` and records:

- exact Trace2D commit,
- gate status, phase and explicit top-level failure category,
- selected test names/count,
- fixture comparison-policy metadata,
- runner identity,
- Windows/CPU information,
- GPU controller names and driver versions,
- actual SDL GPU backend from the rendering process,
- GPUQA capture viewport/normalized RGBA8 format,
- CMake version, exact vcpkg commit and Debug build/test presets,
- committed support-matrix schema/hash/maintained target,
- SHA-256 of the verbose CTest log.

Representative Sprite SR8 and particle conformance keep their already-reviewed fixture-local tolerances. The generic gate does not invent a global screenshot threshold or automatically accept new goldens. See `docs/GPU_QA_CONFORMANCE.md`.

## Support claims

`config/gpu-qa-support-matrix.json` is the committed release/support authority for maintained GPU targets.

Current state:

- Tier A: backend-independent contracts remain hosted-CI responsibilities where applicable;
- Tier B: one maintained `owner-windows-primary` environment, with actual GPU/driver/backend recorded per green run;
- Tier C: **not established**.

A missing target narrows the claim. One green owner GPU does not prove bit-identical output on another GPU vendor/backend/platform.

## One-time Windows runner registration

Use the repository UI:

```text
Settings
 -> Actions
 -> Runners
 -> New self-hosted runner
 -> Windows
 -> x64
```

Follow GitHub's generated download/configuration commands on the GPU machine. `C:\actions-runner` remains the preferred dedicated directory.

During configuration:

1. register the runner specifically to `khsfashi/Trace2D`,
2. add the custom label `trace2d-gpu`,
3. keep the default `self-hosted`, `windows`, and `x64` labels,
4. choose **No** when asked to run the Windows runner as a service,
5. keep the runner under the same interactive Windows account/environment that proves the Trace2D presentation GPU tests locally.

The registration token shown by GitHub is time-limited. Do not commit or paste that token into repository files, issue comments, artifacts, or chat transcripts intended for sharing.

The workflow provisions the pinned Trace2D vcpkg baseline in the runner tool cache, so it does not depend on the owner's existing interactive-shell `VCPKG_ROOT`.

## Automatic steady-state flow

```text
owner Windows logon
  -> scheduled interactive runner starts
  -> trusted push to main or agent/**
  -> GitHub Actions GPU Gate
  -> owner Windows self-hosted runner
  -> pinned vcpkg preparation
  -> CMake configure/build
  -> every Gpu(Smoke|Conformance)Tests fixture with TRACE2D_RUN_GPU_SMOKE=1
  -> environment marker + manifest v2 + verbose log artifact
  -> pass/fail visible on the exact pushed commit
```

No `cd`, `git fetch`, CMake command, environment-variable command, or CTest command should be required from the owner for normal future GPU validation once the interactive runner autostarts at logon.

## Primary references

- GitHub Docs — Adding self-hosted runners
- GitHub Docs — Using self-hosted runners in a workflow
- GitHub Docs — Configuring the self-hosted runner application as a service
- SDL3 — `SDL_GetGPUDeviceDriver` backend identity contract

Decision summary: **ADOPT** repository-scoped Windows self-hosting/default+custom labels; **ADAPT** event routing to trusted in-repository pushes because Trace2D is public; **ADAPT** runner lifetime to an interactive at-logon session because Trace2D's gate exercises windowed presentation; **REUSE** existing `Renderer::DriverName()` for actual SDL backend evidence; **DEFER** broader Tier C vendor/backend/platform qualification until ownership and repeatable infrastructure exist.
