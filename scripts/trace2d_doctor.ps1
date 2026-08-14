param(
    [string]$ProjectRoot = "",
    [string]$ManifestName = "trace2d.project.json",
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$Trace2DRoot = $env:TRACE2D_ROOT,
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$diagnostics = [System.Collections.Generic.List[object]]::new()
$exitCode = 0

function Add-Diagnostic {
    param(
        [string]$Category,
        [string]$Message,
        [string]$Action,
        [int]$Code,
        [ValidateSet("error", "warning")]
        [string]$Severity = "error"
    )

    $script:diagnostics.Add([ordered]@{
        category = $Category
        severity = $Severity
        message = $Message
        action = $Action
    })

    if ($Severity -eq "error" -and $script:exitCode -eq 0) {
        $script:exitCode = $Code
    }
}

function Get-PropertyValue {
    param(
        [object]$Object,
        [string]$Name
    )

    if ($null -eq $Object) {
        return $null
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $null
    }
    return $property.Value
}

function Resolve-ProjectRoot {
    param(
        [string]$RequestedRoot,
        [string]$RequestedManifest
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedRoot)) {
        if (-not (Test-Path -LiteralPath $RequestedRoot -PathType Container)) {
            return $null
        }
        return (Resolve-Path -LiteralPath $RequestedRoot).Path
    }

    $candidate = (Get-Location).Path
    while ($true) {
        if (Test-Path -LiteralPath (Join-Path $candidate $RequestedManifest) -PathType Leaf) {
            return $candidate
        }

        $parent = [IO.Directory]::GetParent($candidate)
        if ($null -eq $parent -or $parent.FullName -eq $candidate) {
            break
        }
        $candidate = $parent.FullName
    }

    return $null
}

function Resolve-ProjectPath {
    param(
        [string]$Root,
        [string]$RelativePath
    )

    if ([string]::IsNullOrWhiteSpace($RelativePath) -or [IO.Path]::IsPathRooted($RelativePath)) {
        return $null
    }

    $rootWithSeparator = [IO.Path]::GetFullPath($Root).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    $candidate = [IO.Path]::GetFullPath((Join-Path $Root ($RelativePath -replace '/', [IO.Path]::DirectorySeparatorChar)))
    if (-not $candidate.StartsWith($rootWithSeparator, [StringComparison]::OrdinalIgnoreCase)) {
        return $null
    }
    return $candidate
}

function Read-JsonFile {
    param([string]$Path)
    return (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json)
}

function Get-GitRevision {
    param([string]$Repository)

    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($null -eq $git) {
        return $null
    }

    try {
        $revision = (& $git.Source -C $Repository rev-parse HEAD 2>$null | Select-Object -First 1)
        if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($revision)) {
            return $null
        }
        return $revision.Trim()
    }
    catch {
        return $null
    }
}

$resolvedProjectRoot = Resolve-ProjectRoot -RequestedRoot $ProjectRoot -RequestedManifest $ManifestName
$manifestPath = $null
$manifest = $null
$projectId = $null
$startupScene = $null
$contentRoot = $null
$configurePresetName = $null
$buildPresetName = $null
$testPresetName = $null
$manifestValid = $false
$expectedVcpkgBaseline = $null

