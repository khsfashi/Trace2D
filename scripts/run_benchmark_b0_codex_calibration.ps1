param(
    [string]$RunsRoot = "",
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$CodexVersion = "0.144.6"
$ModelId = "gpt-5.5"
$ModelRevision = "gpt-5.5-2026-04-23"
$GodotVersion = "4.7.1-stable"
$NodeVersion = "22.18.0"
$GodotMcpPackage = "@satelliteoflove/godot-mcp@4.1.0"
$VcpkgBaseline = "d92484ed3c5020c6679d095ad3e5add907887b62"
$TaskId = "b0-semantic-scene-authoring"
$ProfileRelative = "benchmarks/b0/agent-profile.codex-0.144.6.json"

function Assert-LastExitCode {
    param([string]$Label)
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
}

function Ensure-Directory {
    param([string]$Path)
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

function Download-IfMissing {
    param(
        [string]$Url,
        [string]$Destination
    )
    if (Test-Path -LiteralPath $Destination) {
        return
    }
    Write-Host "Downloading $Url"
    Invoke-WebRequest -Uri $Url -OutFile $Destination
}

function Get-ExpectedChecksum {
    param(
        [string]$ChecksumFile,
        [string]$FileName
    )
    $escaped = [regex]::Escape($FileName)
    $match = Select-String -LiteralPath $ChecksumFile -Pattern "^([0-9a-fA-F]+)\s+\*?$escaped$" | Select-Object -First 1
    if ($null -eq $match) {
        throw "Checksum entry not found for $FileName in $ChecksumFile"
    }
    return $match.Matches[0].Groups[1].Value.ToUpperInvariant()
}

function Assert-Checksum {
    param(
        [string]$Path,
        [ValidateSet("SHA256", "SHA512")]
        [string]$Algorithm,
        [string]$Expected
    )
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm $Algorithm).Hash.ToUpperInvariant()
    if ($actual -ne $Expected.ToUpperInvariant()) {
        throw "$Algorithm mismatch for $Path`nExpected: $Expected`nActual:   $actual"
    }
    return $actual
}

function Scrub-CodexCredentials {
    param([string]$Root)
    if (-not (Test-Path -LiteralPath $Root)) {
        return
    }
    Get-ChildItem -LiteralPath $Root -Recurse -Force -File -Filter "auth.json" -ErrorAction SilentlyContinue |
        ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue }
}

function Copy-ToolDirectory {
    param(
        [string]$Source,
        [string]$Destination
    )
    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }
    Ensure-Directory $Destination
    Copy-Item -Path (Join-Path $Source "*") -Destination $Destination -Recurse -Force
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$profilePath = Join-Path $repoRoot $ProfileRelative
$wrapperPath = Join-Path $repoRoot "scripts/benchmark_b0_codex_wrapper.py"
$harnessPath = Join-Path $repoRoot "scripts/benchmark_b0.py"
$suitePath = Join-Path $repoRoot "benchmarks/b0/suite.json"

foreach ($required in @($profilePath, $wrapperPath, $harnessPath, $suitePath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required PR #118 file is missing: $required`nRun this script from the current PR #118 checkout."
    }
}

$pythonCommand = Get-Command python -ErrorAction Stop
$codexCommand = Get-Command codex -ErrorAction Stop
$gitCommand = Get-Command git -ErrorAction Stop
$cmakeCommand = Get-Command cmake -ErrorAction Stop

$codexVersionText = (& $codexCommand.Source --version | Out-String).Trim()
Assert-LastExitCode "codex --version"
if ($codexVersionText -ne "codex-cli $CodexVersion") {
    throw "Codex version mismatch. Expected codex-cli $CodexVersion, got: $codexVersionText"
}

& $codexCommand.Source login status
Assert-LastExitCode "codex login status"

