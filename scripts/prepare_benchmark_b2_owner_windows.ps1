param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('godot.generic', 'godot.agent', 'trace2d.agent')]
    [string]$Lane,

    [switch]$PersistForActions
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$codexVersion = '0.144.6'
$godotRelease = '4.7.1-stable'
$godotIdentity = '4.7.1.stable.official.a13da4feb'
$godotAiVersion = '3.1.5'
$godotAiCommit = '09a1e3311015153d967710fbe6502ac519585a9b'
$godotAiPackageSha256 = '51863ba177c66299808aa19ef6cd9069768915b2434d7787b9300e40c3620b04'
$vcpkgBaseline = 'd92484ed3c5020c6679d095ad3e5add907887b62'

$repoRoot = Split-Path -Parent $PSScriptRoot
$toolRoot = Join-Path $env:ProgramData 'Trace2D\b2-owner-tools'
New-Item -ItemType Directory -Force -Path $toolRoot | Out-Null

function Set-B2EnvironmentValue {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Value
    )

    [Environment]::SetEnvironmentVariable($Name, $Value, 'Process')
    if ($PersistForActions) {
        if ([string]::IsNullOrWhiteSpace($env:GITHUB_ENV)) {
            throw 'GITHUB_ENV is required when -PersistForActions is used.'
        }
        "$Name=$Value" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
    }
}

function Invoke-B2Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Label,
        [Parameter(Mandatory = $true)][scriptblock]$Command
    )

    & $Command | Out-Host
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw "$Label failed with exit code $exitCode."
    }
}

function Get-B2NativeCodex {
    param([Parameter(Mandatory = $true)][string]$Prefix)

    $candidate = Get-ChildItem -LiteralPath $Prefix -Filter 'codex.exe' -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '(?i)\\vendor\\x86_64-pc-windows-msvc\\bin\\codex\.exe$' } |
        Select-Object -ExpandProperty FullName -First 1
    if ([string]::IsNullOrWhiteSpace($candidate)) {
        throw "Native Codex executable not found under isolated prefix: $Prefix"
    }
    return [System.IO.Path]::GetFullPath($candidate)
}

function Prepare-B2SafeShell {
    $fallbackRoot = Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0'
    $fallback = Join-Path $fallbackRoot 'powershell.exe'
    if (-not (Test-Path $fallback -PathType Leaf)) {
        throw "Windows PowerShell fallback not found: $fallback"
    }

    $shimRoot = Join-Path $toolRoot 'codex-shell'
    New-Item -ItemType Directory -Force -Path $shimRoot | Out-Null
    $shim = Join-Path $shimRoot 'pwsh.exe'
    Copy-Item -LiteralPath $fallback -Destination $shim -Force
    $fallbackConfig = Join-Path $fallbackRoot 'powershell.exe.config'
    if (Test-Path $fallbackConfig -PathType Leaf) {
        Copy-Item -LiteralPath $fallbackConfig -Destination (Join-Path $shimRoot 'pwsh.exe.config') -Force
    }

    $sandboxAccount = 'CodexSandboxOffline'
    & icacls.exe $shimRoot /grant:r "${sandboxAccount}:(OI)(CI)(RX)" /T /C | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to grant sandbox read/execute on B2 shell shim: exit $LASTEXITCODE."
    }

    $pathParts = @($env:PATH -split ';' | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_) -and
        $_ -notmatch '(?i)\\WindowsApps(?:\\|$)' -and
        [System.IO.Path]::GetFullPath($_.TrimEnd('\')) -ne [System.IO.Path]::GetFullPath($shimRoot.TrimEnd('\'))
    })
    $env:PATH = (@($shimRoot) + $pathParts) -join ';'

    $resolved = (& where.exe pwsh.exe 2>$null | Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($resolved) -or
        [System.IO.Path]::GetFullPath($resolved) -ne [System.IO.Path]::GetFullPath($shim)) {
        throw "B2 pwsh shim is not first on PATH. resolved=$resolved expected=$shim"
    }

    & $shim -NoLogo -NoProfile -NonInteractive -Command 'Get-Location | Out-Null; exit 0'
    if ($LASTEXITCODE -ne 0) {
        throw "B2 sandbox-safe pwsh shim failed self-test with exit code $LASTEXITCODE."
    }

    Set-B2EnvironmentValue -Name 'TRACE2D_B2_SAFE_SHELL_ROOT' -Value $shimRoot
    Set-B2EnvironmentValue -Name 'TRACE2D_B1_SAFE_SHELL_ROOT' -Value $shimRoot
    return $shimRoot
}