if ($null -eq $resolvedProjectRoot) {
    Add-Diagnostic -Category "project.root_missing" -Message "No Trace2D project root containing '$ManifestName' was found." -Action "Run from a project directory or pass -ProjectRoot." -Code 10
}
else {
    $manifestPath = Join-Path $resolvedProjectRoot $ManifestName
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        Add-Diagnostic -Category "project.manifest_missing" -Message "Project manifest '$ManifestName' is missing." -Action "Create the versioned Trace2D project manifest at the project root." -Code 11
    }
    else {
        try {
            $manifest = Read-JsonFile -Path $manifestPath
            $formatVersion = Get-PropertyValue -Object $manifest -Name "format_version"
            $projectId = [string](Get-PropertyValue -Object $manifest -Name "project_id")
            $engine = Get-PropertyValue -Object $manifest -Name "engine"
            $startup = Get-PropertyValue -Object $manifest -Name "startup"
            $content = Get-PropertyValue -Object $manifest -Name "content"
            $build = Get-PropertyValue -Object $manifest -Name "build"
            $assets = Get-PropertyValue -Object $manifest -Name "assets"
            $texture = Get-PropertyValue -Object $assets -Name "texture"

            $minimumEngineVersion = [string](Get-PropertyValue -Object $engine -Name "minimum_version")
            $startupScene = [string](Get-PropertyValue -Object $startup -Name "scene")
            $contentRoot = [string](Get-PropertyValue -Object $content -Name "root")
            $configurePresetName = [string](Get-PropertyValue -Object $build -Name "configure_preset")
            $buildPresetName = [string](Get-PropertyValue -Object $build -Name "build_preset")
            $testPresetName = [string](Get-PropertyValue -Object $build -Name "test_preset")

            $manifestErrorsBefore = $diagnostics.Count
            if ($formatVersion -ne 1) {
                Add-Diagnostic -Category "project.manifest_schema" -Message "Only project manifest format_version 1 is supported." -Action "Set format_version to 1 or use a compatible Trace2D SDK." -Code 12
            }
            if ([string]::IsNullOrWhiteSpace($projectId) -or $projectId -notmatch '^[a-z0-9][a-z0-9._-]*$') {
                Add-Diagnostic -Category "project.project_id_invalid" -Message "project_id must be a stable lowercase semantic identifier." -Action "Use letters, digits, '.', '_' or '-' and do not derive identity from a filesystem path." -Code 12
            }
            if ([string]::IsNullOrWhiteSpace($minimumEngineVersion)) {
                Add-Diagnostic -Category "project.engine_version_missing" -Message "engine.minimum_version is required." -Action "Declare the minimum Trace2D SDK version consumed by the project." -Code 12
            }

            $resolvedContentRoot = Resolve-ProjectPath -Root $resolvedProjectRoot -RelativePath $contentRoot
            if ($null -eq $resolvedContentRoot -or -not (Test-Path -LiteralPath $resolvedContentRoot -PathType Container)) {
                Add-Diagnostic -Category "project.content_root_invalid" -Message "content.root must resolve to an existing directory inside the project." -Action "Fix content.root or create the declared content directory." -Code 12
            }

            $resolvedStartupScene = Resolve-ProjectPath -Root $resolvedProjectRoot -RelativePath $startupScene
            if ($null -eq $resolvedStartupScene -or -not (Test-Path -LiteralPath $resolvedStartupScene -PathType Leaf)) {
                Add-Diagnostic -Category "project.startup_scene_invalid" -Message "startup.scene must resolve to an existing file inside the project." -Action "Fix startup.scene or add the declared startup scene." -Code 12
            }
            elseif ($null -ne $resolvedContentRoot) {
                $contentPrefix = [IO.Path]::GetFullPath($resolvedContentRoot).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
                if (-not $resolvedStartupScene.StartsWith($contentPrefix, [StringComparison]::OrdinalIgnoreCase)) {
                    Add-Diagnostic -Category "project.startup_scene_outside_content" -Message "startup.scene must live under content.root." -Action "Move the startup scene under the declared content root." -Code 12
                }
            }

            foreach ($entry in @(
                @{ name = "build.configure_preset"; value = $configurePresetName },
                @{ name = "build.build_preset"; value = $buildPresetName },
                @{ name = "build.test_preset"; value = $testPresetName }
            )) {
                if ([string]::IsNullOrWhiteSpace([string]$entry.value)) {
                    Add-Diagnostic -Category "project.build_contract_invalid" -Message "$($entry.name) is required." -Action "Declare the supported external CMake preset contract in the project manifest." -Code 12
                }
            }

            $textureContract = [ordered]@{
                color_space = [string](Get-PropertyValue -Object $texture -Name "color_space")
                gpu_format_policy = [string](Get-PropertyValue -Object $texture -Name "gpu_format_policy")
                mip_policy = [string](Get-PropertyValue -Object $texture -Name "mip_policy")
                max_size_policy = [string](Get-PropertyValue -Object $texture -Name "max_size_policy")
                rescale_policy = [string](Get-PropertyValue -Object $texture -Name "rescale_policy")
                artifact_identity = [string](Get-PropertyValue -Object $texture -Name "artifact_identity")
            }
            foreach ($key in $textureContract.Keys) {
                if ([string]::IsNullOrWhiteSpace([string]$textureContract[$key])) {
                    Add-Diagnostic -Category "project.asset_policy_missing" -Message "assets.texture.$key is required by the E1 package contract." -Action "Declare the project-owned texture packaging policy explicitly." -Code 12
                }
            }
            if ($textureContract.artifact_identity -ne "sha256") {
                Add-Diagnostic -Category "project.asset_identity_unsupported" -Message "assets.texture.artifact_identity must be 'sha256' for manifest format 1." -Action "Use the deterministic E1 SHA-256 artifact identity policy." -Code 12
            }

            $manifestValid = ($diagnostics.Count -eq $manifestErrorsBefore)
        }
        catch {
            Add-Diagnostic -Category "project.manifest_invalid_json" -Message "Project manifest is not valid JSON: $($_.Exception.Message)" -Action "Repair trace2d.project.json before editing engine/game source." -Code 12
        }
    }
}

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmakeAvailable = $null -ne $cmakeCommand
$cmakeVersion = $null
$cmakeSupported = $false
$cmakeHelp = ""
if (-not $cmakeAvailable) {
    Add-Diagnostic -Category "cmake.missing" -Message "CMake is not available on PATH." -Action "Install CMake 3.28 or newer and make it available on PATH." -Code 20
}
else {
    try {
        $cmakeVersionLine = (& $cmakeCommand.Source --version | Select-Object -First 1)
        if ($cmakeVersionLine -match 'cmake version\s+([0-9]+\.[0-9]+(?:\.[0-9]+)?)') {
            $cmakeVersion = $Matches[1]
            $cmakeSupported = ([version]$cmakeVersion -ge [version]"3.28.0")
        }
        if (-not $cmakeSupported) {
            Add-Diagnostic -Category "cmake.unsupported" -Message "CMake 3.28 or newer is required; detected '$cmakeVersion'." -Action "Upgrade CMake before configuring the project." -Code 21
        }
        $cmakeHelp = (& $cmakeCommand.Source --help | Out-String)
    }
    catch {
        Add-Diagnostic -Category "cmake.query_failed" -Message "CMake exists but could not be queried: $($_.Exception.Message)" -Action "Repair the CMake installation before configuring Trace2D." -Code 21
    }
}