$defaultAuth = Join-Path $HOME ".codex/auth.json"
if (-not (Test-Path -LiteralPath $defaultAuth -PathType Leaf)) {
    throw @"
Codex is logged in, but ~/.codex/auth.json is not available for the isolated benchmark CODEX_HOME.
The benchmark never commits or uploads this credential. Re-login with file-backed CLI credentials, then rerun:

  codex -c 'cli_auth_credentials_store="file"' login
"@
}
$env:TRACE2D_BENCH_CODEX_AUTH_FILE = (Resolve-Path $defaultAuth).Path

$head = (& $gitCommand.Source -C $repoRoot rev-parse HEAD | Out-String).Trim()
Assert-LastExitCode "git rev-parse HEAD"
$profileSha256 = (Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash.ToLowerInvariant()

$localBase = Join-Path $env:LOCALAPPDATA "Trace2D/b0"
$toolsRoot = Join-Path $localBase "tools"
if ([string]::IsNullOrWhiteSpace($RunsRoot)) {
    $RunsRoot = Join-Path $localBase "runs"
}
Ensure-Directory $toolsRoot
Ensure-Directory $RunsRoot

$runId = "codex-calibration-" + (Get-Date -Format "yyyyMMdd-HHmmss") + "-" + ([guid]::NewGuid().ToString("N").Substring(0, 8))
$runRoot = Join-Path $RunsRoot $runId
$calibrationRoot = Join-Path $runRoot "calibration"
$probeRoot = Join-Path $runRoot "isolation-probe"
Ensure-Directory $calibrationRoot
Ensure-Directory $probeRoot
$zipPath = "$runRoot.zip"
$canaryPath = Join-Path $repoRoot ("benchmarks/b0/verifiers/.codex-isolation-canary-" + [guid]::NewGuid().ToString("N") + ".txt")
$completedSuccessfully = $false

try {
    Write-Host "== Trace2D B0 Codex calibration =="
    Write-Host "Repository: $repoRoot"
    Write-Host "Repository head: $head"
    Write-Host "Codex: $codexVersionText"
    Write-Host "Model snapshot requested: $ModelRevision"
    Write-Host "Run root (outside repository): $runRoot"

    # Freeze/build the Trace2D execution surface before the model is started.
    $vcpkgRoot = Join-Path $toolsRoot "vcpkg-$VcpkgBaseline"
    if (-not (Test-Path -LiteralPath (Join-Path $vcpkgRoot ".git"))) {
        Write-Host "Cloning pinned vcpkg..."
        & $gitCommand.Source clone https://github.com/microsoft/vcpkg $vcpkgRoot
        Assert-LastExitCode "vcpkg clone"
    }
    & $gitCommand.Source -C $vcpkgRoot fetch --all --tags --prune
    Assert-LastExitCode "vcpkg fetch"
    & $gitCommand.Source -C $vcpkgRoot checkout --force $VcpkgBaseline
    Assert-LastExitCode "vcpkg checkout"
    if (-not (Test-Path -LiteralPath (Join-Path $vcpkgRoot "vcpkg.exe"))) {
        & (Join-Path $vcpkgRoot "bootstrap-vcpkg.bat") -disableMetrics
        Assert-LastExitCode "vcpkg bootstrap"
    }
    $env:VCPKG_ROOT = $vcpkgRoot

    if (-not $SkipBuild) {
        Push-Location $repoRoot
        try {
            & $cmakeCommand.Source --preset windows-msvc
            Assert-LastExitCode "Trace2D configure"
            & $cmakeCommand.Source --build --preset windows-debug --parallel
            Assert-LastExitCode "Trace2D build"
            & ctest --preset windows-debug
            Assert-LastExitCode "Trace2D test"
        }
        finally {
            Pop-Location
        }
    }

    $builtTrace2d = Join-Path $repoRoot "build/windows-msvc/tools/trace2d/Debug/trace2d.exe"
    $builtTrace2dMcp = Join-Path $repoRoot "build/windows-msvc/tools/mcp/Debug/trace2d_mcp.exe"
    if (-not (Test-Path -LiteralPath $builtTrace2d -PathType Leaf)) {
        throw "Trace2D CLI not found after build: $builtTrace2d"
    }
    if (-not (Test-Path -LiteralPath $builtTrace2dMcp -PathType Leaf)) {
        throw "Trace2D MCP host not found after build: $builtTrace2dMcp"
    }

    # Copy only the public runtime tool surfaces outside the repository. The model
    # never receives a read root covering the source tree or held-out verifier.
    $trace2dToolRoot = Join-Path $toolsRoot "trace2d-$head"
    $trace2dCliRoot = Join-Path $trace2dToolRoot "cli"
    $trace2dMcpRoot = Join-Path $trace2dToolRoot "mcp"
    Copy-ToolDirectory (Split-Path $builtTrace2d -Parent) $trace2dCliRoot
    Copy-ToolDirectory (Split-Path $builtTrace2dMcp -Parent) $trace2dMcpRoot
    $trace2dBin = Join-Path $trace2dCliRoot "trace2d.exe"
    $trace2dMcpBin = Join-Path $trace2dMcpRoot "trace2d_mcp.exe"

    # Official Godot 4.7.1 Windows build + release SHA-512.
    $godotRoot = Join-Path $toolsRoot "godot-$GodotVersion"
    Ensure-Directory $godotRoot
    $godotArchiveName = "Godot_v${GodotVersion}_win64.exe.zip"
    $godotArchive = Join-Path $godotRoot $godotArchiveName
    $godotSums = Join-Path $godotRoot "SHA512-SUMS.txt"
    $godotRelease = "https://github.com/godotengine/godot-builds/releases/download/$GodotVersion"
    Download-IfMissing "$godotRelease/$godotArchiveName" $godotArchive
    Download-IfMissing "$godotRelease/SHA512-SUMS.txt" $godotSums
    $godotExpected = Get-ExpectedChecksum $godotSums $godotArchiveName
    $godotSha512 = Assert-Checksum $godotArchive "SHA512" $godotExpected
    $godotExtract = Join-Path $godotRoot "extracted"
    if (-not (Test-Path -LiteralPath (Join-Path $godotExtract "Godot_v${GodotVersion}_win64.exe"))) {
        if (Test-Path -LiteralPath $godotExtract) { Remove-Item $godotExtract -Recurse -Force }
        Expand-Archive -LiteralPath $godotArchive -DestinationPath $godotExtract -Force
    }
    $godotBin = Join-Path $godotExtract "Godot_v${GodotVersion}_win64.exe"
    $godotReported = (& $godotBin --version | Out-String).Trim()
    Assert-LastExitCode "Godot version"
    if ($godotReported -notmatch '^4\.7\.1\.stable') {
        throw "Pinned Godot reported unexpected version: $godotReported"
    }

    # Official Node 22.18.0 Windows archive + SHA-256.
    $nodeRoot = Join-Path $toolsRoot "node-v$NodeVersion"
    Ensure-Directory $nodeRoot
    $nodeArchiveName = "node-v${NodeVersion}-win-x64.zip"
    $nodeArchive = Join-Path $nodeRoot $nodeArchiveName
    $nodeSums = Join-Path $nodeRoot "SHASUMS256.txt"
    $nodeRelease = "https://nodejs.org/dist/v$NodeVersion"
    Download-IfMissing "$nodeRelease/$nodeArchiveName" $nodeArchive
    Download-IfMissing "$nodeRelease/SHASUMS256.txt" $nodeSums
    $nodeExpected = Get-ExpectedChecksum $nodeSums $nodeArchiveName
    $nodeSha256 = Assert-Checksum $nodeArchive "SHA256" $nodeExpected
    $nodeExtractParent = Join-Path $nodeRoot "extracted"
    $nodeDir = Join-Path $nodeExtractParent "node-v${NodeVersion}-win-x64"
    if (-not (Test-Path -LiteralPath (Join-Path $nodeDir "node.exe"))) {
        if (Test-Path -LiteralPath $nodeExtractParent) { Remove-Item $nodeExtractParent -Recurse -Force }
        Expand-Archive -LiteralPath $nodeArchive -DestinationPath $nodeExtractParent -Force
    }
    $nodeExe = Join-Path $nodeDir "node.exe"
    $npmCmd = Join-Path $nodeDir "npm.cmd"
    $nodeReported = (& $nodeExe --version | Out-String).Trim()
    Assert-LastExitCode "Node version"
    if ($nodeReported -ne "v$NodeVersion") {
        throw "Pinned Node reported unexpected version: $nodeReported"
    }

    # Exact Godot MCP package installed using the pinned Node/npm toolchain.
    $mcpRoot = Join-Path $toolsRoot "godot-mcp-4.1.0-node-$NodeVersion"
    $previousPath = $env:PATH
    $env:PATH = "$nodeDir;$previousPath"
    try {
        Ensure-Directory $mcpRoot
        & $npmCmd install --prefix $mcpRoot --no-audit --no-fund $GodotMcpPackage
        Assert-LastExitCode "Godot MCP npm install"
        $mcpServer = Join-Path $mcpRoot "node_modules/.bin/godot-mcp.cmd"
        if (-not (Test-Path -LiteralPath $mcpServer -PathType Leaf)) {
            throw "Godot MCP server not found: $mcpServer"
        }
        $mcpVersion = (& $mcpServer --version | Out-String).Trim()
        Assert-LastExitCode "Godot MCP version"
        if ($mcpVersion -ne "4.1.0") {
            throw "Godot MCP version mismatch: $mcpVersion"
        }
        $mcpIntegrity = (& $npmCmd view $GodotMcpPackage dist.integrity | Out-String).Trim()
        Assert-LastExitCode "Godot MCP npm integrity"
        if ([string]::IsNullOrWhiteSpace($mcpIntegrity)) {
            throw "npm returned no integrity for $GodotMcpPackage"
        }
    }
    finally {
        $env:PATH = $previousPath
    }

    # Stable generic command name without changing the official Godot binary.
    $shimRoot = Join-Path $toolsRoot "command-shims-$GodotVersion"
    Ensure-Directory $shimRoot
    $godotShim = Join-Path $shimRoot "godot.cmd"
    "@echo off`r`n`"$godotBin`" %*`r`n" | Set-Content -LiteralPath $godotShim -Encoding Ascii

    # The harness profile is committed and therefore has one stable hash across lanes.
    $env:PYTHONPATH = (Join-Path $repoRoot "scripts")
    $env:TRACE2D_BENCH_GODOT_BIN = $godotBin
    $env:TRACE2D_BENCH_GODOT_MCP_SERVER = $mcpServer
    $env:TRACE2D_BENCH_TRACE2D_BIN = $trace2dBin
    $env:TRACE2D_BENCH_TRACE2D_MCP_BIN = $trace2dMcpBin
    $env:TRACE2D_BENCH_CODEX_READ_ROOTS = "$nodeDir$([IO.Path]::PathSeparator)$shimRoot"
    $env:TRACE2D_BENCH_NODE_VERSION = "v$NodeVersion"
    $env:TRACE2D_BENCH_WRAPPER_TIMEOUT = "285"

    $toolchain = [ordered]@{
        schema_version = 1
        kind = "trace2d_b0_codex_local_calibration_toolchain"
        repository_head = $head
        profile_path = $ProfileRelative
        profile_sha256 = $profileSha256
        codex = [ordered]@{
            version = $CodexVersion
            model_id = $ModelId
            model_revision = $ModelRevision
            auth = "chatgpt-managed-local"
        }
        godot = [ordered]@{
            version = $GodotVersion
            reported_version = $godotReported
            archive_sha512 = $godotSha512.ToLowerInvariant()
        }
        node = [ordered]@{
            version = $NodeVersion
            archive_sha256 = $nodeSha256.ToLowerInvariant()
        }
        godot_mcp = [ordered]@{
            package = $GodotMcpPackage
            version = $mcpVersion
            npm_integrity = $mcpIntegrity
        }
        vcpkg_baseline = $VcpkgBaseline
        generated_at = (Get-Date).ToUniversalTime().ToString("o")
    }
    $toolchain | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $runRoot "toolchain.json") -Encoding utf8

    # Empirical isolation gate. The canary is deliberately placed beside the
    # held-out verifier, outside the model's workspace and every explicit read root.
    $canarySecret = "TRACE2D-B0-DENY-" + [guid]::NewGuid().ToString("N")
    Set-Content -LiteralPath $canaryPath -Value $canarySecret -Encoding utf8 -NoNewline
    $probeWorkspace = Join-Path $probeRoot "workspace"
    Ensure-Directory $probeWorkspace
    $probeEvidence = Join-Path $probeRoot "isolation.json"
    Write-Host "Running Codex filesystem-isolation probe..."
    & $pythonCommand.Source -m benchmark_b0_codex_wrapper probe-isolation `
        --workspace $probeWorkspace `
        --canary $canaryPath `
        --evidence $probeEvidence
    Assert-LastExitCode "Codex isolation probe"
    Scrub-CodexCredentials $probeRoot

    # Only unscored calibration runs are allowed on this PR state. The suite/task
    # remain qualification_required/qualification_candidate until this evidence is reviewed.
    foreach ($lane in @("godot.generic", "godot.agent", "trace2d.agent")) {
        if ($lane.StartsWith("godot.")) {
            $env:TRACE2D_BENCH_ENGINE_VERSION = $GodotVersion
        }
        else {
            $env:TRACE2D_BENCH_ENGINE_VERSION = "trace2d@$head"
        }
        Write-Host "Running unscored calibration: $lane"
        & $pythonCommand.Source $harnessPath run-trial `
            --task $TaskId `
            --lane $lane `
            --agent-profile $profilePath `
            --runs-root $calibrationRoot
        Assert-LastExitCode "B0 calibration $lane"
        Scrub-CodexCredentials $calibrationRoot
    }

    Write-Host "Generating unscored calibration report..."
    $rawRecords = Join-Path $calibrationRoot "raw.jsonl"
    $reportPath = Join-Path $runRoot "calibration-report.json"
    & $pythonCommand.Source $harnessPath report --records $rawRecords --include-unscored |
        Set-Content -LiteralPath $reportPath -Encoding utf8
    Assert-LastExitCode "B0 calibration report"

    # Preserve a human-readable concise summary without reconstructing core metrics.
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    if (-not $report.integrity.same_agent_profile_per_task) {
        throw "Calibration mixed agent-profile hashes across lanes."
    }
    if ($report.record_count -ne 3) {
        throw "Expected exactly 3 calibration records, got $($report.record_count)."
    }

    $completedSuccessfully = $true
}
finally {
    if (Test-Path -LiteralPath $canaryPath) {
        Remove-Item -LiteralPath $canaryPath -Force -ErrorAction SilentlyContinue
    }
    Scrub-CodexCredentials $runRoot

    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    if (Test-Path -LiteralPath $runRoot) {
        Compress-Archive -Path (Join-Path $runRoot "*") -DestinationPath $zipPath -CompressionLevel Optimal -Force
        Write-Host "Evidence ZIP: $zipPath"
    }
}

if (-not $completedSuccessfully) {
    throw "B0 Codex calibration did not complete. Upload the evidence ZIP printed above so the failure can be classified without guessing."
}

Write-Host ""
Write-Host "B0 Codex calibration completed."
Write-Host "Isolation passed and exactly one unscored trial was preserved for each of the three lanes."
Write-Host "Do not run scored trials yet. Upload the evidence ZIP so PR #118 can review the real model/isolation facts before eligibility is promoted."
Write-Host "Evidence ZIP: $zipPath"
