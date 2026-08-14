param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [Parameter(Mandatory = $true)]
    [string]$VcpkgRoot,
    [Parameter(Mandatory = $true)]
    [string]$Trace2DRoot,
    [string]$OutputDirectory = (Join-Path $env:TEMP "trace2d-doctor-tests")
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )
    if (-not $Condition) {
        throw $Message
    }
}

$doctor = Join-Path $RepositoryRoot "scripts/trace2d_doctor.ps1"
$project = Join-Path $RepositoryRoot "examples/e0_external_game"
$pwsh = (Get-Command pwsh -ErrorAction Stop).Source

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$healthyPath = Join-Path $OutputDirectory "healthy.json"
$brokenPath = Join-Path $OutputDirectory "missing-vcpkg.json"

& $pwsh -NoLogo -NoProfile -File $doctor `
    -ProjectRoot $project `
    -VcpkgRoot $VcpkgRoot `
    -Trace2DRoot $Trace2DRoot `
    -OutputPath $healthyPath | Out-Null
$healthyExit = $LASTEXITCODE
Assert-Condition ($healthyExit -eq 0) "Healthy doctor run returned exit code $healthyExit."

$healthy = Get-Content -LiteralPath $healthyPath -Raw | ConvertFrom-Json
Assert-Condition ($healthy.format_version -eq 1) "Healthy report format_version changed."
Assert-Condition ($healthy.status -eq "ok") "Healthy report did not return status=ok."
Assert-Condition ($healthy.project.manifest.valid -eq $true) "Healthy report did not validate the project manifest."
Assert-Condition ($healthy.vcpkg.baseline_matches -eq $true) "Healthy report did not verify the pinned vcpkg baseline."
Assert-Condition ($healthy.trace2d_sdk.available -eq $true) "Healthy report did not find the installed Trace2D SDK."
Assert-Condition ($healthy.headless.eligible -eq $true) "Healthy report did not mark the headless path eligible."
Assert-Condition ($healthy.headless.tested -eq $false) "Doctor must not claim the headless path was tested."

$missingVcpkgRoot = Join-Path $OutputDirectory "definitely-missing-vcpkg"
& $pwsh -NoLogo -NoProfile -File $doctor `
    -ProjectRoot $project `
    -VcpkgRoot $missingVcpkgRoot `
    -Trace2DRoot $Trace2DRoot `
    -OutputPath $brokenPath | Out-Null
$brokenExit = $LASTEXITCODE
Assert-Condition ($brokenExit -eq 30) "Missing-vcpkg doctor run returned exit code $brokenExit instead of 30."

$broken = Get-Content -LiteralPath $brokenPath -Raw | ConvertFrom-Json
$brokenCategories = @($broken.diagnostics | ForEach-Object { $_.category })
Assert-Condition ($broken.status -eq "setup_error") "Broken report did not return status=setup_error."
Assert-Condition ($brokenCategories -contains "vcpkg.missing") "Broken report did not classify the missing vcpkg setup failure."
Assert-Condition ($broken.headless.eligible -eq $false) "Broken setup must not be marked headless-eligible."

Write-Host "Trace2D doctor contract passed (healthy + missing-vcpkg classification)."
