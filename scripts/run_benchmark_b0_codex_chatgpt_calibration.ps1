param(
    [string]$RunsRoot = "",
    [string]$PreviousRunRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$CodexVersion = "0.144.6"
$ModelId = "gpt-5.6-sol"
$ModelRevision = "gpt-5.6-sol"
$ProviderRevisionPolicy = "chatgpt_managed_identifier_no_dated_snapshot"
$GodotVersion = "4.7.1-stable"
$NodeVersion = "22.18.0"
$TaskId = "b0-semantic-scene-authoring"
$ProfileRelative = "benchmarks/b0/agent-profile.codex-0.144.6.json"

function Assert-LastExitCode {
    param([string]$Label)
    if ($LASTEXITCODE -ne 0) { throw "$Label failed with exit code $LASTEXITCODE" }
}

function Ensure-Directory {
    param([string]$Path)
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

function Require-File {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "$Label not found: $Path" }
    return (Resolve-Path $Path).Path
}

function Copy-ToolDirectory {
    param([string]$Source, [string]$Destination)
    if (Test-Path -LiteralPath $Destination) { Remove-Item -LiteralPath $Destination -Recurse -Force }
    Ensure-Directory $Destination
    Copy-Item -Path (Join-Path $Source "*") -Destination $Destination -Recurse -Force
}

function Scrub-CodexCredentials {
    param([string]$Root)
    if (-not (Test-Path -LiteralPath $Root)) { return }
    Get-ChildItem -LiteralPath $Root -Recurse -Force -File -Filter "auth.json" -ErrorAction SilentlyContinue |
        ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$profilePath = Require-File (Join-Path $repoRoot $ProfileRelative) "B0 Codex profile"
$harnessPath = Require-File (Join-Path $repoRoot "scripts/benchmark_b0.py") "B0 harness"
$packagerPath = Require-File (Join-Path $repoRoot "scripts/package_benchmark_b0_evidence.py") "B0 evidence packager"
$suitePath = Require-File (Join-Path $repoRoot "benchmarks/b0/suite.json") "B0 suite"
$chatgptWrapperPath = Require-File (Join-Path $repoRoot "scripts/benchmark_b0_codex_chatgpt_wrapper.py") "ChatGPT Codex wrapper"

$pythonCommand = Get-Command python -ErrorAction Stop
$codexCommand = Get-Command codex -ErrorAction Stop
$gitCommand = Get-Command git -ErrorAction Stop

$codexVersionText = (& $codexCommand.Source --version | Out-String).Trim()
Assert-LastExitCode "codex --version"
if ($codexVersionText -ne "codex-cli $CodexVersion") {
    throw "Codex version mismatch. Expected codex-cli $CodexVersion, got: $codexVersionText"
}
& $codexCommand.Source login status
Assert-LastExitCode "codex login status"

$defaultAuth = Join-Path $HOME ".codex/auth.json"
if (-not (Test-Path -LiteralPath $defaultAuth -PathType Leaf)) {
    throw "~/.codex/auth.json is required for the isolated CODEX_HOME. Run: codex -c 'cli_auth_credentials_store=\"file\"' login"
}
$env:TRACE2D_BENCH_CODEX_AUTH_FILE = (Resolve-Path $defaultAuth).Path

$localBase = Join-Path $env:LOCALAPPDATA "Trace2D/b0"
$toolsRoot = Join-Path $localBase "tools"
if ([string]::IsNullOrWhiteSpace($RunsRoot)) { $RunsRoot = Join-Path $localBase "runs" }
Ensure-Directory $RunsRoot

if ([string]::IsNullOrWhiteSpace($PreviousRunRoot)) {
    $previous = Get-ChildItem -LiteralPath $RunsRoot -Directory -Filter "codex-calibration-*" -ErrorAction Stop |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "toolchain.json") -PathType Leaf } |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if ($null -eq $previous) { throw "No previous B0 calibration toolchain was found under $RunsRoot" }
    $PreviousRunRoot = $previous.FullName
}
$previousToolchainPath = Require-File (Join-Path $PreviousRunRoot "toolchain.json") "previous calibration toolchain"
$previousToolchain = Get-Content -LiteralPath $previousToolchainPath -Raw | ConvertFrom-Json

