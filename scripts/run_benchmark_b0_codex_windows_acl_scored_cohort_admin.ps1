[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Fail-B0 {
    param([string]$Message, [int]$Code = 2)
    Write-Error $Message
    exit $Code
}

if ($env:OS -ne "Windows_NT") {
    Fail-B0 "B0 scored cohort owner-local execution requires native Windows."
}

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Fail-B0 @"
This launcher must run from an Administrator PowerShell.
Close this shell, open PowerShell with 'Run as administrator', then run:
  cd D:\Trace2D-pr118
  .\scripts\run_benchmark_b0_codex_windows_acl_scored_cohort_admin.ps1
The elevation is host orchestration only; the frozen Codex sandbox/profile/budget remain unchanged.
"@
}

if (-not $env:LOCALAPPDATA) {
    Fail-B0 "LOCALAPPDATA is required on the owner-local Windows host."
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$runsRoot = Join-Path $env:LOCALAPPDATA "Trace2D\b0\runs"
New-Item -ItemType Directory -Force -Path $runsRoot | Out-Null

# Prove that the elevated host can create/read/remove evidence before any model
# call or scored slot starts. This prevents a later packager error from hiding a
# basic owner-local filesystem permission problem.
$probePath = Join-Path $runsRoot (".owner-host-write-probe-{0}.txt" -f [Guid]::NewGuid().ToString("N"))
$probeToken = "TRACE2D_B0_OWNER_HOST_WRITE_OK"
try {
    [IO.File]::WriteAllText($probePath, $probeToken, [Text.UTF8Encoding]::new($false))
    $roundTrip = [IO.File]::ReadAllText($probePath, [Text.Encoding]::UTF8)
    if ($roundTrip -ne $probeToken) {
        Fail-B0 "Owner-local evidence root write/read preflight did not round-trip correctly: $runsRoot"
    }
}
catch {
    Fail-B0 "Owner-local evidence root is not writable even from Administrator PowerShell: $runsRoot`n$($_.Exception.Message)"
}
finally {
    Remove-Item -Force -ErrorAction SilentlyContinue $probePath
}

# Never create an unofficial replacement sample. If any earlier scored launcher
# reached even one raw scored record, stop and preserve that cohort for review.
$existingScored = @()
Get-ChildItem -Path $runsRoot -Directory -Filter "codex-chatgpt-scored-*" -ErrorAction SilentlyContinue | ForEach-Object {
    $raw = Join-Path $_.FullName "scored\raw.jsonl"
    if (Test-Path -LiteralPath $raw -PathType Leaf) {
        $count = @(
            Get-Content -LiteralPath $raw -ErrorAction Stop |
                Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
        ).Count
        if ($count -gt 0) {
            $existingScored += [pscustomobject]@{
                RunRoot = $_.FullName
                RawRecords = $count
            }
        }
    }
}

if ($existingScored.Count -gt 0) {
    $details = ($existingScored | Format-Table -AutoSize | Out-String).TrimEnd()
    Fail-B0 @"
Existing scored B0 raw records were found. Do NOT rerun the cohort or replace those samples.
Preserve/upload the existing run for review instead:
$details
"@ 3
}

$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    Fail-B0 "python was not found on PATH."
}

Write-Host "[B0] Administrator host confirmed."
Write-Host "[B0] Evidence root write/read preflight passed: $runsRoot"
Write-Host "[B0] No prior scored raw record exists; preregistered nine-slot cohort may start."
Write-Host "[B0] The runner will still re-prove the distinct Codex sandbox SID and held-out ACL canary before slot 1."

Push-Location $repoRoot
try {
    & $python.Source ".\scripts\run_benchmark_b0_codex_windows_acl_scored_cohort.py" --runs-root $runsRoot
    $exitCode = $LASTEXITCODE
}
finally {
    Pop-Location
}

if ($null -eq $exitCode) {
    $exitCode = 2
}
exit $exitCode
