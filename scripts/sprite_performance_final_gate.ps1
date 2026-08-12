param(
    [string]$OutputDirectory = "artifacts/sprite-performance-final-gate",
    [string]$CtestPreset = "windows-debug",
    [string]$RendererWorkloadTool = "",
    [string]$AnimationWorkloadTool = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Invoke-JsonCommand {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [Parameter(Mandatory = $true)][string]$Description
    )

    $commandOutput = & $Executable @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $text = $commandOutput -join [Environment]::NewLine
    if ($exitCode -ne 0) {
        throw "$Description failed with exit code $exitCode.`n$text"
    }

    try {
        $json = $text | ConvertFrom-Json
    }
    catch {
        throw "$Description did not emit valid JSON.`n$text"
    }

    $text | Set-Content -Path $OutputPath -Encoding utf8
    return $json
}

function Require-Tool {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Description
    )

    if (-not (Test-Path $Path -PathType Leaf)) {
        throw "$Description was not found at: $Path"
    }
    return (Resolve-Path $Path).Path
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repositoryRoot
try {
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $resolvedOutput = (Resolve-Path $OutputDirectory).Path

    $headSha = (& git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($headSha)) {
        throw "Unable to resolve the current Trace2D commit."
    }

    $contractPath = Join-Path $repositoryRoot "docs/contracts/sprite-performance-sperf.json"
    if (-not (Test-Path $contractPath -PathType Leaf)) {
        throw "SPERF machine-readable contract was not found: $contractPath"
    }
    $contract = Get-Content -Raw -Path $contractPath | ConvertFrom-Json
    if ($contract.schema -ne "trace2d.sprite-performance.v1" -or $contract.stage -ne "SPERF") {
        throw "Unexpected SPERF contract schema/stage."
    }

    if ([string]::IsNullOrWhiteSpace($RendererWorkloadTool)) {
        $RendererWorkloadTool = Join-Path `
            $repositoryRoot `
            "build/windows-msvc/tools/renderer_workload/Debug/trace2d_renderer_workload.exe"
    }
    if ([string]::IsNullOrWhiteSpace($AnimationWorkloadTool)) {
        $AnimationWorkloadTool = Join-Path `
            $repositoryRoot `
            "build/windows-msvc/tools/sprite_animation_workload/Debug/trace2d_sprite_animation_workload.exe"
    }
    $RendererWorkloadTool = Require-Tool $RendererWorkloadTool "Renderer workload tool"
    $AnimationWorkloadTool = Require-Tool $AnimationWorkloadTool "Sprite animation workload tool"

    # SR8 owns the production Sprite structural workload. Parse the exact measured marker rather
    # than recreating those counts in this script.
    $sr8Output = & ctest `
        --preset $CtestPreset `
        -R "SpriteRendererConformanceTests" `
        --output-on-failure `
        -V 2>&1
    $sr8ExitCode = $LASTEXITCODE
    $sr8Text = $sr8Output -join [Environment]::NewLine
    $sr8LogPath = Join-Path $resolvedOutput "sr8-conformance.txt"
    $sr8Text | Set-Content -Path $sr8LogPath -Encoding utf8
    $sr8Output | ForEach-Object { Write-Host $_ }
    if ($sr8ExitCode -ne 0) {
        throw "SR8 backend-independent conformance failed with exit code $sr8ExitCode."
    }
    if ($sr8Text -match '(?im)\*\*\*Skipped|\[\s*SKIPPED\s*\]') {
        throw "SR8 backend-independent conformance evidence is invalid because a selected test was skipped."
    }

    $workloadMatch = [regex]::Match(
        $sr8Text,
        'TRACE2D_SR8_WORKLOAD_V1\s+submitted=(\d+)\s+visible=(\d+)\s+culled=(\d+)\s+quads=(\d+)\s+runs=(\d+)')
    if (-not $workloadMatch.Success) {
        throw "SR8 structural workload evidence marker was not observed."
    }

    $sr8Measured = [ordered]@{
        schema = "trace2d.sprite-renderer-workload.v1"
        submitted_sprites = [uint64]$workloadMatch.Groups[1].Value
        visible_sprites = [uint64]$workloadMatch.Groups[2].Value
        culled_sprites = [uint64]$workloadMatch.Groups[3].Value
        visible_quads = [uint64]$workloadMatch.Groups[4].Value
        contiguous_runs = [uint64]$workloadMatch.Groups[5].Value
        metric_source = "deterministic_structure"
    }

    $sr8Expected = $contract.renderer.sr8_committed_workload
    foreach ($field in @("submitted_sprites", "visible_sprites", "culled_sprites", "visible_quads", "contiguous_runs")) {
        if ([uint64]$sr8Measured[$field] -ne [uint64]$sr8Expected.$field) {
            throw "SR8 field '$field' changed without an explicit SPERF contract update."
        }
    }
    if ($sr8Measured.metric_source -ne $sr8Expected.metric_source) {
        throw "SR8 metric source does not match the SPERF contract."
    }

    $rendererWorkloadPath = Join-Path $resolvedOutput "renderer-workloads.json"
    $rendererWorkloads = Invoke-JsonCommand `
        -Executable $RendererWorkloadTool `
        -Arguments @("--list") `
        -OutputPath $rendererWorkloadPath `
        -Description "Renderer workload structure command"

    if ($rendererWorkloads.status -ne "ok" -or
        $rendererWorkloads.metric_source -ne "deterministic_structure") {
        throw "Renderer workload structure evidence has an unexpected status/source."
    }

    $actualRendererWorkloads = @($rendererWorkloads.workloads)
    $expectedRendererWorkloads = @($contract.renderer.supplementary_workloads)
    if ($actualRendererWorkloads.Count -ne $expectedRendererWorkloads.Count) {
        throw "Renderer workload count does not match the SPERF contract."
    }

    foreach ($expected in $expectedRendererWorkloads) {
        $matches = @($actualRendererWorkloads | Where-Object { $_.name -eq $expected.name })
        if ($matches.Count -ne 1) {
            throw "Expected exactly one renderer workload named '$($expected.name)'."
        }
        $actual = $matches[0]
        foreach ($field in @("authored_sprites", "visible_sprites", "culled_sprites", "contiguous_texture_runs")) {
            if ([uint64]$actual.$field -ne [uint64]$expected.$field) {
                throw "Renderer workload '$($expected.name)' field '$field' does not match SPERF."
            }
        }
    }

    $animationEvidence = @()
    foreach ($workloadName in @($contract.animation.workloads)) {
        $safeName = [string]$workloadName
        $animationPath = Join-Path $resolvedOutput "animation-$safeName.json"
        $animation = Invoke-JsonCommand `
            -Executable $AnimationWorkloadTool `
            -Arguments @("--workload", $safeName) `
            -OutputPath $animationPath `
            -Description "Sprite animation workload '$safeName'"

        if ($animation.schema -ne $contract.animation.structural_schema -or
            $animation.status -ne "ok" -or
            $animation.workload -ne $safeName -or
            $animation.deterministic_replay -ne $true) {
            throw "Sprite animation workload '$safeName' did not provide valid deterministic replay evidence."
        }

        $animationEvidence += [ordered]@{
            workload = $safeName
            path = "animation-$safeName.json"
            sha256 = (Get-FileHash -Algorithm SHA256 -Path $animationPath).Hash.ToLowerInvariant()
            transcript_digest_fnv1a64 = [string]$animation.transcript_digest_fnv1a64
        }
    }

    $payload = $contract.renderer.built_in_vertex_payload
    $derivedBytesPerQuad = [uint64]$payload.vertices_per_quad * [uint64]$payload.bytes_per_vertex
    $derivedSr8UploadBytes = [uint64]$sr8Measured.visible_quads * $derivedBytesPerQuad
    if ($derivedBytesPerQuad -ne [uint64]$payload.bytes_per_visible_quad -or
        $derivedSr8UploadBytes -ne [uint64]$payload.sr8_visible_vertex_upload_bytes) {
        throw "SPERF built-in Sprite vertex payload arithmetic is internally inconsistent."
    }

    $manifest = [ordered]@{
        schema = "trace2d.sprite-performance-final-gate.v1"
        generated_utc = [DateTime]::UtcNow.ToString("o")
        commit = $headSha
        contract = [ordered]@{
            path = "docs/contracts/sprite-performance-sperf.json"
            schema = $contract.schema
            sha256 = (Get-FileHash -Algorithm SHA256 -Path $contractPath).Hash.ToLowerInvariant()
        }
        renderer = [ordered]@{
            sr8_conformance = [ordered]@{
                path = "sr8-conformance.txt"
                sha256 = (Get-FileHash -Algorithm SHA256 -Path $sr8LogPath).Hash.ToLowerInvariant()
                structural_workload = $sr8Measured
            }
            supplementary_workloads = [ordered]@{
                path = "renderer-workloads.json"
                sha256 = (Get-FileHash -Algorithm SHA256 -Path $rendererWorkloadPath).Hash.ToLowerInvariant()
                count = $actualRendererWorkloads.Count
            }
            built_in_vertex_payload = [ordered]@{
                vertices_per_quad = [uint64]$payload.vertices_per_quad
                bytes_per_vertex = [uint64]$payload.bytes_per_vertex
                bytes_per_visible_quad = $derivedBytesPerQuad
                sr8_visible_vertex_upload_bytes = $derivedSr8UploadBytes
                retained_capacity_metric = [string]$contract.renderer.retained_capacity.metric
                driver_allocation_claimed = $false
            }
        }
        animation = [ordered]@{
            schema = [string]$contract.animation.structural_schema
            workloads = $animationEvidence
            optional_timing_in_portable_gate = $false
        }
        memory_policy = [ordered]@{
            rgba8_page_byte_formula = [string]$contract.memory.rgba8_page_byte_formula
            driver_gpu_allocation_is_not_inferred = [bool]$contract.memory.driver_gpu_allocation_is_not_inferred
            package_and_resource_lifetime_owners = @($contract.memory.package_and_resource_lifetime_owners)
        }
        hot_path = [ordered]@{
            production_code_changed_by_gate = $false
            runtime_instrumentation_required = $false
            reporting_scope = "explicit_tooling_only"
        }
        timing_policy = [ordered]@{
            shared_ci_threshold = $false
            deterministic_gate_contains_wall_clock_threshold = $false
            optional_timing_requires_environment_metadata = $true
        }
        interpretation = "SPERF composes existing Sprite structural/runtime evidence. Engine-owned payload/capacity bytes are not driver allocation; local timing is environment evidence and never portable correctness truth."
    }

    $manifestPath = Join-Path $resolvedOutput "manifest.json"
    $manifest | ConvertTo-Json -Depth 12 | Set-Content -Path $manifestPath -Encoding utf8

    Write-Host "Sprite SPERF deterministic final gate passed for commit $headSha."
    Write-Host "Evidence directory: $resolvedOutput"
}
finally {
    Pop-Location
}