$vcpkgAvailable = -not [string]::IsNullOrWhiteSpace($VcpkgRoot) -and (Test-Path -LiteralPath $VcpkgRoot -PathType Container)
$vcpkgRevision = $null
$vcpkgBaselineMatches = $false
$vcpkgToolchainAvailable = $false
if ($null -ne $resolvedProjectRoot) {
    $projectVcpkgManifestPath = Join-Path $resolvedProjectRoot "vcpkg.json"
    if (Test-Path -LiteralPath $projectVcpkgManifestPath -PathType Leaf) {
        try {
            $projectVcpkgManifest = Read-JsonFile -Path $projectVcpkgManifestPath
            $expectedVcpkgBaseline = [string](Get-PropertyValue -Object $projectVcpkgManifest -Name "builtin-baseline")
        }
        catch {
            Add-Diagnostic -Category "vcpkg.manifest_invalid" -Message "Project vcpkg.json is invalid: $($_.Exception.Message)" -Action "Repair the external project's pinned vcpkg manifest." -Code 32
        }
    }
    else {
        Add-Diagnostic -Category "vcpkg.manifest_missing" -Message "External project vcpkg.json is missing." -Action "Add a pinned vcpkg manifest for the external consumer dependencies." -Code 32
    }
}

if (-not $vcpkgAvailable) {
    Add-Diagnostic -Category "vcpkg.missing" -Message "The selected VCPKG_ROOT does not exist." -Action "Install the pinned vcpkg revision and set VCPKG_ROOT." -Code 30
}
else {
    $vcpkgToolchainPath = Join-Path $VcpkgRoot "scripts/buildsystems/vcpkg.cmake"
    $vcpkgToolchainAvailable = Test-Path -LiteralPath $vcpkgToolchainPath -PathType Leaf
    if (-not $vcpkgToolchainAvailable) {
        Add-Diagnostic -Category "vcpkg.toolchain_missing" -Message "VCPKG_ROOT does not contain scripts/buildsystems/vcpkg.cmake." -Action "Use a complete vcpkg checkout." -Code 30
    }

    $vcpkgRevision = Get-GitRevision -Repository $VcpkgRoot
    if ([string]::IsNullOrWhiteSpace($vcpkgRevision)) {
        Add-Diagnostic -Category "vcpkg.revision_unverified" -Message "The vcpkg checkout revision could not be determined." -Action "Use a Git checkout of the project-pinned vcpkg baseline." -Code 31
    }
    elseif ([string]::IsNullOrWhiteSpace($expectedVcpkgBaseline)) {
        Add-Diagnostic -Category "vcpkg.baseline_missing" -Message "The project does not declare builtin-baseline." -Action "Pin builtin-baseline in the external project's vcpkg.json." -Code 32
    }
    else {
        $vcpkgBaselineMatches = $vcpkgRevision.Equals($expectedVcpkgBaseline, [StringComparison]::OrdinalIgnoreCase)
        if (-not $vcpkgBaselineMatches) {
            Add-Diagnostic -Category "vcpkg.baseline_mismatch" -Message "VCPKG_ROOT is at '$vcpkgRevision' but the project requires '$expectedVcpkgBaseline'." -Action "Checkout the declared builtin-baseline before configuring." -Code 31
        }
    }
}

