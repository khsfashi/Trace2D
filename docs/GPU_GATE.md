# Owner real-GPU gate automation

Status: **bootstrap pending owner runner registration**  
Tracking: **#140**

Trace2D's real presentation-GPU tests are intentionally separate from ordinary GitHub-hosted CI. The repository now has one generic owner GPU gate that runs all opt-in real-GPU smoke/conformance fixtures on a trusted Windows self-hosted runner.

## Trust boundary

Trace2D is public. A self-hosted runner can execute repository code with the permissions of its local Windows account, so arbitrary public pull-request code must never be routed to the owner's machine.

The GPU workflow therefore:

- has **no** `pull_request` or `pull_request_target` trigger,
- runs automatically only for pushes inside `khsfashi/Trace2D` to `main` or `agent/**`,
- routes only to `[self-hosted, windows, x64, trace2d-gpu]`,
- uses `contents: read`,
- disables persisted checkout credentials,
- serializes the physical GPU runner and cancels stale queued/running branch revisions.

Fork PRs remain hosted-CI-only. Broader tiered GPU/backend/release qualification remains owned by #92.

## Presentation-session requirement

Trace2D's current real-GPU fixtures create a windowed presentation path rather than only exercising headless compute. The owner runner therefore runs in the owner's **interactive Windows logon session**, not as a Windows service in Session 0.

For this gate:

- configure the GitHub Actions runner **without** Windows service mode,
- bootstrap it with `run.cmd` from the logged-in Windows account,
- after the first successful GPU Gate run, configure `run.cmd` to start automatically at owner logon using Windows Task Scheduler with an interactive token,
- the machine must be powered on and the owner Windows account must have logged in before queued presentation-GPU work can execute,
- do not claim service-mode presentation support unless #92 later proves it on the maintained backend/environment matrix.

This is deliberately stricter than generic GitHub runner guidance because the Trace2D acceptance contract includes real window/presentation behavior.

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

Follow GitHub's generated download/configuration commands on the GPU machine. `C:\actions-runner` remains the preferred dedicated directory.

During configuration:

1. register the runner specifically to `khsfashi/Trace2D`,
2. add the custom label `trace2d-gpu`,
3. keep the default `self-hosted`, `windows`, and `x64` labels,
4. choose **No** when asked to run the Windows runner as a service,
5. keep the runner under the same interactive Windows account/environment that already proves the Trace2D presentation GPU tests locally.

The registration token shown by GitHub is time-limited. Do not commit or paste that token into repository files, workflow YAML, issue comments, artifacts, or chat transcripts intended for sharing.

The workflow provisions the pinned Trace2D vcpkg baseline in the runner tool cache, so it does not depend on the owner's existing interactive-shell `VCPKG_ROOT`.

After configuration, start the runner once interactively with:

```powershell
.\run.cmd
```

Leave that window running for the bootstrap GPU Gate. When the first workflow succeeds, configure `run.cmd` as an at-logon scheduled task using `InteractiveToken`; that keeps the runner in the real user presentation session while removing normal manual startup.

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
  -> manifest + verbose log artifact
  -> pass/fail visible on the exact pushed commit
```

No `cd`, `git fetch`, CMake command, environment-variable command, or CTest command should be required from the owner for normal future GPU validation once the interactive runner autostarts at logon.

## Primary references reviewed for #140

- GitHub Docs — Adding self-hosted runners
- GitHub Docs — Using self-hosted runners in a workflow
- GitHub Docs — Configuring the self-hosted runner application as a service

Decision summary: **ADOPT** repository-scoped Windows self-hosting/default+custom labels; **ADAPT** event routing to trusted in-repository pushes because Trace2D is public; **ADAPT** runner lifetime to an interactive at-logon session because Trace2D's gate exercises windowed presentation; **DEFER** broad #92 GPU release qualification and service/headless presentation claims.