$suite = Get-Content -LiteralPath $suitePath -Raw | ConvertFrom-Json
$frozenTrace2dCommit = [string]$suite.frozen_source.trace2d_commit
if ([string]$previousToolchain.frozen_trace2d_commit -ne $frozenTrace2dCommit) {
    throw "Previous calibration used a different frozen Trace2D source."
}
if ([string]$previousToolchain.codex.version -ne $CodexVersion) { throw "Previous calibration used a different Codex version." }
if ([string]$previousToolchain.godot.version -ne $GodotVersion) { throw "Previous calibration used a different Godot version." }
if ([string]$previousToolchain.node.version -ne $NodeVersion) { throw "Previous calibration used a different Node version." }
if ([string]$previousToolchain.godot_mcp.version -ne "4.1.0") { throw "Previous calibration used a different Godot MCP version." }

$head = (& $gitCommand.Source -C $repoRoot rev-parse HEAD | Out-String).Trim()
Assert-LastExitCode "git rev-parse HEAD"
$profileSha256 = (Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash.ToLowerInvariant()
$previousToolchainSha256 = (Get-FileHash -LiteralPath $previousToolchainPath -Algorithm SHA256).Hash.ToLowerInvariant()

$builtTrace2d = Require-File (Join-Path $repoRoot "build/windows-msvc/tools/trace2d/Debug/trace2d.exe") "previously built Trace2D CLI"
$builtTrace2dMcp = Require-File (Join-Path $repoRoot "build/windows-msvc/tools/mcp/Debug/trace2d_mcp.exe") "previously built Trace2D MCP host"
$trace2dToolRoot = Join-Path $toolsRoot "trace2d-$frozenTrace2dCommit"
$trace2dCliRoot = Join-Path $trace2dToolRoot "cli"
$trace2dMcpRoot = Join-Path $trace2dToolRoot "mcp"
Copy-ToolDirectory (Split-Path $builtTrace2d -Parent) $trace2dCliRoot
Copy-ToolDirectory (Split-Path $builtTrace2dMcp -Parent) $trace2dMcpRoot
$trace2dBin = Require-File (Join-Path $trace2dCliRoot "trace2d.exe") "frozen Trace2D CLI copy"
$trace2dMcpBin = Require-File (Join-Path $trace2dMcpRoot "trace2d_mcp.exe") "frozen Trace2D MCP copy"

$godotRoot = Join-Path $toolsRoot "godot-$GodotVersion"
$godotArchive = Require-File (Join-Path $godotRoot "Godot_v${GodotVersion}_win64.exe.zip") "cached Godot archive"
$godotBin = Require-File (Join-Path $godotRoot "extracted/Godot_v${GodotVersion}_win64.exe") "cached Godot executable"
$godotSha512 = (Get-FileHash -LiteralPath $godotArchive -Algorithm SHA512).Hash.ToLowerInvariant()
if ($godotSha512 -ne ([string]$previousToolchain.godot.archive_sha512).ToLowerInvariant()) { throw "Cached Godot archive no longer matches the qualified calibration evidence." }
$godotReported = (& $godotBin --version | Out-String).Trim()
Assert-LastExitCode "Godot version"
if ($godotReported -notmatch '^4\.7\.1\.stable') { throw "Unexpected Godot version: $godotReported" }

$nodeRoot = Join-Path $toolsRoot "node-v$NodeVersion"
$nodeArchive = Require-File (Join-Path $nodeRoot "node-v${NodeVersion}-win-x64.zip") "cached Node archive"
$nodeDir = Join-Path $nodeRoot "extracted/node-v${NodeVersion}-win-x64"
$nodeExe = Require-File (Join-Path $nodeDir "node.exe") "cached Node executable"
$nodeSha256 = (Get-FileHash -LiteralPath $nodeArchive -Algorithm SHA256).Hash.ToLowerInvariant()
if ($nodeSha256 -ne ([string]$previousToolchain.node.archive_sha256).ToLowerInvariant()) { throw "Cached Node archive no longer matches the qualified calibration evidence." }
$nodeReported = (& $nodeExe --version | Out-String).Trim()
Assert-LastExitCode "Node version"
if ($nodeReported -ne "v$NodeVersion") { throw "Unexpected Node version: $nodeReported" }

$mcpRoot = Join-Path $toolsRoot "godot-mcp-4.1.0-node-$NodeVersion"
$mcpServer = Require-File (Join-Path $mcpRoot "node_modules/.bin/godot-mcp.cmd") "cached Godot MCP server"
$previousPath = $env:PATH
$env:PATH = "$nodeDir;$previousPath"
try {
    $mcpVersion = (& $mcpServer --version | Out-String).Trim()
    Assert-LastExitCode "Godot MCP version"
}
finally { $env:PATH = $previousPath }
if ($mcpVersion -ne "4.1.0") { throw "Unexpected Godot MCP version: $mcpVersion" }

$shimRoot = Join-Path $toolsRoot "command-shims-$GodotVersion"
Ensure-Directory $shimRoot
$godotShim = Join-Path $shimRoot "godot.cmd"
"@echo off`r`n`"$godotBin`" %*`r`n" | Set-Content -LiteralPath $godotShim -Encoding Ascii

$env:PYTHONPATH = (Join-Path $repoRoot "scripts")
$env:TRACE2D_BENCH_GODOT_BIN = $godotBin
$env:TRACE2D_BENCH_GODOT_MCP_SERVER = $mcpServer
$env:TRACE2D_BENCH_TRACE2D_BIN = $trace2dBin
$env:TRACE2D_BENCH_TRACE2D_MCP_BIN = $trace2dMcpBin
$env:TRACE2D_BENCH_CODEX_READ_ROOTS = "$nodeDir$([IO.Path]::PathSeparator)$shimRoot"
$env:TRACE2D_BENCH_NODE_VERSION = "v$NodeVersion"
$env:TRACE2D_BENCH_WRAPPER_TIMEOUT = "285"

$runId = "codex-chatgpt-calibration-" + (Get-Date -Format "yyyyMMdd-HHmmss") + "-" + ([guid]::NewGuid().ToString("N").Substring(0, 8))
$runRoot = Join-Path $RunsRoot $runId
$calibrationRoot = Join-Path $runRoot "calibration"
$probeRoot = Join-Path $runRoot "isolation-probe"
$zipPath = "$runRoot.zip"
$canaryPath = Join-Path $repoRoot ("benchmarks/b0/verifiers/.codex-isolation-canary-" + [guid]::NewGuid().ToString("N") + ".txt")
Ensure-Directory $calibrationRoot
Ensure-Directory $probeRoot
$completedSuccessfully = $false
$packagingError = $null

try {
    $toolchain = [ordered]@{
        schema_version = 1
        kind = "trace2d_b0_codex_chatgpt_calibration_toolchain"
        repository_head = $head
        frozen_trace2d_commit = $frozenTrace2dCommit
        profile_path = $ProfileRelative
        profile_sha256 = $profileSha256
        reused_previous_toolchain_sha256 = $previousToolchainSha256
        codex = [ordered]@{
            version = $CodexVersion
            model_id = $ModelId
            model_revision = $ModelRevision
            provider_revision_policy = $ProviderRevisionPolicy
            auth = "chatgpt-managed-local"
        }
        godot = [ordered]@{ version = $GodotVersion; reported_version = $godotReported; archive_sha512 = $godotSha512 }
        node = [ordered]@{ version = $NodeVersion; archive_sha256 = $nodeSha256 }
        godot_mcp = [ordered]@{ package = "@satelliteoflove/godot-mcp@4.1.0"; version = $mcpVersion; npm_integrity = [string]$previousToolchain.godot_mcp.npm_integrity }
        generated_at = (Get-Date).ToUniversalTime().ToString("o")
    }
    $toolchain | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $runRoot "toolchain.json") -Encoding utf8

    Write-Host "Running ChatGPT Codex model/isolation probe with $ModelId..."
    $canarySecret = "TRACE2D-B0-DENY-" + [guid]::NewGuid().ToString("N")
    Set-Content -LiteralPath $canaryPath -Value $canarySecret -Encoding utf8 -NoNewline
    $probeWorkspace = Join-Path $probeRoot "workspace"
    Ensure-Directory $probeWorkspace
    $probeEvidence = Join-Path $probeRoot "isolation.json"
    & $pythonCommand.Source -m benchmark_b0_codex_chatgpt_wrapper probe-isolation --workspace $probeWorkspace --canary $canaryPath --evidence $probeEvidence
    Assert-LastExitCode "Codex ChatGPT isolation probe"
    Scrub-CodexCredentials $probeRoot

    $trialExitCodes = [ordered]@{}
    foreach ($lane in @("godot.generic", "godot.agent", "trace2d.agent")) {
        if ($lane.StartsWith("godot.")) { $env:TRACE2D_BENCH_ENGINE_VERSION = $GodotVersion }
        else { $env:TRACE2D_BENCH_ENGINE_VERSION = "trace2d@$frozenTrace2dCommit" }
        Write-Host "Running unscored ChatGPT Codex calibration: $lane"
        & $pythonCommand.Source $harnessPath run-trial --task $TaskId --lane $lane --agent-profile $profilePath --runs-root $calibrationRoot
        $trialExitCodes[$lane] = $LASTEXITCODE
        Scrub-CodexCredentials $calibrationRoot
        if ($LASTEXITCODE -ne 0) { Write-Host "Calibration $lane returned $LASTEXITCODE; preserving it and continuing." }
    }
    $trialExitCodes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runRoot "calibration-exit-codes.json") -Encoding utf8

    $rawRecords = Join-Path $calibrationRoot "raw.jsonl"
    if (-not (Test-Path -LiteralPath $rawRecords -PathType Leaf)) { throw "Calibration produced no raw records." }
    $reportPath = Join-Path $runRoot "calibration-report.json"
    & $pythonCommand.Source $harnessPath report --records $rawRecords --include-unscored | Set-Content -LiteralPath $reportPath -Encoding utf8
    Assert-LastExitCode "B0 calibration report"
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    if (-not $report.integrity.same_agent_profile_per_task) { throw "Calibration mixed Agent profile hashes." }
    if ($report.record_count -ne 3) { throw "Expected exactly three preserved lane records, got $($report.record_count)." }
    $completedSuccessfully = $true
}
finally {
    if (Test-Path -LiteralPath $canaryPath) { Remove-Item -LiteralPath $canaryPath -Force -ErrorAction SilentlyContinue }
    Scrub-CodexCredentials $runRoot
    if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
    if (Test-Path -LiteralPath $runRoot) {
        try {
            & $pythonCommand.Source $packagerPath --run-root $runRoot --output $zipPath
            if ($LASTEXITCODE -ne 0) { throw "scrubbed evidence packager returned exit code $LASTEXITCODE" }
            Write-Host "Evidence ZIP: $zipPath"
        }
        catch {
            $packagingError = $_.Exception.Message
            Write-Warning "Evidence ZIP packaging failed: $packagingError"
        }
    }
}

if ($null -ne $packagingError) { throw "B0 evidence ZIP packaging failed: $packagingError" }
if (-not $completedSuccessfully) { throw "B0 ChatGPT Codex calibration did not complete. Upload the generated evidence ZIP for classification." }

Write-Host "B0 ChatGPT Codex calibration completed."
Write-Host "Upload only the scrubbed evidence ZIP; do not run scored trials yet."
Write-Host "Evidence ZIP: $zipPath"