$presetsPath = if ($null -ne $resolvedProjectRoot) { Join-Path $resolvedProjectRoot "CMakePresets.json" } else { $null }
$presetAvailable = $false
$generatorAvailable = $false
$generator = $null
$architecture = $null
$toolset = $null
if ($null -ne $presetsPath -and (Test-Path -LiteralPath $presetsPath -PathType Leaf) -and -not [string]::IsNullOrWhiteSpace($configurePresetName)) {
    try {
        $presets = Read-JsonFile -Path $presetsPath
        $configurePresets = @(Get-PropertyValue -Object $presets -Name "configurePresets")
        $selectedPreset = $configurePresets | Where-Object { [string](Get-PropertyValue -Object $_ -Name "name") -eq $configurePresetName } | Select-Object -First 1
        if ($null -eq $selectedPreset) {
            Add-Diagnostic -Category "build.preset_missing" -Message "Configure preset '$configurePresetName' is not present in CMakePresets.json." -Action "Restore the manifest-declared configure preset." -Code 40
        }
        else {
            $presetAvailable = $true
            $generator = [string](Get-PropertyValue -Object $selectedPreset -Name "generator")
            $architecture = [string](Get-PropertyValue -Object $selectedPreset -Name "architecture")
            $toolset = [string](Get-PropertyValue -Object $selectedPreset -Name "toolset")
            if ([string]::IsNullOrWhiteSpace($generator)) {
                Add-Diagnostic -Category "build.generator_missing" -Message "Configure preset '$configurePresetName' does not select a generator." -Action "Declare the maintained generator explicitly." -Code 40
            }
            elseif ($cmakeAvailable -and $cmakeSupported) {
                $generatorAvailable = $cmakeHelp -match [regex]::Escape($generator)
                if (-not $generatorAvailable) {
                    Add-Diagnostic -Category "compiler.generator_unavailable" -Message "CMake does not report generator '$generator' as available." -Action "Install the maintained compiler/toolchain or select a supported preset." -Code 41
                }
            }
        }
    }
    catch {
        Add-Diagnostic -Category "build.presets_invalid" -Message "CMakePresets.json could not be parsed: $($_.Exception.Message)" -Action "Repair the external project's versioned CMake presets." -Code 40
    }
}
elseif ($null -ne $resolvedProjectRoot) {
    Add-Diagnostic -Category "build.presets_missing" -Message "CMakePresets.json is missing or the manifest does not select a configure preset." -Action "Add the supported external consumer preset contract." -Code 40
}

