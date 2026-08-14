param(
    [switch]$PersistForActions
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$fallbackRoot = Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0'
$fallback = Join-Path $fallbackRoot 'powershell.exe'
if (-not (Test-Path $fallback -PathType Leaf)) {
    throw "Windows PowerShell fallback not found: $fallback"
}

$shimRoot = Join-Path $env:ProgramData 'Trace2D\b1-codex-shell'
New-Item -ItemType Directory -Force -Path $shimRoot | Out-Null
$shim = Join-Path $shimRoot 'pwsh.exe'
Copy-Item -LiteralPath $fallback -Destination $shim -Force
$fallbackConfig = Join-Path $fallbackRoot 'powershell.exe.config'
if (Test-Path $fallbackConfig -PathType Leaf) {
    Copy-Item -LiteralPath $fallbackConfig -Destination (Join-Path $shimRoot 'pwsh.exe.config') -Force
}

# The scored Windows sandbox account needs to execute the shell that Codex discovers.
# Grant only read/execute on this dedicated shim directory; never broaden WindowsApps ACLs.
$sandboxAccount = 'CodexSandboxOffline'
& icacls.exe $shimRoot /grant:r "${sandboxAccount}:(OI)(CI)(RX)" /T /C | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Failed to grant sandbox read/execute on the dedicated shell shim: exit $LASTEXITCODE."
}

& $shim -NoLogo -NoProfile -NonInteractive -Command 'Get-Location | Out-Null; exit 0'
if ($LASTEXITCODE -ne 0) {
    throw "Sandbox-safe pwsh shim failed self-test with exit code $LASTEXITCODE."
}

# GitHub Actions starts each `shell: pwsh` step in a fresh process. Rebuild PATH in
# every consumer step instead of trusting a PATH value written by a previous step.
$pathParts = @($env:PATH -split ';' | Where-Object {
    -not [string]::IsNullOrWhiteSpace($_) -and
    $_ -notmatch '(?i)\\WindowsApps(?:\\|$)' -and
    [System.IO.Path]::GetFullPath($_.TrimEnd('\')) -ne [System.IO.Path]::GetFullPath($shimRoot.TrimEnd('\'))
})
$sanitizedPath = (@($shimRoot) + $pathParts) -join ';'
$env:PATH = $sanitizedPath

$resolved = (& where.exe pwsh.exe 2>$null | Select-Object -First 1)
if ([string]::IsNullOrWhiteSpace($resolved) -or
    [System.IO.Path]::GetFullPath($resolved) -ne [System.IO.Path]::GetFullPath($shim)) {
    throw "pwsh shim is not first on PATH. resolved=$resolved expected=$shim"
}

$vendorRelative = 'vendor\x86_64-pc-windows-msvc\bin\codex.exe'
$nativeCandidates = @(
    (Join-Path $env:APPDATA "npm\node_modules\@openai\codex-win32-x64\$vendorRelative"),
    (Join-Path $env:APPDATA "npm\node_modules\@openai\codex\node_modules\@openai\codex-win32-x64\$vendorRelative"),
    (Join-Path $env:APPDATA "npm\node_modules\@openai\codex\$vendorRelative")
)
$nativeCodex = $nativeCandidates | Where-Object { Test-Path $_ -PathType Leaf } | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($nativeCodex)) {
    $openAiRoot = Join-Path $env:APPDATA 'npm\node_modules\@openai'
    if (Test-Path $openAiRoot -PathType Container) {
        $nativeCodex = Get-ChildItem -LiteralPath $openAiRoot -Filter 'codex.exe' -File -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '(?i)\\vendor\\x86_64-pc-windows-msvc\\bin\\codex\.exe$' } |
            Select-Object -ExpandProperty FullName -First 1
    }
}
if ([string]::IsNullOrWhiteSpace($nativeCodex) -or -not (Test-Path $nativeCodex -PathType Leaf)) {
    throw 'Frozen native Codex binary not found under the global @openai npm installation.'
}

$nativeVersion = (& $nativeCodex --version 2>&1 | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $nativeVersion -notmatch '(^|\s)0\.144\.6($|\s)') {
    throw "Frozen native Codex version mismatch: $nativeVersion"
}

$env:TRACE2D_B1_SAFE_SHELL_ROOT = $shimRoot
$env:TRACE2D_BENCH_CODEX_BIN = $nativeCodex

if ($PersistForActions) {
    if ([string]::IsNullOrWhiteSpace($env:GITHUB_ENV)) {
        throw 'GITHUB_ENV is required when -PersistForActions is used.'
    }
    "TRACE2D_B1_SAFE_SHELL_ROOT=$shimRoot" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
    "TRACE2D_BENCH_CODEX_BIN=$nativeCodex" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
}

Write-Host "B1 owner shell: $resolved"
Write-Host "B1 owner Codex: $nativeCodex ($nativeVersion)"
