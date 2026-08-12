# Owner real-GPU gate automation

Status: **bootstrap pending owner runner registration**  
Tracking: **#140**

Trace2D's real presentation-GPU tests are intentionally separate from ordinary GitHub-hosted CI. The repository now has one generic owner GPU gate that runs all opt-in real-GPU smoke/conformance fixtures on a trusted Windows self-hosted runner.

## Trust boundary

Trace2D is public. A self-hosted runner can execute repository code with the permissions of its local service account, so arbitrary public pull-request code must never be routed to the owner's machine.

The GPU workflow therefore:

- has **no** `pull_request` or `pull_request_target` trigger,
- runs automatically only for pushes inside `khsfashi/Trace2D` to `main` or `agent/**`,
- routes only to `[self-hosted, windows, x64, trace2d-gpu]`,
- uses `contents: read`,
- disables persisted checkout credentials,
- serializes the physical GPU runner and cancels stale queued/running branch revisions.

Fork PRs remain hosted-CI-only. Broader tiered GPU/backend/release qualification remains owned by #92.

## Test selection contract

`scripts/gpu_gate.ps1` selects CTest cases with:

```text
Gpu(Smoke|Conformance)Tests
```

This includes real presentation-GPU smoke/conformance fixtures such as particle and Sprite GPU tests while intentionally excluding CPU-only contract fixtures such as `ParticleGpuRuntimeTests`.

A new real-GPU test must follow that fixture naming convention. The gate fails if zero tests are selected, any selected test is skipped, or CTest fails.

The gate sets:

```text
TRACE2D_RUN_GPU_SMOKE=1
```

for the selected CTest run only.

## Evidence

Each run writes `artifacts/gpu-gate/` and uploads it as a GitHub Actions artifact. `manifest.json` records:

- exact Trace2D commit,
- selected test names/count,
- pass/fail status,
- runner identity,
- Windows/CPU information,
- GPU controller names and driver versions,
- CMake version and exact vcpkg commit,
- SHA-256 of the verbose CTest log.

Pixel correctness remains asserted by the tests themselves. Persisted frame captures and a multi-GPU/backend matrix are deferred to #92 rather than being invented by this infrastructure slice.

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

Follow GitHub's generated download/configuration commands on the GPU machine. GitHub recommends `C:\actions-runner` for a Windows service installation.

During configuration:

1. register the runner specifically to `khsfashi/Trace2D`,
2. add the custom label `trace2d-gpu`,
3. keep the default `self-hosted`, `windows`, and `x64` labels,
4. configure the Windows runner as a service so it reconnects after boot,
5. use a service account/environment that can execute the installed Git, CMake, Visual Studio 2022 C++ toolchain, and presentation GPU driver.

The registration token shown by GitHub is time-limited. Do not commit or paste that token into repository files, workflow YAML, issue comments, or artifacts.

The workflow provisions the pinned Trace2D vcpkg baseline in the runner tool cache, so the service does not depend on the owner's interactive-shell `VCPKG_ROOT`.

After registration, verify the runner is online in repository settings. On Windows a configured runner service can also be inspected with:

```powershell
Get-Service "actions.runner.*"
```

## Automatic steady-state flow

```text
trusted push to main or agent/**
  -> GitHub Actions GPU Gate
  -> owner Windows self-hosted runner
  -> pinned vcpkg preparation
  -> CMake configure/build
  -> every Gpu(Smoke|Conformance)Tests fixture with TRACE2D_RUN_GPU_SMOKE=1
  -> manifest + verbose log artifact
  -> pass/fail visible on the exact pushed commit
```

No `cd`, `git fetch`, CMake command, environment-variable command, or CTest command should be required from the owner for normal future GPU validation once the runner is online.

## Primary references reviewed for #140

- GitHub Docs — Adding self-hosted runners
- GitHub Docs — Using self-hosted runners in a workflow
- GitHub Docs — Configuring the self-hosted runner application as a service

Decision summary: **ADOPT** repository-scoped Windows self-hosting/labels/service; **ADAPT** event routing to trusted in-repository pushes because Trace2D is public; **DEFER** broad #92 GPU release qualification.
