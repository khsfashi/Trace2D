param(
    [string]$OutputDirectory = "artifacts/gpu-gate",
    [string]$GpuTestRegex = "Gpu(Smoke|Conformance)Tests"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Invoke-NativeChecked {
    param([string]$Name, [scriptblock]$Command)
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE."
    }
}

function Get-GpuFixturePolicy {
    param([string]$TestName)

    if ($TestName.StartsWith("GpuQaEnvironmentConformanceTests.")) {
        return [ordered]@{
            test = $TestName
            subsystem = "gpu_qa_environment"
            comparison_mode = "metadata_exact"
            tolerance = $null
        }
    }
    if ($TestName.StartsWith("SpriteRendererGpuConformanceTests.")) {
        return [ordered]@{
            test = $TestName
            subsystem = "sprite"
            comparison_mode = "tolerant_semantic_pixels"
            tolerance = [ordered]@{
                metric = "max_per_channel_absolute_difference"
                value = 8
                alpha = "included"
                source = "docs/SPRITE_RENDERER_CONFORMANCE_SR8.md"
            }
        }
    }
    if ($TestName.StartsWith("ParticleGpuConformanceTests.")) {
        return [ordered]@{
            test = $TestName
            subsystem = "particles"
            comparison_mode = "mixed_exact_and_tolerant_semantics"
            tolerance = [ordered]@{
                metric = "projected_bright_region_center_pixels"
                value = 2.0
                discrete_semantics = "exact_where_contract_is_exact"
                source = "tests/render/ParticleGpuConformanceTests.cpp"
            }
        }
    }

    return [ordered]@{
        test = $TestName
        subsystem = "fixture_local"
        comparison_mode = "fixture_local_assertions"
        tolerance = $null
    }
}

