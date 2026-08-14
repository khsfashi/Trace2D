param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [Parameter(Mandatory = $true)]
    [string]$BuildDirectory,
    [Parameter(Mandatory = $true)]
    [string]$VcpkgRoot,
    [string]$Configuration = "Debug",
    [string]$ConfigurePreset = "windows-msvc",
    [string]$EvidenceDirectory = (Join-Path $env:TEMP "trace2d-e1-external-consumer")
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Invoke-Checked {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$Description
    )

    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

function Get-CanonicalTreeDigest {
    param(
        [string]$Root,
        [string]$ManifestPath
    )

    $lines = [System.Collections.Generic.List[string]]::new()
    $files = Get-ChildItem -LiteralPath $Root -File -Recurse | Sort-Object FullName
    foreach ($file in $files) {
        $relative = [IO.Path]::GetRelativePath($Root, $file.FullName).Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        $lines.Add("$relative`t$hash")
    }

    $manifestText = ($lines -join "`n") + "`n"
    Set-Content -LiteralPath $ManifestPath -Value $manifestText -Encoding utf8NoBOM

    $bytes = [Text.Encoding]::UTF8.GetBytes($manifestText)
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Get-CMakeCacheValue {
    param(
        [string]$CachePath,
        [string]$Key
    )

    if (-not (Test-Path -LiteralPath $CachePath -PathType Leaf)) {
        return $null
    }

    $match = Get-Content -LiteralPath $CachePath | Where-Object { $_ -match "^$([regex]::Escape($Key))(?::[^=]+)?=(.*)$" } | Select-Object -First 1
    if ($null -eq $match) {
        return $null
    }
    if ($match -match '^[^=]+=(.*)$') {
        return $Matches[1]
    }
    return $null
}

$cmake = (Get-Command cmake -ErrorAction Stop).Source
$cpack = (Get-Command cpack -ErrorAction Stop).Source
$ctest = (Get-Command ctest -ErrorAction Stop).Source
$git = (Get-Command git -ErrorAction Stop).Source

$resolvedBuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
$cpackConfig = Join-Path $resolvedBuildDirectory "CPackConfig.cmake"
if (-not (Test-Path -LiteralPath $cpackConfig -PathType Leaf)) {
    throw "CPackConfig.cmake is missing from '$resolvedBuildDirectory'. Configure Trace2D with TRACE2D_INSTALL_SDK=ON."
}

if (Test-Path -LiteralPath $EvidenceDirectory) {
    Remove-Item -LiteralPath $EvidenceDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null

$installA = Join-Path $EvidenceDirectory "install-a"
$installB = Join-Path $EvidenceDirectory "install-b"
$packageADirectory = Join-Path $EvidenceDirectory "package-a"
$packageBDirectory = Join-Path $EvidenceDirectory "package-b"
$extractDirectory = Join-Path $EvidenceDirectory "package-extracted"
$doctorEvidence = Join-Path $EvidenceDirectory "doctor"
New-Item -ItemType Directory -Path $packageADirectory, $packageBDirectory, $extractDirectory, $doctorEvidence -Force | Out-Null

Invoke-Checked -Executable $cmake -Arguments @(
    "--install", $resolvedBuildDirectory,
    "--config", $Configuration,
    "--prefix", $installA
) -Description "First SDK install"

Invoke-Checked -Executable $cmake -Arguments @(
    "--install", $resolvedBuildDirectory,
    "--config", $Configuration,
    "--prefix", $installB
) -Description "Repeated SDK install"

$installManifestA = Join-Path $EvidenceDirectory "install-a.files.sha256"
$installManifestB = Join-Path $EvidenceDirectory "install-b.files.sha256"
$installDigestA = Get-CanonicalTreeDigest -Root $installA -ManifestPath $installManifestA
$installDigestB = Get-CanonicalTreeDigest -Root $installB -ManifestPath $installManifestB
if ($installDigestA -ne $installDigestB) {
    throw "Repeated install trees differ: $installDigestA vs $installDigestB."
}

Invoke-Checked -Executable $cpack -Arguments @(
    "--config", $cpackConfig,
    "-C", $Configuration,
    "-G", "ZIP",
    "-B", $packageADirectory
) -Description "First CPack package"

Invoke-Checked -Executable $cpack -Arguments @(
    "--config", $cpackConfig,
    "-C", $Configuration,
    "-G", "ZIP",
    "-B", $packageBDirectory
) -Description "Repeated CPack package"

$packageA = Get-ChildItem -LiteralPath $packageADirectory -Filter "*.zip" -File | Select-Object -First 1
$packageB = Get-ChildItem -LiteralPath $packageBDirectory -Filter "*.zip" -File | Select-Object -First 1
if ($null -eq $packageA -or $null -eq $packageB) {
    throw "CPack did not produce the expected ZIP artifacts."
}

$packageHashA = (Get-FileHash -LiteralPath $packageA.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$packageHashB = (Get-FileHash -LiteralPath $packageB.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$packageByteReproducible = $packageHashA -eq $packageHashB

Expand-Archive -LiteralPath $packageA.FullName -DestinationPath $extractDirectory -Force
$installedConfig = Get-ChildItem -LiteralPath $extractDirectory -Filter "Trace2DConfig.cmake" -File -Recurse |
    Where-Object { $_.FullName.Replace('\', '/') -match '/lib/cmake/Trace2D/Trace2DConfig\.cmake$' } |
    Select-Object -First 1
if ($null -eq $installedConfig) {
    throw "Packaged SDK does not contain lib/cmake/Trace2D/Trace2DConfig.cmake."
}

$trace2dDirectory = Split-Path -Parent $installedConfig.FullName
$cmakeDirectory = Split-Path -Parent $trace2dDirectory
$libDirectory = Split-Path -Parent $cmakeDirectory
$packageRoot = Split-Path -Parent $libDirectory

$doctorTest = Join-Path $RepositoryRoot "scripts/test_trace2d_doctor.ps1"
& $doctorTest `
    -RepositoryRoot $RepositoryRoot `
    -VcpkgRoot $VcpkgRoot `
    -Trace2DRoot $packageRoot `
    -OutputDirectory $doctorEvidence

$projectRoot = Join-Path $RepositoryRoot "examples/e0_external_game"
$externalBuildDirectory = Join-Path $projectRoot "build/windows-msvc"
if (Test-Path -LiteralPath $externalBuildDirectory) {
    Remove-Item -LiteralPath $externalBuildDirectory -Recurse -Force
}

$previousTrace2DRoot = $env:TRACE2D_ROOT
$previousVcpkgRoot = $env:VCPKG_ROOT
try {
    $env:TRACE2D_ROOT = $packageRoot
    $env:VCPKG_ROOT = $VcpkgRoot
    Push-Location $projectRoot
    try {
        Invoke-Checked -Executable $cmake -Arguments @("--preset", $ConfigurePreset) -Description "External consumer configure"
        Invoke-Checked -Executable $cmake -Arguments @("--build", "--preset", "windows-debug", "--parallel") -Description "External consumer build"
        Invoke-Checked -Executable $ctest -Arguments @("--preset", "windows-debug") -Description "External consumer headless test"
    }
    finally {
        Pop-Location
    }
}
finally {
    $env:TRACE2D_ROOT = $previousTrace2DRoot
    $env:VCPKG_ROOT = $previousVcpkgRoot
}

$sourceRevision = (& $git -C $RepositoryRoot rev-parse HEAD | Select-Object -First 1).Trim()
$vcpkgRevision = (& $git -C $VcpkgRoot rev-parse HEAD | Select-Object -First 1).Trim()
$rootVcpkg = Get-Content -LiteralPath (Join-Path $RepositoryRoot "vcpkg.json") -Raw | ConvertFrom-Json
$cmakeVersionLine = (& $cmake --version | Select-Object -First 1)
$cmakeVersion = if ($cmakeVersionLine -match 'cmake version\s+(.+)$') { $Matches[1].Trim() } else { $cmakeVersionLine }
$cachePath = Join-Path $resolvedBuildDirectory "CMakeCache.txt"

$provenance = [ordered]@{
    format_version = 1
    artifact_kind = "trace2d-sdk-ci-evidence"
    publication_status = "ci-evidence-not-release"
    source = [ordered]@{
        revision = $sourceRevision
        configure_preset = $ConfigurePreset
        configuration = $Configuration
    }
    toolchain = [ordered]@{
        cmake_version = $cmakeVersion
        generator = Get-CMakeCacheValue -CachePath $cachePath -Key "CMAKE_GENERATOR"
        generator_platform = Get-CMakeCacheValue -CachePath $cachePath -Key "CMAKE_GENERATOR_PLATFORM"
        generator_toolset = Get-CMakeCacheValue -CachePath $cachePath -Key "CMAKE_GENERATOR_TOOLSET"
        cxx_compiler = Get-CMakeCacheValue -CachePath $cachePath -Key "CMAKE_CXX_COMPILER"
    }
    dependencies = [ordered]@{
        vcpkg_builtin_baseline = [string]$rootVcpkg.'builtin-baseline'
        vcpkg_revision = $vcpkgRevision
        baseline_matches = ([string]$rootVcpkg.'builtin-baseline').Equals($vcpkgRevision, [StringComparison]::OrdinalIgnoreCase)
    }
    install_tree = [ordered]@{
        sha256 = $installDigestA
        repeated_sha256 = $installDigestB
        repeat_reproducible = $true
        comparison_scope = "two installs from the same compiled build tree"
    }
    package = [ordered]@{
        file = $packageA.Name
        sha256 = $packageHashA
        repeated_sha256 = $packageHashB
        repeat_byte_reproducible = $packageByteReproducible
        format = "zip"
        consumer_tested_from_extracted_package = $true
    }
    verification = [ordered]@{
        doctor_healthy = $true
        doctor_missing_vcpkg_classified = $true
        external_configure = $true
        external_build = $true
        headless_tested = $true
        windowed_host_compiled = $true
    }
    reproducibility_boundaries = @(
        "The install-tree comparison repeats installation from one compiled build; independent compiler/linker byte reproducibility is not claimed by E1.",
        "The ZIP package is generated twice and both SHA-256 values are recorded. ZIP timestamps or compiler/archive metadata may make raw archives differ; a difference is evidence, not silently rewritten.",
        "Code signing, release attestations and SBOM publication are deferred until Trace2D publishes a user-facing release artifact rather than CI acceptance evidence."
    )
}

$provenancePath = Join-Path $EvidenceDirectory "provenance.json"
$provenance | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $provenancePath -Encoding utf8

Write-Host "Trace2D E1 external-consumer gate passed."
Write-Host "Install tree SHA-256: $installDigestA"
Write-Host "Package SHA-256: $packageHashA"
Write-Host "Repeated package byte-identical: $packageByteReproducible"
Write-Host "Evidence: $EvidenceDirectory"