$trace2dPackageAvailable = $false
$trace2dSdkMetadata = $null
$trace2dVersion = $null
$trace2dSdkBaseline = $null
if ([string]::IsNullOrWhiteSpace($Trace2DRoot) -or -not (Test-Path -LiteralPath $Trace2DRoot -PathType Container)) {
    Add-Diagnostic -Category "trace2d.package_missing" -Message "TRACE2D_ROOT does not point to an installed or extracted Trace2D SDK." -Action "Install/package Trace2D and set TRACE2D_ROOT to that prefix." -Code 50
}
else {
    $trace2dConfigPath = Join-Path $Trace2DRoot "lib/cmake/Trace2D/Trace2DConfig.cmake"
    $trace2dMetadataPath = Join-Path $Trace2DRoot "share/Trace2D/trace2d.sdk.json"
    if (-not (Test-Path -LiteralPath $trace2dConfigPath -PathType Leaf) -or -not (Test-Path -LiteralPath $trace2dMetadataPath -PathType Leaf)) {
        Add-Diagnostic -Category "trace2d.package_incomplete" -Message "TRACE2D_ROOT is missing Trace2DConfig.cmake or trace2d.sdk.json." -Action "Use the supported Trace2D install/CPack layout rather than a copied library file." -Code 50
    }
    else {
        try {
            $trace2dSdkMetadata = Read-JsonFile -Path $trace2dMetadataPath
            $trace2dVersion = [string](Get-PropertyValue -Object $trace2dSdkMetadata -Name "trace2d_version")
            $trace2dSdkBaseline = [string](Get-PropertyValue -Object $trace2dSdkMetadata -Name "vcpkg_builtin_baseline")
            $trace2dPackageAvailable = $true

            if (-not [string]::IsNullOrWhiteSpace($expectedVcpkgBaseline) -and $trace2dSdkBaseline -ne $expectedVcpkgBaseline) {
                Add-Diagnostic -Category "trace2d.dependency_baseline_mismatch" -Message "Trace2D SDK baseline '$trace2dSdkBaseline' does not match project baseline '$expectedVcpkgBaseline'." -Action "Use an SDK and external project built against the same pinned dependency baseline." -Code 51
            }

            if ($null -ne $manifest) {
                $engineContract = Get-PropertyValue -Object $manifest -Name "engine"
                $minimumVersion = [string](Get-PropertyValue -Object $engineContract -Name "minimum_version")
                if (-not [string]::IsNullOrWhiteSpace($minimumVersion) -and -not [string]::IsNullOrWhiteSpace($trace2dVersion)) {
                    if ([version]$trace2dVersion -lt [version]$minimumVersion) {
                        Add-Diagnostic -Category "trace2d.version_too_old" -Message "Trace2D SDK '$trace2dVersion' is older than required '$minimumVersion'." -Action "Install a compatible Trace2D SDK." -Code 51
                    }
                }
            }
        }
        catch {
            Add-Diagnostic -Category "trace2d.metadata_invalid" -Message "Trace2D SDK metadata is invalid: $($_.Exception.Message)" -Action "Reinstall the SDK from a supported Trace2D package." -Code 50
        }
    }
}

$healthy = $exitCode -eq 0
$compilerIdentity = if (-not [string]::IsNullOrWhiteSpace($generator)) {
    if ([string]::IsNullOrWhiteSpace($architecture)) { $generator } else { "$generator / $architecture" }
}
else {
    $null
}

$result = [ordered]@{
    format_version = 1
    status = if ($healthy) { "ok" } else { "setup_error" }
    project = [ordered]@{
        root = $resolvedProjectRoot
        manifest = [ordered]@{
            path = $manifestPath
            available = $null -ne $manifestPath -and (Test-Path -LiteralPath $manifestPath -PathType Leaf)
            valid = $manifestValid
            project_id = $projectId
            format_version = if ($null -ne $manifest) { Get-PropertyValue -Object $manifest -Name "format_version" } else { $null }
            startup_scene = $startupScene
            content_root = $contentRoot
        }
    }
    cmake = [ordered]@{
        available = $cmakeAvailable
        version = $cmakeVersion
        supported = $cmakeSupported
        minimum_version = "3.28.0"
    }
    compiler = [ordered]@{
        available = $generatorAvailable
        identity = $compilerIdentity
        generator = $generator
        architecture = $architecture
        toolset = $toolset
        tested = $false
        supported = $presetAvailable -and $generatorAvailable
    }
    vcpkg = [ordered]@{
        available = $vcpkgAvailable
        root = if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) { $null } else { $VcpkgRoot }
        expected_baseline = $expectedVcpkgBaseline
        actual_revision = $vcpkgRevision
        baseline_matches = $vcpkgBaselineMatches
        toolchain_available = $vcpkgToolchainAvailable
    }
    trace2d_sdk = [ordered]@{
        available = $trace2dPackageAvailable
        root = if ([string]::IsNullOrWhiteSpace($Trace2DRoot)) { $null } else { $Trace2DRoot }
        version = $trace2dVersion
        vcpkg_baseline = $trace2dSdkBaseline
    }
    build = [ordered]@{
        configure_preset = $configurePresetName
        build_preset = $buildPresetName
        test_preset = $testPresetName
        preset_available = $presetAvailable
    }
    headless = [ordered]@{
        available = $trace2dPackageAvailable
        eligible = $healthy
        tested = $false
        supported = $healthy
    }
    diagnostics = @($diagnostics)
}

$json = $result | ConvertTo-Json -Depth 10
if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
    $outputDirectory = Split-Path -Parent $OutputPath
    if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    }
    Set-Content -LiteralPath $OutputPath -Value $json -Encoding utf8
}

Write-Output $json
exit $exitCode
