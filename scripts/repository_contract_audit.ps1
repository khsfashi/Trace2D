param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$failures = [System.Collections.Generic.List[string]]::new()
$checks = 0

function Add-Failure {
    param([string]$Message)
    $script:failures.Add($Message)
}

function Get-RepoPath {
    param([string]$RelativePath)
    return Join-Path $RepositoryRoot ($RelativePath -replace '/', [IO.Path]::DirectorySeparatorChar)
}

function Require-File {
    param([string]$RelativePath)
    $script:checks++
    if (-not (Test-Path -LiteralPath (Get-RepoPath $RelativePath) -PathType Leaf)) {
        Add-Failure "Missing required repository contract file: $RelativePath"
    }
}

function Require-Regex {
    param(
        [string]$RelativePath,
        [string]$Pattern,
        [string]$Description
    )

    $script:checks++
    $fullPath = Get-RepoPath $RelativePath
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        Add-Failure "Cannot check '$Description' because $RelativePath is missing."
        return
    }

    $text = Get-Content -LiteralPath $fullPath -Raw
    if ($text -notmatch $Pattern) {
        Add-Failure "${RelativePath}: missing invariant '$Description'."
    }
}

function Test-LocalMarkdownLinks {
    param([string]$RelativePath)

    $fullPath = Get-RepoPath $RelativePath
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        return
    }

    $text = Get-Content -LiteralPath $fullPath -Raw
    $matches = [regex]::Matches($text, '\[[^\]]+\]\(([^)]+)\)')
    $baseDirectory = Split-Path -Parent $fullPath

    foreach ($match in $matches) {
        $rawTarget = $match.Groups[1].Value.Trim()
        if ([string]::IsNullOrWhiteSpace($rawTarget)) {
            continue
        }

        # Ignore external URLs, anchors and mail links. This auditor owns only
        # deterministic repository-local file existence.
        if ($rawTarget -match '^(https?://|mailto:|#)') {
            continue
        }

        # Strip optional markdown title text and angle brackets, then anchors.
        $target = ($rawTarget -split '\s+')[0].Trim('<', '>')
        $target = ($target -split '#', 2)[0]
        if ([string]::IsNullOrWhiteSpace($target)) {
            continue
        }

        # Keep the initial audit deliberately low-noise: only contract/document
        # file links are checked. Asset/sample links may have generator semantics.
        if ($target -notmatch '(?i)(\.md|AGENTS\.md|PROJECT_STATUS\.md|SECURITY\.md)$') {
            continue
        }

        $decodedTarget = [Uri]::UnescapeDataString($target)
        if ([IO.Path]::IsPathRooted($decodedTarget)) {
            $candidate = Get-RepoPath ($decodedTarget.TrimStart('/', '\'))
        }
        else {
            $candidate = Join-Path $baseDirectory ($decodedTarget -replace '/', [IO.Path]::DirectorySeparatorChar)
        }

        $script:checks++
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            $relativeCandidate = [IO.Path]::GetRelativePath($RepositoryRoot, $candidate)
            Add-Failure "${RelativePath}: local Markdown link '$rawTarget' resolves to missing file '$relativeCandidate'."
        }
    }
}

$requiredFiles = @(
    'AGENTS.md',
    'PROJECT_STATUS.md',
    'SECURITY.md',
    'docs/ROADMAP.md',
    'docs/AI_OPERATED_WORKFLOW.md',
    'docs/AUTONOMOUS_BENCHMARK.md',
    'docs/COMMIT_KNOWLEDGE.md',
    'docs/EXTERNAL_REFERENCE_PROTOCOL.md',
    'docs/FOUNDATIONAL_AI_GAME_REFERENCES.md',
    'docs/REFERENCE_PROJECTS.md',
    'docs/REPOSITORY_AUTOMATION.md',
    'docs/SPINE.md',
    '.github/dependabot.yml',
    '.github/workflows/ci.yml',
    '.github/workflows/codeql.yml',
    '.github/workflows/scorecard.yml'
)

