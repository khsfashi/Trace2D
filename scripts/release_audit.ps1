param(
    [switch]$RequireLicense
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = (& git rev-parse --show-toplevel).Trim()
if (-not $repoRoot) {
    throw "release_audit.ps1 must run inside a Git worktree."
}

Push-Location $repoRoot
try {
    $failures = [System.Collections.Generic.List[string]]::new()
    $trackedFiles = @(& git ls-files)

    function Add-AuditFailure {
        param([string]$Message)
        $failures.Add($Message)
    }

    function Remove-KnownPublicCiWorkspacePaths {
        param([string]$Text)

        # GitHub-hosted Linux runners use a machine-generic checkout workspace
        # rooted under the runner account. It contains neither a developer home
        # directory nor a private account identifier, while the generic Linux
        # home detector intentionally cannot distinguish it on its own. Build the
        # known CI pattern from fragments so the audit source never self-matches.
        $githubActionsWorkspacePattern = '/ho' + 'me/runner/work/[^/\r\n\t ]+/[^/\r\n\t ]+/'
        $normalized = [regex]::Replace(
            $Text,
            $githubActionsWorkspacePattern,
            '/github-actions/workspace/',
            [System.Text.RegularExpressions.RegexOptions]::IgnoreCase
        )

        # Earlier audit-hardening commits used literal illustrative placeholders
        # while documenting the detector. Normalize only those exact placeholders
        # in Git history; real developer/user paths remain covered.
        $illustrativeHomePlaceholder = '/ho' + 'me/<user>/'
        $illustrativeWildcardHome = '/ho' + 'me/*/'
        $normalized = $normalized.Replace($illustrativeHomePlaceholder, '/example-user-home/')
        return $normalized.Replace($illustrativeWildcardHome, '/example-user-home-wildcard/')
    }

    Write-Host "[release-audit] Checking tracked generated/build artifacts..."

    $forbiddenTrackedPatterns = @(
        '^(?:build|Build|out|vcpkg_installed|\.vs|Testing)(?:/|$)',
        '\.(?:obj|pdb|ilk|lib|dll|exe|so|dylib|a)$',
        '^(?:CMakeCache\.txt|compile_commands\.json)$'
    )

    foreach ($path in $trackedFiles) {
        foreach ($pattern in $forbiddenTrackedPatterns) {
            if ($path -match $pattern) {
                Add-AuditFailure "Tracked generated/build artifact: $path"
                break
            }
        }
    }

    Write-Host "[release-audit] Checking repository-relative Markdown links..."

    foreach ($path in $trackedFiles) {
        if ([System.IO.Path]::GetExtension($path) -ne '.md') {
            continue
        }

        $absolutePath = Join-Path $repoRoot $path
        $content = Get-Content -LiteralPath $absolutePath -Raw
        $matches = [regex]::Matches($content, '\[[^\]]+\]\((?<target>[^)]+)\)')

        foreach ($match in $matches) {
            $target = $match.Groups['target'].Value.Trim()
            if ($target.StartsWith('<') -and $target.EndsWith('>')) {
                $target = $target.Substring(1, $target.Length - 2)
            }

            if ($target -match '^(?:https?|mailto):' -or $target.StartsWith('#')) {
                continue
            }

            if ($target.Contains(' "')) {
                $target = $target.Substring(0, $target.IndexOf(' "'))
            }

            $targetPath = $target.Split('#', 2)[0]
            if (-not $targetPath) {
                continue
            }

            $decodedTarget = [System.Uri]::UnescapeDataString($targetPath)
            $sourceDirectory = Split-Path -Parent $absolutePath
            if ($decodedTarget.StartsWith('/')) {
                $resolved = Join-Path $repoRoot $decodedTarget.TrimStart('/')
            }
            else {
                $resolved = Join-Path $sourceDirectory $decodedTarget
            }

            if (-not (Test-Path -LiteralPath $resolved)) {
                Add-AuditFailure "Broken relative Markdown link in ${path}: $targetPath"
            }
        }
    }

    Write-Host "[release-audit] Checking current tree and full Git patch history for high-confidence secrets/private paths..."

    $secretPatterns = [ordered]@{
        'private-key' = '-----BEGIN ' + '(?:RSA |EC |OPENSSH )?' + 'PRIVATE KEY-----'
        'aws-access-key' = '\bAK' + 'IA[0-9A-Z]{16}\b'
        'github-token' = '\bgh' + '[pousr]_[A-Za-z0-9]{36,}\b'
        'google-api-key' = '\bAI' + 'za[0-9A-Za-z_-]{35}\b'
        'slack-token' = '\bxox' + '[aboprs]-[0-9A-Za-z-]{10,}\b'
        'windows-user-path' = '[A-Za-z]:\\Us' + 'ers\\[^\\\r\n\t ]+\\'
        'mac-user-path' = '/Us' + 'ers/[^/\r\n\t ]+/'
        'linux-home-path' = '/ho' + 'me/[^/\r\n\t ]+/'
        'private-http-endpoint' = 'https?://[^\s/]*(?:\.inter' + 'nal|\.co' + 'rp)(?::\d+)?(?:/|\b)'
    }

    foreach ($path in $trackedFiles) {
        $absolutePath = Join-Path $repoRoot $path
        $bytes = [System.IO.File]::ReadAllBytes($absolutePath)
        $probeLength = [Math]::Min($bytes.Length, 8192)
        $isBinary = $false
        for ($index = 0; $index -lt $probeLength; ++$index) {
            if ($bytes[$index] -eq 0) {
                $isBinary = $true
                break
            }
        }

        if ($isBinary) {
            continue
        }

        $content = [System.Text.Encoding]::UTF8.GetString($bytes)
        $contentForSecretScan = Remove-KnownPublicCiWorkspacePaths $content
        foreach ($entry in $secretPatterns.GetEnumerator()) {
            if ([regex]::IsMatch($contentForSecretScan, $entry.Value, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)) {
                Add-AuditFailure "Current tree matched $($entry.Key): $path"
            }
        }
    }

    $historyLines = @(& git log --all --format= --patch --no-ext-diff --no-color -- .)
    if ($LASTEXITCODE -ne 0) {
        throw "git log failed while scanning repository history."
    }
    $history = [string]::Join("`n", $historyLines)
    $historyForSecretScan = Remove-KnownPublicCiWorkspacePaths $history

    foreach ($entry in $secretPatterns.GetEnumerator()) {
        if ([regex]::IsMatch($historyForSecretScan, $entry.Value, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)) {
            Add-AuditFailure "Git patch history matched high-confidence pattern: $($entry.Key)"
        }
    }

    if ($RequireLicense) {
        Write-Host "[release-audit] Requiring a repository license..."
        $licenseFiles = @($trackedFiles | Where-Object { $_ -match '^(?:LICENSE|LICENSE\.txt|LICENSE\.md)$' })
        if ($licenseFiles.Count -eq 0) {
            Add-AuditFailure "No root repository LICENSE file is tracked."
        }
    }
    else {
        Write-Host "[release-audit] License presence is not enforced in this pre-decision audit."
    }

    if ($failures.Count -gt 0) {
        Write-Error ("Public-release audit failed:`n - " + ($failures -join "`n - "))
        exit 1
    }

    Write-Host "[release-audit] PASS"
}
finally {
    Pop-Location
}
