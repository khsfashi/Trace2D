param(
    [Parameter(Mandatory = $true)]
    [string]$MachineLabel,

    [Parameter(Mandatory = $true)]
    [string]$CpuModel,

    [string]$OutputDirectory = "artifacts/particle-final-gate",
    [ValidateRange(0, 10000)]
    [int]$WarmupIterations = 10,
    [ValidateRange(1, 10000)]
    [int]$MeasuredIterations = 50
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Invoke-NativeChecked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [scriptblock]$Command
    )

    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE."
    }
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repositoryRoot
try {
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $resolvedOutput = (Resolve-Path $OutputDirectory).Path

    $headSha = (& git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($headSha)) {
        throw "Unable to resolve the current Git commit."
    }

    Invoke-NativeChecked "CMake configure" { cmake --preset windows-msvc }
    Invoke-NativeChecked "Debug build" { cmake --build --preset windows-debug --parallel }

    $previousGpuSmoke = $env:TRACE2D_RUN_GPU_SMOKE
    $env:TRACE2D_RUN_GPU_SMOKE = "1"
    try {
        $gpuTestOutput = & ctest --preset windows-debug -R "ParticleGpu(Smoke|Conformance)Tests" --output-on-failure -V 2>&1
        $gpuTestExitCode = $LASTEXITCODE
        $gpuTestText = $gpuTestOutput -join [Environment]::NewLine
        $gpuTestText | Set-Content -Path (Join-Path $resolvedOutput "gpu-tests.txt") -Encoding utf8

        if ($gpuTestExitCode -ne 0) {
            throw "Real-GPU particle tests failed with exit code $gpuTestExitCode."
        }
        if ($gpuTestText -match "(?i)skipped") {
            throw "Real-GPU evidence is invalid because at least one selected test was skipped."
        }
        if ($gpuTestText -notmatch "ParticleGpuSmokeTests\.ExplicitGpuEmitterAdvancesCapturesAndReusesCapacity") {
            throw "GPU smoke test name was not observed in verbose CTest output."
        }
        if ($gpuTestText -notmatch "ParticleGpuConformanceTests\.ExplicitGpuExecutionTracksCpuOracleAcrossRandomSpawnMotionAndLifetime") {
            throw "GPU conformance test name was not observed in verbose CTest output."
        }
    }
    finally {
        $env:TRACE2D_RUN_GPU_SMOKE = $previousGpuSmoke
    }

    Invoke-NativeChecked "Release build" { cmake --build --preset windows-release --parallel }

    $analyzer = Join-Path $repositoryRoot "build/windows-msvc/tools/particle_analyze/Release/trace2d_particle_analyze.exe"
    if (-not (Test-Path $analyzer -PathType Leaf)) {
        throw "Release particle analyzer was not found at: $analyzer"
    }

    $workloads = @(
        "workload_cpu_small",
        "workload_cpu_medium",
        "workload_gpu_scale"
    )

    $evidenceFiles = @()
    foreach ($workload in $workloads) {
        $effect = "tests/particles/fixtures/$workload.trace2d.particle.toml"
        $rawOutput = & $analyzer `
            --project-root . `
            --effect $effect `
            --frames 240 `
            --seed 1311768467463790320 `
            --stable-id 1234605616436508552 `
            --timing `
            --warmup $WarmupIterations `
            --iterations $MeasuredIterations `
            --machine-label $MachineLabel `
            --cpu-model $CpuModel 2>&1
        $analyzerExitCode = $LASTEXITCODE
        if ($analyzerExitCode -ne 0) {
            throw "Particle analyzer failed for $workload with exit code $analyzerExitCode.`n$($rawOutput -join [Environment]::NewLine)"
        }

        $jsonText = $rawOutput -join [Environment]::NewLine
        try {
            $parsed = $jsonText | ConvertFrom-Json
        }
        catch {
            throw "Particle analyzer did not emit valid JSON for $workload."
        }

        if ($parsed.status -ne "ok") {
            throw "Particle analyzer reported non-ok status for $workload."
        }
        if ($parsed.backend_changed_by_analyzer -ne $false) {
            throw "Particle analyzer changed backend state for $workload."
        }
        if ($null -eq $parsed.timing -or $parsed.timing.metric_source -ne "machine_dependent_timing") {
            throw "Timing evidence is missing for $workload."
        }
        if ($parsed.timing.environment.build_configuration -ne "Release") {
            throw "Timing evidence for $workload was not produced by a Release build."
        }
        if ($parsed.timing.measured_iterations -ne $MeasuredIterations) {
            throw "Timing iteration count mismatch for $workload."
        }

        $outputPath = Join-Path $resolvedOutput "$workload.json"
        $jsonText | Set-Content -Path $outputPath -Encoding utf8
        $evidenceFiles += [ordered]@{
            workload = $workload
            path = (Split-Path -Leaf $outputPath)
            sha256 = (Get-FileHash -Algorithm SHA256 -Path $outputPath).Hash.ToLowerInvariant()
            selected_backend = $parsed.selected_backend
            capacity = $parsed.workload.capacity_per_emitter
            peak_alive = $parsed.workload.peak_alive
            prepared_cpu_state_bytes = $parsed.cpu_reference_memory.prepared_state_bytes
            particle_updates = $parsed.work_counts.particle_updates
            average_ns = $parsed.timing.average_ns
            median_ns = $parsed.timing.median_ns
            p95_ns = $parsed.timing.p95_ns
            ns_per_particle_update = $parsed.timing.ns_per_particle_update
        }
    }

    $gpuControllers = @()
    try {
        $gpuControllers = @(Get-CimInstance Win32_VideoController | ForEach-Object {
            [ordered]@{
                name = $_.Name
                driver_version = $_.DriverVersion
            }
        })
    }
    catch {
        $gpuControllers = @([ordered]@{
            name = "unavailable"
            driver_version = "unavailable"
        })
    }

    $manifest = [ordered]@{
        schema = "trace2d.particle-final-gate.v1"
        generated_utc = [DateTime]::UtcNow.ToString("o")
        commit = $headSha
        machine_label = $MachineLabel
        cpu_model = $CpuModel
        gpu_controllers = $gpuControllers
        build = [ordered]@{
            configure_preset = "windows-msvc"
            gpu_test_preset = "windows-debug"
            timing_build_preset = "windows-release"
        }
        gpu_test_log = [ordered]@{
            path = "gpu-tests.txt"
            sha256 = (Get-FileHash -Algorithm SHA256 -Path (Join-Path $resolvedOutput "gpu-tests.txt")).Hash.ToLowerInvariant()
            skipped = $false
        }
        timing = [ordered]@{
            warmup_iterations = $WarmupIterations
            measured_iterations = $MeasuredIterations
            frames_per_iteration = 240
            workloads = $evidenceFiles
        }
        interpretation = "Raw final-gate evidence only. This script never changes particle backend selection or invents a safe budget."
    }

    $manifestPath = Join-Path $resolvedOutput "manifest.json"
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -Path $manifestPath -Encoding utf8

    Write-Host "Particle final gate passed for commit $headSha."
    Write-Host "Evidence directory: $resolvedOutput"
    Write-Host "Return manifest.json plus the three workload JSON files for #53 final review."
}
finally {
    Pop-Location
}
