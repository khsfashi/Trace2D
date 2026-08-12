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

$repositoryRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repositoryRoot
$status = "failed"
$errorMessage = $null
$selectedTests = @()
$headSha = "unavailable"
$testLogPath = $null
try {
    if (-not $IsWindows) {
        throw "The real-GPU gate requires Windows."
    }
    if ([string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {
        throw "VCPKG_ROOT is not configured."
    }
    $toolchain = Join-Path $env:VCPKG_ROOT "scripts/buildsystems/vcpkg.cmake"
    if (-not (Test-Path $toolchain -PathType Leaf)) {
        throw "vcpkg toolchain not found under VCPKG_ROOT."
    }

    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $resolvedOutput = (Resolve-Path $OutputDirectory).Path
    $testLogPath = Join-Path $resolvedOutput "gpu-tests.txt"

    $headSha = (& git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($headSha)) {
        throw "Unable to resolve current Trace2D commit."
    }

    Invoke-NativeChecked "CMake configure" { cmake --preset windows-msvc }
    Invoke-NativeChecked "Debug build" { cmake --build --preset windows-debug --parallel }

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

    $previousGpuSmoke = $env:TRACE2D_RUN_GPU_SMOKE
    $env:TRACE2D_RUN_GPU_SMOKE = "1"
    try {
        $gpuOutput = & ctest --preset windows-debug -R $GpuTestRegex --output-on-failure -V 2>&1
        $gpuExitCode = $LASTEXITCODE
        $gpuText = $gpuOutput -join [Environment]::NewLine
        $gpuText | Set-Content -Path $testLogPath -Encoding utf8
        $gpuOutput | ForEach-Object { Write-Host $_ }

        if ($gpuExitCode -ne 0) {
            throw "Real-GPU tests failed with exit code $gpuExitCode."
        }
        if ($gpuText -match '(?im)\*\*\*Skipped|\[\s*SKIPPED\s*\]') {
            throw "Real-GPU evidence is invalid because at least one selected test was skipped."
        }
    }
    finally {
        $env:TRACE2D_RUN_GPU_SMOKE = $previousGpuSmoke
    }

    $status = "passed"
}
catch {
    $errorMessage = $_.Exception.Message
    throw
}
finally {
    try {
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
        try {
            $vcpkgCommit = (& git -C $env:VCPKG_ROOT rev-parse HEAD).Trim()
        } catch {}

        $logHash = $null
        if ($testLogPath -and (Test-Path $testLogPath -PathType Leaf)) {
            $logHash = (Get-FileHash -Algorithm SHA256 -Path $testLogPath).Hash.ToLowerInvariant()
        }

        $manifest = [ordered]@{
            schema = "trace2d.gpu-gate.v1"
            generated_utc = [DateTime]::UtcNow.ToString("o")
            status = $status
            error = $errorMessage
            commit = $headSha
            test_regex = $GpuTestRegex
            selected_test_count = $selectedTests.Count
            selected_tests = $selectedTests
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
            }
            toolchain = [ordered]@{
                cmake = $cmakeVersion
                vcpkg_commit = $vcpkgCommit
                configure_preset = "windows-msvc"
                build_preset = "windows-debug"
                test_preset = "windows-debug"
            }
            gpu_test_log = [ordered]@{
                path = $(if ($logHash) { "gpu-tests.txt" } else { $null })
                sha256 = $logHash
            }
        }
        $manifest | ConvertTo-Json -Depth 8 | Set-Content -Path (Join-Path $resolvedOutput "manifest.json") -Encoding utf8
    }
    finally {
        Pop-Location
    }
}

Write-Host "Trace2D GPU gate passed for commit $headSha with $($selectedTests.Count) selected test(s)."