function Prepare-B2Codex {
    $prefix = Join-Path $toolRoot "codex-$codexVersion"
    $native = $null
    if (Test-Path $prefix -PathType Container) {
        try {
            $native = Get-B2NativeCodex -Prefix $prefix
            $observed = (& $native --version 2>&1 | Out-String).Trim()
            if ($LASTEXITCODE -ne 0 -or $observed -notmatch '(^|\s)0\.144\.6($|\s)') {
                $native = $null
            }
        } catch {
            $native = $null
        }
    }

    if ([string]::IsNullOrWhiteSpace($native)) {
        if (Test-Path $prefix) {
            Remove-Item -LiteralPath $prefix -Recurse -Force
        }
        New-Item -ItemType Directory -Force -Path $prefix | Out-Null
        Invoke-B2Checked -Label 'isolated Codex npm install' -Command {
            npm install --no-audit --no-fund --prefix $prefix "@openai/codex@$codexVersion"
        }
        $native = Get-B2NativeCodex -Prefix $prefix
    }

    $version = (& $native --version 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $version -notmatch '(^|\s)0\.144\.6($|\s)') {
        throw "Frozen isolated Codex version mismatch: $version"
    }

    Set-B2EnvironmentValue -Name 'TRACE2D_BENCH_CODEX_BIN' -Value $native
    return @{ Path = $native; Version = $version }
}

function Prepare-B2Godot {
    $installRoot = Join-Path $toolRoot "godot-$godotRelease"
    $archiveName = "Godot_v${godotRelease}_win64.exe.zip"
    $exeName = "Godot_v${godotRelease}_win64.exe"
    $godot = Join-Path $installRoot $exeName

    $valid = $false
    if (Test-Path $godot -PathType Leaf) {
        $observed = (& $godot --version 2>&1 | Out-String).Trim()
        $valid = ($LASTEXITCODE -eq 0 -and $observed -eq $godotIdentity)
    }

    if (-not $valid) {
        if (Test-Path $installRoot) {
            Remove-Item -LiteralPath $installRoot -Recurse -Force
        }
        New-Item -ItemType Directory -Force -Path $installRoot | Out-Null

        $downloadRoot = Join-Path $toolRoot 'downloads'
        New-Item -ItemType Directory -Force -Path $downloadRoot | Out-Null
        $archive = Join-Path $downloadRoot $archiveName
        $sums = Join-Path $downloadRoot "SHA512-SUMS-$godotRelease.txt"
        $base = "https://github.com/godotengine/godot-builds/releases/download/$godotRelease"

        Invoke-WebRequest -Uri "$base/$archiveName" -OutFile $archive
        Invoke-WebRequest -Uri "$base/SHA512-SUMS.txt" -OutFile $sums

        $escapedName = [regex]::Escape($archiveName)
        $line = Get-Content -LiteralPath $sums | Where-Object { $_ -match "\s+$escapedName$" } | Select-Object -First 1
        if ([string]::IsNullOrWhiteSpace($line)) {
            throw "Pinned Godot archive missing from official SHA512-SUMS.txt: $archiveName"
        }
        $expected = (($line -split '\s+')[0]).ToLowerInvariant()
        $actual = (Get-FileHash -LiteralPath $archive -Algorithm SHA512).Hash.ToLowerInvariant()
        if ($actual -ne $expected) {
            throw "Pinned Godot SHA-512 mismatch. expected=$expected actual=$actual"
        }

        Expand-Archive -LiteralPath $archive -DestinationPath $installRoot -Force
    }

    if (-not (Test-Path $godot -PathType Leaf)) {
        throw "Pinned Godot executable missing after extraction: $godot"
    }
    $version = (& $godot --version 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $version -ne $godotIdentity) {
        throw "Pinned Godot identity mismatch: expected=$godotIdentity observed=$version"
    }

    Set-B2EnvironmentValue -Name 'TRACE2D_B2_GODOT_BIN' -Value $godot
    Set-B2EnvironmentValue -Name 'TRACE2D_BENCH_GODOT_BIN' -Value $godot
    return @{ Path = $godot; Version = $version }
}

