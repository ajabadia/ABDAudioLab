<#
.SYNOPSIS
    Verifies the integrity (size and SHA-256 hash) of release artifacts against a versioned manifest.

.DESCRIPTION
    Consumes docs/release/release-integrity-<Version>.json and validates local binaries.
    Returns:
      Exit Code 0: All artifacts match size and SHA-256 exactly.
      Exit Code 1: One or more artifacts are missing, size diverges, or SHA-256 diverges.
      Exit Code 2: Invalid invocation parameters, missing directory, or unparseable manifest.

.EXAMPLE
    .\tools\verify-release-hashes.ps1 -Version v2.1.0 -ArtifactsDirectory .\build\ABDAudioLab_artefacts\Release
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$Version = "v2.1.0",

    [Parameter(Mandatory = $false)]
    [string]$ArtifactsDirectory = "build\ABDAudioLab_artefacts\Release",

    [Parameter(Mandatory = $false)]
    [string]$ManifestPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# 1. Resolve repository root and manifest path
$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path

if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $ManifestPath = Join-Path $repoRoot "docs\release\release-integrity-$Version.json"
}

Write-Host "====================================================================="
Write-Host " ABDAudioLab - Release Artifacts Integrity Verifier"
Write-Host "====================================================================="

# 2. Validate manifest existence and structure (Exit 2 if invalid)
if (-not (Test-Path -Path $ManifestPath -PathType Leaf)) {
    Write-Host "[FATAL] Manifest not found: $ManifestPath" -ForegroundColor Red
    exit 2
}

try {
    $manifestContent = Get-Content -Path $ManifestPath -Raw -Encoding UTF8
    $manifest = $manifestContent | ConvertFrom-Json
} catch {
    Write-Host "[FATAL] Failed to parse JSON manifest: $_" -ForegroundColor Red
    exit 2
}

if (-not $manifest.artifacts -or -not ($manifest.artifacts -is [System.Array])) {
    Write-Host "[FATAL] Manifest does not contain a valid 'artifacts' array." -ForegroundColor Red
    exit 2
}

# 3. Validate artifacts directory existence (Exit 2 if parameter invalid)
if ([System.IO.Path]::IsPathRooted($ArtifactsDirectory)) {
    $resolvedArtifactsDir = $ArtifactsDirectory
} else {
    $resolvedArtifactsDir = Join-Path $repoRoot $ArtifactsDirectory
}
if (-not (Test-Path -Path $resolvedArtifactsDir -PathType Container)) {
    Write-Host "[FATAL] Artifacts directory does not exist or is not a directory: $resolvedArtifactsDir" -ForegroundColor Red
    exit 2
}

Write-Host "Manifest:    $ManifestPath"
Write-Host "Version:     $($manifest.version) (Commit: $($manifest.releaseCommit))"
Write-Host "Artifacts:   $resolvedArtifactsDir"
Write-Host "---------------------------------------------------------------------"

# 4. Verify each artifact
$hasFailure = $false

foreach ($art in $manifest.artifacts) {
    $fileName = $art.path
    $expectedSize = [long]$art.sizeBytes
    $expectedSha256 = ($art.sha256).Trim().ToUpperInvariant()
    $targetFile = Join-Path $resolvedArtifactsDir $fileName

    Write-Host "Checking: $fileName"

    if (-not (Test-Path -Path $targetFile -PathType Leaf)) {
        Write-Host "  [FAIL] File does not exist: $targetFile" -ForegroundColor Red
        $hasFailure = $true
        continue
    }

    # Verify size
    $actualSize = (Get-Item -Path $targetFile).Length
    if ($actualSize -ne $expectedSize) {
        Write-Host "  [FAIL] Size mismatch: got $actualSize bytes, expected $expectedSize bytes" -ForegroundColor Red
        $hasFailure = $true
        continue
    }

    # Verify SHA-256
    $actualSha256 = (Get-FileHash -Path $targetFile -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($actualSha256 -ne $expectedSha256) {
        Write-Host "  [FAIL] SHA-256 mismatch:" -ForegroundColor Red
        Write-Host "         Expected: $expectedSha256" -ForegroundColor Red
        Write-Host "         Actual:   $actualSha256" -ForegroundColor Red
        $hasFailure = $true
        continue
    }

    Write-Host "  [PASS] Size: $actualSize bytes" -ForegroundColor Green
    Write-Host "  [PASS] SHA-256: $actualSha256" -ForegroundColor Green
}

Write-Host "---------------------------------------------------------------------"

if ($hasFailure) {
    Write-Host "Release integrity verification: FAILED" -ForegroundColor Red
    exit 1
}

Write-Host "Release integrity verification: PASS" -ForegroundColor Green
exit 0