function Resolve-FailureCategory {
    param([string]$Phase)

    switch ($Phase) {
        "preflight" { return "platform_or_dependency_failure" }
        "configure" { return "configure_failure" }
        "build" { return "build_failure" }
        "discover" { return "test_discovery_failure" }
        "gpu_execution" { return "gpu_fixture_failure" }
        "environment_probe" { return "environment_probe_failure" }
        "evidence" { return "evidence_generation_failure" }
        default { return "evidence_generation_failure" }
    }
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repositoryRoot
$status = "failed"
$phase = "preflight"
$failureCategory = $null
$errorMessage = $null
$selectedTests = @()
$fixturePolicies = @()
$headSha = "unavailable"
$testLogPath = $null
$gpuEnvironment = $null
$supportMatrixSchema = "unavailable"
$supportMatrixHash = $null
try {
    if ($env:OS -ne "Windows_NT") {
        throw "The real-GPU gate requires Windows."
    }
    if ([string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {
        throw "VCPKG_ROOT is not configured."
    }
    $toolchain = Join-Path $env:VCPKG_ROOT "scripts/buildsystems/vcpkg.cmake"
    if (-not (Test-Path $toolchain -PathType Leaf)) {
        throw "vcpkg toolchain not found under VCPKG_ROOT."
    }

    $supportMatrixPath = Join-Path $repositoryRoot "config/gpu-qa-support-matrix.json"
    if (-not (Test-Path $supportMatrixPath -PathType Leaf)) {
        throw "GPU QA support matrix is missing: $supportMatrixPath"
    }
    $supportMatrix = Get-Content -Raw -Path $supportMatrixPath | ConvertFrom-Json
    if ($supportMatrix.schema -ne "trace2d.gpu-qa-support-matrix.v1") {
        throw "Unexpected GPU QA support matrix schema '$($supportMatrix.schema)'."
    }
    $supportMatrixSchema = $supportMatrix.schema
    $supportMatrixHash = (Get-FileHash -Algorithm SHA256 -Path $supportMatrixPath).Hash.ToLowerInvariant()

    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $resolvedOutput = (Resolve-Path $OutputDirectory).Path
    $testLogPath = Join-Path $resolvedOutput "gpu-tests.txt"

    $headSha = (& git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($headSha)) {
        throw "Unable to resolve current Trace2D commit."
    }

    $phase = "configure"
    Invoke-NativeChecked "CMake configure" { cmake --preset windows-msvc }

    $phase = "build"
    Invoke-NativeChecked "Debug build" { cmake --build --preset windows-debug --parallel }

    $phase = "discover"
    $listOutput = & ctest --preset windows-debug -N -R $GpuTestRegex 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "CTest discovery failed with exit code $LASTEXITCODE."
    }
    $listText = $listOutput -join [Environment]::NewLine
    $matches = [regex]::Matches($listText, '(?m)^\s*Test\s+#\d+:\s+(.+?)\s*$')
    $selectedTests = @($matches | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    if ($selectedTests.Count -eq 0) {
        throw "GPU gate selected zero tests with regex '$GpuTestRegex'."
    }
    $fixturePolicies = @($selectedTests | ForEach-Object { Get-GpuFixturePolicy -TestName $_ })

    $phase = "gpu_execution"
    $previousGpuSmoke = $env:TRACE2D_RUN_GPU_SMOKE
    $env:TRACE2D_RUN_GPU_SMOKE = "1"
    try {
        $gpuOutput = & ctest --preset windows-debug -R $GpuTestRegex --output-on-failure -V 2>&1
        $gpuExitCode = $LASTEXITCODE
        $gpuText = $gpuOutput -join [Environment]::NewLine
        $gpuText | Set-Content -Path $testLogPath -Encoding utf8
        $gpuOutput | ForEach-Object { Write-Host $_ }

        if ($gpuExitCode -ne 0) {
            $failureCategory = "gpu_fixture_failure"
            throw "Real-GPU tests failed with exit code $gpuExitCode."
        }
        if ($gpuText -match '(?im)\*\*\*Skipped|\[\s*SKIPPED\s*\]') {
            $failureCategory = "unsupported_or_skipped_fixture"
            throw "Real-GPU evidence is invalid because at least one selected test was skipped."
        }

        $phase = "environment_probe"
        $environmentMatches = [regex]::Matches(
            $gpuText,
            'TRACE2D_GPUQA_ENV_V1\s+backend=(?<backend>\S+)\s+viewport_width=(?<width>\d+)\s+viewport_height=(?<height>\d+)\s+capture_format=(?<format>\S+)\s+comparison=(?<comparison>\S+)')
        if ($environmentMatches.Count -ne 1) {
            $failureCategory = "environment_probe_failure"
            throw "Expected exactly one TRACE2D_GPUQA_ENV_V1 marker, found $($environmentMatches.Count)."
        }

        $environmentMatch = $environmentMatches[0]
        $gpuEnvironment = [ordered]@{
            sdl_gpu_backend = $environmentMatch.Groups["backend"].Value
            viewport_width = [uint32]$environmentMatch.Groups["width"].Value
            viewport_height = [uint32]$environmentMatch.Groups["height"].Value
            normalized_capture_format = $environmentMatch.Groups["format"].Value
            comparison_contract = $environmentMatch.Groups["comparison"].Value
        }
    }
    finally {
        $env:TRACE2D_RUN_GPU_SMOKE = $previousGpuSmoke
    }

    $status = "passed"
    $phase = "complete"
}
catch {
    $errorMessage = $_.Exception.Message
    if ($null -eq $failureCategory) {
        $failureCategory = Resolve-FailureCategory -Phase $phase
    }
    throw
}
finally {
    try {
        $phaseBeforeEvidence = $phase
        if ($status -ne "passed" -and $null -eq $failureCategory) {
            $failureCategory = Resolve-FailureCategory -Phase $phaseBeforeEvidence
        }

        New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
        $resolvedOutput = (Resolve-Path $OutputDirectory).Path

        $gpuControllers = @()
        try {
            $gpuControllers = @(Get-CimInstance Win32_VideoController | ForEach-Object {
                [ordered]@{ name = $_.Name; driver_version = $_.DriverVersion }
            })
        } catch {
            $gpuControllers = @([ordered]@{ name = "unavailable"; driver_version = "unavailable" })
        }

        $cpuName = "unavailable"
        try { $cpuName = (Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name) } catch {}
        $osInfo = [ordered]@{ caption = "unavailable"; version = "unavailable"; build = "unavailable" }
        try {
            $os = Get-CimInstance Win32_OperatingSystem
            $osInfo = [ordered]@{ caption = $os.Caption; version = $os.Version; build = $os.BuildNumber }
        } catch {}

        $cmakeVersion = "unavailable"
        try { $cmakeVersion = ((& cmake --version | Select-Object -First 1) -as [string]).Trim() } catch {}
        $vcpkgCommit = "unavailable"
        if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {
            try { $vcpkgCommit = (& git -C $env:VCPKG_ROOT rev-parse HEAD).Trim() } catch {}
        }

        $logHash = $null
        if ($testLogPath -and (Test-Path $testLogPath -PathType Leaf)) {
            $logHash = (Get-FileHash -Algorithm SHA256 -Path $testLogPath).Hash.ToLowerInvariant()
        }

        $manifest = [ordered]@{
            schema = "trace2d.gpu-gate.v2"
            generated_utc = [DateTime]::UtcNow.ToString("o")
            gate = [ordered]@{
                status = $status
                phase = $phaseBeforeEvidence
                failure_category = $failureCategory
                error = $errorMessage
            }
            commit = $headSha
            test_regex = $GpuTestRegex
            selected_test_count = $selectedTests.Count
            selected_tests = $selectedTests
            fixture_policies = $fixturePolicies
            runner = [ordered]@{
                name = $env:RUNNER_NAME
                os = $env:RUNNER_OS
                arch = $env:RUNNER_ARCH
                required_custom_label = "trace2d-gpu"
            }
            machine = [ordered]@{
                os = $osInfo
                cpu = $cpuName
                gpu_controllers = $gpuControllers
                renderer_environment = $gpuEnvironment
            }
            toolchain = [ordered]@{
                cmake = $cmakeVersion
                vcpkg_commit = $vcpkgCommit
                configure_preset = "windows-msvc"
                build_preset = "windows-debug"
                test_preset = "windows-debug"
                build_configuration = "Debug"
            }
            support_matrix = [ordered]@{
                schema = $supportMatrixSchema
                path = "config/gpu-qa-support-matrix.json"
                sha256 = $supportMatrixHash
                maintained_tier_b_target = "owner-windows-primary"
                tier_c_claim = "not_established"
            }
            gpu_test_log = [ordered]@{
                path = $(if ($logHash) { "gpu-tests.txt" } else { $null })
                sha256 = $logHash
            }
        }
        $manifest | ConvertTo-Json -Depth 10 | Set-Content -Path (Join-Path $resolvedOutput "manifest.json") -Encoding utf8
    }
    catch {
        if ($status -eq "passed") {
            throw
        }
        Write-Error "GPU gate evidence generation also failed: $($_.Exception.Message)"
    }
    finally {
        Pop-Location
    }
}

Write-Host "Trace2D GPU gate passed for commit $headSha with $($selectedTests.Count) selected test(s) on SDL backend $($gpuEnvironment.sdl_gpu_backend)."