function Prepare-B2GodotAi {
    $provider = Join-Path $toolRoot "godot-ai-$godotAiCommit"
    if (-not (Test-Path (Join-Path $provider '.git') -PathType Container)) {
        if (Test-Path $provider) {
            Remove-Item -LiteralPath $provider -Recurse -Force
        }
        New-Item -ItemType Directory -Force -Path $provider | Out-Null
        Invoke-B2Checked -Label 'godot-ai git init' -Command { git -C $provider init -q }
        Invoke-B2Checked -Label 'godot-ai remote add' -Command { git -C $provider remote add origin https://github.com/hi-godot/godot-ai.git }
    }
    Invoke-B2Checked -Label 'godot-ai exact source fetch' -Command { git -C $provider fetch -q --depth 1 origin $godotAiCommit }
    Invoke-B2Checked -Label 'godot-ai exact source checkout' -Command { git -C $provider checkout -q --force --detach FETCH_HEAD }
    $head = (& git -C $provider rev-parse HEAD | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -ne $godotAiCommit) {
        throw "Godot AI source commit mismatch: expected=$godotAiCommit observed=$head"
    }

    $pyproject = Join-Path $provider 'pyproject.toml'
    $sourceVersion = (& python -c 'import tomllib,sys; print(tomllib.load(open(sys.argv[1],"rb"))["project"]["version"])' $pyproject | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $sourceVersion -ne $godotAiVersion) {
        throw "Godot AI source version mismatch: expected=$godotAiVersion observed=$sourceVersion"
    }

    $packageRoot = Join-Path $toolRoot "godot-ai-package-$godotAiVersion"
    New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null
    $package = Get-ChildItem -LiteralPath $packageRoot -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like 'godot_ai-3.1.5*' } |
        Select-Object -First 1
    if ($null -eq $package -or
        (Get-FileHash -LiteralPath $package.FullName -Algorithm SHA256).Hash.ToLowerInvariant() -ne $godotAiPackageSha256) {
        Get-ChildItem -LiteralPath $packageRoot -Force -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force
        Invoke-B2Checked -Label 'godot-ai exact package download' -Command {
            python -m pip download --disable-pip-version-check --no-deps --dest $packageRoot "godot-ai==$godotAiVersion"
        }
        $package = Get-ChildItem -LiteralPath $packageRoot -File |
            Where-Object { $_.Name -like 'godot_ai-3.1.5*' } |
            Select-Object -First 1
    }
    if ($null -eq $package) {
        throw 'Downloaded Godot AI package was not found.'
    }
    $packageHash = (Get-FileHash -LiteralPath $package.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($packageHash -ne $godotAiPackageSha256) {
        throw "Godot AI package identity mismatch: expected=$godotAiPackageSha256 observed=$packageHash"
    }

    $venv = Join-Path $toolRoot "godot-ai-venv-$godotAiVersion"
    $venvPython = Join-Path $venv 'Scripts\python.exe'
    if (-not (Test-Path $venvPython -PathType Leaf)) {
        if (Test-Path $venv) {
            Remove-Item -LiteralPath $venv -Recurse -Force
        }
        Invoke-B2Checked -Label 'godot-ai venv creation' -Command { python -m venv $venv }
    }

    $installed = (& $venvPython -c 'import importlib.metadata as m; print(m.version("godot-ai"))' 2>$null | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $installed -ne $godotAiVersion) {
        Invoke-B2Checked -Label 'godot-ai exact package install' -Command {
            & $venvPython -m pip install --disable-pip-version-check --force-reinstall $package.FullName
        }
    }
    $installed = (& $venvPython -c 'import importlib.metadata as m; print(m.version("godot-ai"))' | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $installed -ne $godotAiVersion) {
        throw "Godot AI installed version mismatch: expected=$godotAiVersion observed=$installed"
    }

    $addon = Join-Path $provider 'plugin\addons\godot_ai'
    if (-not (Test-Path (Join-Path $addon 'plugin.cfg') -PathType Leaf)) {
        throw "Godot AI addon plugin.cfg missing: $addon"
    }

    Set-B2EnvironmentValue -Name 'TRACE2D_B2_GODOT_AI_PYTHON' -Value $venvPython
    Set-B2EnvironmentValue -Name 'TRACE2D_B2_GODOT_AI_ADDON_DIR' -Value $addon
    return @{ Python = $venvPython; Addon = $addon; Version = $installed; Commit = $head; PackageSha256 = $packageHash }
}

function Prepare-B2Trace2D {
    $vcpkgRoot = Join-Path $toolRoot "vcpkg-$vcpkgBaseline"
    if (-not (Test-Path (Join-Path $vcpkgRoot '.git') -PathType Container)) {
        if (Test-Path $vcpkgRoot) {
            Remove-Item -LiteralPath $vcpkgRoot -Recurse -Force
        }
        Invoke-B2Checked -Label 'vcpkg clone' -Command { git clone https://github.com/microsoft/vcpkg $vcpkgRoot }
    }
    Invoke-B2Checked -Label 'vcpkg baseline fetch' -Command { git -C $vcpkgRoot fetch origin $vcpkgBaseline --depth 1 }
    Invoke-B2Checked -Label 'vcpkg baseline checkout' -Command { git -C $vcpkgRoot checkout --force $vcpkgBaseline }

    $vcpkgExe = Join-Path $vcpkgRoot 'vcpkg.exe'
    if (-not (Test-Path $vcpkgExe -PathType Leaf)) {
        Invoke-B2Checked -Label 'vcpkg bootstrap' -Command { & (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics }
    }
    Set-B2EnvironmentValue -Name 'VCPKG_ROOT' -Value $vcpkgRoot

    Push-Location $repoRoot
    try {
        Invoke-B2Checked -Label 'Trace2D configure' -Command { cmake --preset windows-msvc }
        Invoke-B2Checked -Label 'Trace2D public CLI build' -Command { cmake --build --preset windows-release --target trace2d --parallel }
    } finally {
        Pop-Location
    }

    $trace2d = Join-Path $repoRoot 'build\windows-msvc\tools\trace2d\Release\trace2d.exe'
    if (-not (Test-Path $trace2d -PathType Leaf)) {
        throw "Trace2D public CLI missing after build: $trace2d"
    }
    Set-B2EnvironmentValue -Name 'TRACE2D_BENCH_TRACE2D_BIN' -Value $trace2d
    return @{ Path = $trace2d; Vcpkg = $vcpkgRoot }
}

$shellRoot = Prepare-B2SafeShell
$codex = Prepare-B2Codex
$godot = $null
$godotAi = $null
$trace2d = $null

if ($Lane.StartsWith('godot.')) {
    $godot = Prepare-B2Godot
}
if ($Lane -eq 'godot.agent') {
    $godotAi = Prepare-B2GodotAi
}
if ($Lane -eq 'trace2d.agent') {
    $trace2d = Prepare-B2Trace2D
}

$authRaw = $env:TRACE2D_BENCH_CODEX_AUTH_FILE
$auth = if ([string]::IsNullOrWhiteSpace($authRaw)) { Join-Path $HOME '.codex\auth.json' } else { $authRaw }
if (-not (Test-Path $auth -PathType Leaf)) {
    throw "Codex auth file not found for runner account: $auth"
}

Write-Host "B2 lane: $Lane"
Write-Host "B2 shell: $shellRoot"
Write-Host "B2 Codex: $($codex.Path) ($($codex.Version))"
if ($null -ne $godot) { Write-Host "B2 Godot: $($godot.Path) ($($godot.Version))" }
if ($null -ne $godotAi) { Write-Host "B2 Godot AI: $($godotAi.Version) $($godotAi.Commit) sha256:$($godotAi.PackageSha256)" }
if ($null -ne $trace2d) { Write-Host "B2 Trace2D CLI: $($trace2d.Path)" }
Write-Host "B2 auth: present (path intentionally not exported or printed)"