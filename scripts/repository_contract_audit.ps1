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

        if ($rawTarget -match '^(https?://|mailto:|#)') {
            continue
        }

        $target = ($rawTarget -split '\s+')[0].Trim('<', '>')
        $target = ($target -split '#', 2)[0]
        if ([string]::IsNullOrWhiteSpace($target)) {
            continue
        }

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
    'docs/COMPETITIVE_STRATEGY.md',
    'docs/DEVELOPMENT_CONTENT_PIPELINE.md',
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

# Agent continuation must remain connected to durable research/history protocols.
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

# Competitive comparisons use a strong current baseline and preserve losses.
Require-Regex 'docs/COMPETITIVE_STRATEGY.md' '(?i)strongest-baseline rule' 'strongest current Godot baseline selection'
Require-Regex 'docs/COMPETITIVE_STRATEGY.md' '(?i)losing results are valid results' 'unfavorable benchmark results remain publishable evidence'
Require-Regex 'docs/COMPETITIVE_STRATEGY.md' 'satelliteoflove/godot-mcp' 'runtime-verification Godot baseline candidate'
Require-Regex 'docs/COMPETITIVE_STRATEGY.md' 'hi-godot/godot-ai' 'broad-authoring Godot baseline candidate'
Require-Regex 'docs/COMPETITIVE_STRATEGY.md' 'Erodenn/godot-mcp-runtime' 'zero-footprint Godot baseline candidate'

# Development content may accumulate evidence automatically, but prose requires
# an explicit owner request and remains a draft until human edit/publication.
Require-Regex 'docs/DEVELOPMENT_CONTENT_PIPELINE.md' '(?i)Evidence is accumulated automatically' 'automatic evidence collection remains separate from prose'
Require-Regex 'docs/DEVELOPMENT_CONTENT_PIPELINE.md' '(?i)Prose is generated only when the maintainer explicitly asks' 'draft generation requires explicit maintainer request'
Require-Regex 'docs/DEVELOPMENT_CONTENT_PIPELINE.md' '(?i)Editorial Brief' 'on-demand authoring preserves explicit editorial direction'
Require-Regex 'docs/DEVELOPMENT_CONTENT_PIPELINE.md' '(?i)Author Reference Corpus' 'maintainer-authored style corpus is an explicit input'
Require-Regex 'docs/DEVELOPMENT_CONTENT_PIPELINE.md' 'https://woodroot\.tistory\.com/' 'approved public author style reference remains recorded'
Require-Regex 'docs/DEVELOPMENT_CONTENT_PIPELINE.md' '(?i)Dynamic platform registry' 'platform targets remain registry-driven'
Require-Regex 'docs/DEVELOPMENT_CONTENT_PIPELINE.md' '(?i)Do not hard-code Tistory, X, Reddit, Hacker News, GeekNews' 'platform list is not hard-coded into core schema'
Require-Regex 'docs/DEVELOPMENT_CONTENT_PIPELINE.md' '(?i)do not need to share wording, length, angle, language, structure' 'platform content may differ while sharing evidence'
Require-Regex 'docs/DEVELOPMENT_CONTENT_PIPELINE.md' 'Content: none \| candidate \| major \| release' 'optional content-significance trailer remains reserved'
Require-Regex 'docs/DEVELOPMENT_CONTENT_PIPELINE.md' '(?i)never invent personal anecdotes' 'style imitation cannot fabricate biography'
Require-Regex 'docs/DEVELOPMENT_CONTENT_PIPELINE.md' '(?i)no draft is generated without an explicit maintainer request' 'C1 preserves explicit drafting trigger'
Require-Regex 'docs/DEVELOPMENT_CONTENT_PIPELINE.md' '(?i)never automatically published' 'generated drafts cannot become external posts automatically'

# Durable commit history preserves validation gaps/gates and decision lineage.
foreach ($trailer in @('Tested:', 'Not-tested:', 'Gate:', 'Supersedes:')) {
    Require-Regex 'docs/COMMIT_KNOWLEDGE.md' ([regex]::Escape($trailer)) "commit trailer $trailer"
}
Require-Regex 'docs/COMMIT_KNOWLEDGE.md' '(?i)final squash commit' 'final squash commit is the durable knowledge atom'

# Spine remains explicitly human/license gated.
Require-Regex 'docs/SPINE.md' 'SP0.*mandatory HUMAN license gate' 'Spine SP0 human license gate'
Require-Regex 'docs/SPINE.md' '(?i)do not vendor.*spine-cpp' 'Spine runtime is not silently vendored before SP0'

# Repository automation derives truth instead of duplicating project state.
Require-Regex 'docs/REPOSITORY_AUTOMATION.md' '(?i)derive rather than duplicate' 'future readiness is derived rather than duplicated'
Require-Regex 'docs/REPOSITORY_AUTOMATION.md' '(?i)not eligible' 'capability eligibility remains separate from Agent failure'
Require-Regex 'docs/REPOSITORY_AUTOMATION.md' '(?i)artifact attestation' 'release provenance direction remains recorded'

$linkAuditFiles = @(
    'AGENTS.md',
    'PROJECT_STATUS.md',
    'SECURITY.md',
    'docs/AUTONOMOUS_BENCHMARK.md',
    'docs/COMMIT_KNOWLEDGE.md',
    'docs/COMPETITIVE_STRATEGY.md',
    'docs/DEVELOPMENT_CONTENT_PIPELINE.md',
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
