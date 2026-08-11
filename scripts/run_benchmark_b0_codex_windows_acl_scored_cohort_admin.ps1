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

$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    Fail-B0 "python was not found on PATH."
}

# Prove that the elevated PowerShell host can create/read/remove evidence before
# any model call or scored slot starts.
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

# The scored runner itself is Python. Verify the exact elevated Python process can
# create a child run directory, write/read a file inside it, and remove it. This
# catches endpoint-security or token differences that a PowerShell-only probe
# would miss, before the frozen model or any scored slot is touched.
$pythonProbeDir = Join-Path $runsRoot (".owner-python-write-probe-{0}" -f [Guid]::NewGuid().ToString("N"))
$pythonProbeCode = @'
from pathlib import Path
import sys
root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=False)
path = root / "probe.txt"
value = "TRACE2D_B0_OWNER_PYTHON_WRITE_OK"
path.write_text(value, encoding="utf-8")
if path.read_text(encoding="utf-8") != value:
    raise SystemExit("python write/read round-trip mismatch")
path.unlink()
root.rmdir()
'@
try {
    $pythonProbeCode | & $python.Source - $pythonProbeDir
    if ($LASTEXITCODE -ne 0) {
        Fail-B0 "Elevated Python cannot create/write/read/remove a child run directory under: $runsRoot"
    }
}
catch {
    Fail-B0 "Elevated Python evidence preflight failed under: $runsRoot`n$($_.Exception.Message)"
}
finally {
    Remove-Item -LiteralPath $pythonProbeDir -Recurse -Force -ErrorAction SilentlyContinue
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

Write-Host "[B0] Administrator host confirmed."
Write-Host "[B0] Evidence root PowerShell write/read preflight passed: $runsRoot"
Write-Host "[B0] Evidence root elevated-Python child write/read preflight passed."
Write-Host "[B0] No prior scored raw record exists; preregistered nine-slot cohort may start."
Write-Host "[B0] The runner will still re-prove the distinct Codex sandbox SID and held-out ACL canary before slot 1."

$previousPythonUnbuffered = $env:PYTHONUNBUFFERED
$env:PYTHONUNBUFFERED = "1"
Push-Location $repoRoot
try {
    & $python.Source ".\scripts\run_benchmark_b0_codex_windows_acl_scored_cohort.py" --runs-root $runsRoot
    $exitCode = $LASTEXITCODE
}
finally {
    Pop-Location
    if ($null -eq $previousPythonUnbuffered) {
        Remove-Item Env:PYTHONUNBUFFERED -ErrorAction SilentlyContinue
    }
    else {
        $env:PYTHONUNBUFFERED = $previousPythonUnbuffered
    }
}

if ($null -eq $exitCode) {
    $exitCode = 2
}

if ($exitCode -ne 0) {
    $latest = Get-ChildItem -Path $runsRoot -Directory -Filter "codex-chatgpt-scored-*" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($latest) {
        Write-Host "[B0] Latest scored orchestration root: $($latest.FullName)"
        $files = @(Get-ChildItem -LiteralPath $latest.FullName -File -Recurse -Force -ErrorAction SilentlyContinue)
        Write-Host "[B0] Packageable/debug file count visible to elevated PowerShell: $($files.Count)"
        if ($files.Count -gt 0) {
            $files | Select-Object -First 20 FullName, Length | Format-Table -AutoSize | Out-Host
        }
    }
}

exit $exitCode