foreach ($file in $requiredFiles) {
    Require-File $file
}

# Agent continuation must remain connected to the durable research and history
# protocols. These are intentionally path-level checks rather than prose parsing.
Require-Regex 'AGENTS.md' 'docs/EXTERNAL_REFERENCE_PROTOCOL\.md' 'external-reference protocol is required reading'
Require-Regex 'AGENTS.md' 'docs/COMMIT_KNOWLEDGE\.md' 'commit-knowledge protocol is required reading'
Require-Regex 'AGENTS.md' 'docs/REPOSITORY_AUTOMATION\.md' 'repository-automation contract is required reading'
Require-Regex 'AGENTS.md' '@GitHub Trace2D 다음 진행해줘' 'short next/continue command remains documented'
Require-Regex 'AGENTS.md' 'Main uses squash merges' 'squash-merge knowledge boundary remains explicit'

# Benchmark claims must never collapse to Agent self-report or one environment.
Require-Regex 'docs/AUTONOMOUS_BENCHMARK.md' 'Godot \+ generic coding tools' 'generic Godot benchmark lane'
Require-Regex 'docs/AUTONOMOUS_BENCHMARK.md' 'Godot \+.*MCP' 'Godot Agent-bridge benchmark lane'
Require-Regex 'docs/AUTONOMOUS_BENCHMARK.md' '(?i)independent verifier' 'independent benchmark verifier'
Require-Regex 'docs/AUTONOMOUS_BENCHMARK.md' '(?i)(Harness self-determinism / replay validation|replay.{0,80}determinism|determinism.{0,80}replay)' 'replay/self-determinism evidence'

# Durable commit history must preserve real validation gaps/gates and allow
# intentional replacement of an old decision.
foreach ($trailer in @('Tested:', 'Not-tested:', 'Gate:', 'Supersedes:')) {
    Require-Regex 'docs/COMMIT_KNOWLEDGE.md' ([regex]::Escape($trailer)) "commit trailer $trailer"
}
Require-Regex 'docs/COMMIT_KNOWLEDGE.md' '(?i)final squash commit' 'final squash commit is the durable knowledge atom'

# The Spine compatibility boundary is intentionally human/licensing gated and
# must not disappear through an unrelated automated edit.
Require-Regex 'docs/SPINE.md' 'SP0.*mandatory HUMAN license gate' 'Spine SP0 human license gate'
Require-Regex 'docs/SPINE.md' '(?i)do not vendor.*spine-cpp' 'Spine runtime is not silently vendored before SP0'

# The repository automation contract must keep future self-description derived
# from existing truth rather than introducing a second mandatory task database.
Require-Regex 'docs/REPOSITORY_AUTOMATION.md' '(?i)derive rather than duplicate' 'future readiness is derived rather than duplicated'
Require-Regex 'docs/REPOSITORY_AUTOMATION.md' '(?i)not eligible' 'capability eligibility remains separate from Agent failure'
Require-Regex 'docs/REPOSITORY_AUTOMATION.md' '(?i)artifact attestation' 'release provenance direction remains recorded'

$linkAuditFiles = @(
    'AGENTS.md',
    'PROJECT_STATUS.md',
    'SECURITY.md',
    'docs/AUTONOMOUS_BENCHMARK.md',
    'docs/COMMIT_KNOWLEDGE.md',
    'docs/EXTERNAL_REFERENCE_PROTOCOL.md',
    'docs/FOUNDATIONAL_AI_GAME_REFERENCES.md',
    'docs/REPOSITORY_AUTOMATION.md'
)

foreach ($file in $linkAuditFiles) {
    Test-LocalMarkdownLinks $file
}

if ($failures.Count -gt 0) {
    Write-Host "Repository contract audit FAILED ($($failures.Count) failure(s), $checks checks)." -ForegroundColor Red
    foreach ($failure in $failures) {
        Write-Host " - $failure" -ForegroundColor Red
    }
    exit 1
}

Write-Host "Repository contract audit passed ($checks checks)." -ForegroundColor Green
exit 0
