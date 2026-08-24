param(
    [string]$OutputDirectory = "artifacts/sprite-renderer-final-gate",
    [string]$ExistingGpuEvidenceDirectory = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repositoryRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repositoryRoot
try {
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $resolvedOutput = (Resolve-Path $OutputDirectory).Path
    $headSha = (& git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($headSha)) {
        throw "Unable to resolve the current Trace2D commit."
    }

    # Reuse already-produced trusted GPU evidence when the workflow supplies it. Standalone owner
    # execution keeps the convenient behavior of running the generic GPU gate itself.
    $gpuEvidenceMode = "delegated"
    if ([string]::IsNullOrWhiteSpace($ExistingGpuEvidenceDirectory)) {
        $gpuEvidenceDirectory = Join-Path $resolvedOutput "gpu"
        & (Join-Path $PSScriptRoot "gpu_gate.ps1") `
            -OutputDirectory $gpuEvidenceDirectory `
            -GpuTestRegex "Sprite.*Gpu(Smoke|Conformance)Tests"
    }
    else {
        if (-not (Test-Path $ExistingGpuEvidenceDirectory -PathType Container)) {
            throw "Existing GPU evidence directory was not found: $ExistingGpuEvidenceDirectory"
        }
        $gpuEvidenceDirectory = (Resolve-Path $ExistingGpuEvidenceDirectory).Path
        $gpuEvidenceMode = "reused"
    }

    $gpuManifestPath = Join-Path $gpuEvidenceDirectory "manifest.json"
    if (-not (Test-Path $gpuManifestPath -PathType Leaf)) {
        throw "Sprite GPU gate did not produce manifest.json."
    }
    $gpuManifest = Get-Content -Raw -Path $gpuManifestPath | ConvertFrom-Json
    switch ($gpuManifest.schema) {
        "trace2d.gpu-gate.v1" { $gpuGateStatus = $gpuManifest.status }
        "trace2d.gpu-gate.v2" { $gpuGateStatus = $gpuManifest.gate.status }
        default { throw "Unsupported generic GPU gate manifest schema '$($gpuManifest.schema)'." }
    }
    if ($gpuGateStatus -ne "passed" -or $gpuManifest.commit -ne $headSha) {
        throw "Sprite GPU gate evidence does not match the current successful commit."
    }

    $requiredGpuSuites = @(
        "SpriteGpuSmokeTests",
        "SpriteOrderMaskGpuSmokeTests",
        "SpritePrimitiveGpuSmokeTests",
        "SpritePixelPerfectGpuSmokeTests",
        "SpriteBatchGpuSmokeTests",
        "SpriteRendererGpuConformanceTests"
    )
    foreach ($suite in $requiredGpuSuites) {
        $matchingTests = @($gpuManifest.selected_tests | Where-Object { $_ -like "$suite.*" })
        if ($matchingTests.Count -eq 0) {
            throw "Required Sprite GPU suite '$suite' was not selected by the trusted GPU gate."
        }
    }

    $cpuOutput = & ctest `
        --preset windows-debug `
        -R "SpriteRendererConformanceTests" `
        --output-on-failure `
        -V 2>&1
    $cpuExitCode = $LASTEXITCODE
    $cpuText = $cpuOutput -join [Environment]::NewLine
    $cpuLogPath = Join-Path $resolvedOutput "cpu-conformance.txt"
    $cpuText | Set-Content -Path $cpuLogPath -Encoding utf8
    $cpuOutput | ForEach-Object { Write-Host $_ }
    if ($cpuExitCode -ne 0) {
        throw "SR8 backend-independent conformance failed with exit code $cpuExitCode."
    }
    if ($cpuText -match '(?im)\*\*\*Skipped|\[\s*SKIPPED\s*\]') {
        throw "SR8 backend-independent conformance evidence is invalid because a selected test was skipped."
    }

    $workloadMatch = [regex]::Match(
        $cpuText,
        'TRACE2D_SR8_WORKLOAD_V1\s+submitted=(\d+)\s+visible=(\d+)\s+culled=(\d+)\s+quads=(\d+)\s+runs=(\d+)')
    if (-not $workloadMatch.Success) {
        throw "SR8 structural workload evidence marker was not observed in verbose CTest output."
    }

    $structuralWorkload = [ordered]@{
        schema = "trace2d.sprite-renderer-workload.v1"
        source_test = "SpriteRendererConformanceTests.Sr8CommittedStructuralWorkloadHasExactRawMetrics"
        submitted_sprites = [uint64]$workloadMatch.Groups[1].Value
        visible_sprites = [uint64]$workloadMatch.Groups[2].Value
        culled_sprites = [uint64]$workloadMatch.Groups[3].Value
        visible_quads = [uint64]$workloadMatch.Groups[4].Value
        contiguous_runs = [uint64]$workloadMatch.Groups[5].Value
        metric_source = "deterministic_structure"
    }
    if ($structuralWorkload.submitted_sprites -ne [uint64]1024 -or
        $structuralWorkload.visible_sprites -ne [uint64]768 -or
        $structuralWorkload.culled_sprites -ne [uint64]256 -or
        $structuralWorkload.visible_quads -ne [uint64]960 -or
        $structuralWorkload.contiguous_runs -ne [uint64]7) {
        throw "SR8 committed structural workload changed without an explicit contract update."
    }

    $workloadTool = Join-Path `
        $repositoryRoot `
        "build/windows-msvc/tools/renderer_workload/Debug/trace2d_renderer_workload.exe"
    if (-not (Test-Path $workloadTool -PathType Leaf)) {
        throw "Debug renderer workload tool was not found at: $workloadTool"
    }

    $rendererWorkloadOutput = & $workloadTool --list 2>&1
    $rendererWorkloadExitCode = $LASTEXITCODE
    $rendererWorkloadText = $rendererWorkloadOutput -join [Environment]::NewLine
    if ($rendererWorkloadExitCode -ne 0) {
        throw "Renderer workload structure command failed with exit code $rendererWorkloadExitCode."
    }
    try {
        $rendererWorkloads = $rendererWorkloadText | ConvertFrom-Json
    }
    catch {
        throw "Renderer workload tool did not emit valid JSON."
    }
    if ($rendererWorkloads.status -ne "ok" -or
        $rendererWorkloads.metric_source -ne "deterministic_structure" -or
        @($rendererWorkloads.workloads).Count -ne 3) {
        throw "Renderer workload structure evidence is incomplete or has an unexpected metric source."
    }

    $rendererWorkloadPath = Join-Path $resolvedOutput "renderer-workloads.json"
    $rendererWorkloadText | Set-Content -Path $rendererWorkloadPath -Encoding utf8

    $gpuManifestRelativePath = [IO.Path]::GetRelativePath($resolvedOutput, $gpuManifestPath).Replace('\\', '/')
    $manifest = [ordered]@{
        schema = "trace2d.sprite-renderer-final-gate.v1"
        generated_utc = [DateTime]::UtcNow.ToString("o")
        commit = $headSha
        comparison_policy = [ordered]@{
            deterministic_cpu = "exact_contract_values"
            gpu_color = "fixture_owned_bounded_per_channel_tolerance"
            gameplay_authority_from_pixels = $false
        }
        cpu_conformance = [ordered]@{
            path = "cpu-conformance.txt"
            sha256 = (Get-FileHash -Algorithm SHA256 -Path $cpuLogPath).Hash.ToLowerInvariant()
            structural_workload = $structuralWorkload
        }
        renderer_workloads = [ordered]@{
            path = "renderer-workloads.json"
            sha256 = (Get-FileHash -Algorithm SHA256 -Path $rendererWorkloadPath).Hash.ToLowerInvariant()
            metric_source = "deterministic_structure"
            count = @($rendererWorkloads.workloads).Count
        }
        real_gpu = [ordered]@{
            evidence_mode = $gpuEvidenceMode
            delegated_schema = $gpuManifest.schema
            manifest_path = $gpuManifestRelativePath
            manifest_sha256 = (Get-FileHash -Algorithm SHA256 -Path $gpuManifestPath).Hash.ToLowerInvariant()
            selected_test_count = $gpuManifest.selected_test_count
            required_suites = $requiredGpuSuites
            skipped = $false
        }
        interpretation = "SR8 validation evidence only. Captures are presentation evidence; deterministic Sprite state and structural metrics remain authoritative. No portable timing threshold is asserted here."
    }

    $manifestPath = Join-Path $resolvedOutput "manifest.json"
    $manifest | ConvertTo-Json -Depth 10 | Set-Content -Path $manifestPath -Encoding utf8

    Write-Host "Sprite renderer SR8 final gate passed for commit $headSha."
    Write-Host "Evidence directory: $resolvedOutput"
}
finally {
    Pop-Location
}
