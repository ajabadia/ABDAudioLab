<#
.SYNOPSIS
    Verifies the integrity (SHA-256 hash and size) of release artifacts and fixtures against a versioned manifest.

.DESCRIPTION
    Consumes a versioned manifest (e.g. docs/release/release-integrity-v2.1.0.json) and verifies
    local binaries and fixtures using SHA-256 and size checks.

    Exit Codes:
      0: All expected artifacts exist and match their expected SHA-256 hashes and sizes (or optional items skipped).
      1: One or more required artifacts are missing, size diverges, or SHA-256 hash diverges.
      2: Invalid arguments, missing manifest file, or JSON parsing error.

.PARAMETER ManifestPath
    Path to the JSON integrity manifest. Defaults to docs/release/release-integrity-<Version>.json.

.PARAMETER RepoRoot
    Root directory of the repository. Defaults to the parent folder of the tools directory.

.PARAMETER Version
    Version tag to locate the default manifest (default: "v2.1.0").

.PARAMETER UpdateManifest
    Explicit maintenance switch to regenerate hashes and sizes in the manifest.
    WARNING: Hash updates must be deliberate and committed explicitly in git.

.EXAMPLE
    .\tools\verify-release-hashes.ps1
    .\tools\verify-release-hashes.ps1 -ManifestPath docs\release\release-integrity-v2.1.0.json
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$ManifestPath = "",

    [Parameter(Mandatory = $false)]
    [string]$RepoRoot = "",

    [Parameter(Mandatory = $false)]
    [string]$Version = "v2.1.0",

    [Parameter(Mandatory = $false)]
    [switch]$UpdateManifest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# 1. Resolve repository root
if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $resolvedRepoRoot = (Resolve-Path "$PSScriptRoot\..").Path
} elseif ([System.IO.Path]::IsPathRooted($RepoRoot)) {
    $resolvedRepoRoot = (Resolve-Path $RepoRoot).Path
} else {
    $resolvedRepoRoot = (Resolve-Path (Join-Path (Get-Location) $RepoRoot)).Path
}

if (-not (Test-Path -Path $resolvedRepoRoot -PathType Container)) {
    Write-Host "[FATAL] Invalid repository root directory: $resolvedRepoRoot" -ForegroundColor Red
    exit 2
}

# 2. Resolve manifest path
if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $resolvedManifestPath = Join-Path $resolvedRepoRoot "docs\release\release-integrity-$Version.json"
} elseif ([System.IO.Path]::IsPathRooted($ManifestPath)) {
    $resolvedManifestPath = $ManifestPath
} else {
    $resolvedManifestPath = Join-Path $resolvedRepoRoot $ManifestPath
}

Write-Host "====================================================================="
Write-Host " ABDAudioLab - Release Artifacts & Fixtures Integrity Verifier"
Write-Host "====================================================================="
Write-Host "Repo Root: $resolvedRepoRoot"
Write-Host "Manifest:  $resolvedManifestPath"
Write-Host "---------------------------------------------------------------------"

if (-not (Test-Path -Path $resolvedManifestPath -PathType Leaf)) {
    Write-Host "[FATAL] Manifest not found: $resolvedManifestPath" -ForegroundColor Red
    exit 2
}

try {
    $manifestRaw = Get-Content -Path $resolvedManifestPath -Raw -Encoding UTF8
    $manifest = $manifestRaw | ConvertFrom-Json
} catch {
    Write-Host "[FATAL] Failed to parse JSON manifest: $_" -ForegroundColor Red
    exit 2
}

if (-not $manifest.artifacts -or -not ($manifest.artifacts -is [System.Array])) {
    Write-Host "[FATAL] Manifest does not contain a valid 'artifacts' array." -ForegroundColor Red
    exit 2
}

# 3. Handle deliberate manifest regeneration (-UpdateManifest)
if ($UpdateManifest.IsPresent) {
    Write-Host "[MAINTENANCE] Regenerating artifact hashes into manifest..." -ForegroundColor Yellow
    foreach ($art in $manifest.artifacts) {
        $relPath = $art.path
        $targetFile = if ([System.IO.Path]::IsPathRooted($relPath)) { $relPath } else { Join-Path $resolvedRepoRoot $relPath }

        if (Test-Path -Path $targetFile -PathType Leaf) {
            $computedHash = (Get-FileHash -Path $targetFile -Algorithm SHA256).Hash.ToUpperInvariant()
            $computedSize = (Get-Item -Path $targetFile).Length
            $art.sha256 = $computedHash
            $art.sizeBytes = $computedSize
            Write-Host "  Updated $relPath -> SHA-256: $computedHash ($computedSize bytes)" -ForegroundColor Cyan
        } else {
            Write-Host "  [WARN] Cannot update missing file: $relPath" -ForegroundColor Yellow
        }
    }

    $updatedJson = $manifest | ConvertTo-Json -Depth 10
    [System.IO.File]::WriteAllText($resolvedManifestPath, $updatedJson + "`n", [System.Text.Encoding]::UTF8)
    Write-Host "Manifest successfully saved to: $resolvedManifestPath" -ForegroundColor Green
    Write-Host "Please review git diff carefully before committing." -ForegroundColor Yellow
    exit 0
}

# 4. Standard verification
$failureCount = 0
$passCount = 0
$skipCount = 0

foreach ($art in $manifest.artifacts) {
    $relPath = $art.path
    $isOptional = $false
    if ($art.PSObject.Properties['optional']) {
        $isOptional = [bool]$art.optional
    }

    $targetFile = if ([System.IO.Path]::IsPathRooted($relPath)) { $relPath } else { Join-Path $resolvedRepoRoot $relPath }
    $expectedSha256 = if ($art.PSObject.Properties['sha256']) { ($art.sha256).Trim().ToUpperInvariant() } else { "" }
    $expectedSize = if ($art.PSObject.Properties['sizeBytes']) { [long]$art.sizeBytes } else { -1 }

    if (-not (Test-Path -Path $targetFile -PathType Leaf)) {
        if ($isOptional) {
            Write-Host "[SKIP] $relPath (Optional artifact not present)" -ForegroundColor DarkGray
            $skipCount++
        } else {
            Write-Host "[FAIL] $relPath" -ForegroundColor Red
            Write-Host "        error: File not found: $targetFile" -ForegroundColor Red
            $failureCount++
        }
        continue
    }

    $actualSize = (Get-Item -Path $targetFile).Length
    $actualSha256 = (Get-FileHash -Path $targetFile -Algorithm SHA256).Hash.ToUpperInvariant()

    $mismatch = $false
    if ($expectedSize -ge 0 -and $actualSize -ne $expectedSize) {
        $mismatch = $true
    }
    if (-not [string]::IsNullOrWhiteSpace($expectedSha256) -and $actualSha256 -ne $expectedSha256) {
        $mismatch = $true
    }

    if ($mismatch) {
        Write-Host "[FAIL] $relPath" -ForegroundColor Red
        if (-not [string]::IsNullOrWhiteSpace($expectedSha256) -and $actualSha256 -ne $expectedSha256) {
            Write-Host "        expected: $expectedSha256" -ForegroundColor Red
            Write-Host "        actual:   $actualSha256" -ForegroundColor Red
        }
        if ($expectedSize -ge 0 -and $actualSize -ne $expectedSize) {
            Write-Host "        expected size: $expectedSize bytes" -ForegroundColor Red
            Write-Host "        actual size:   $actualSize bytes" -ForegroundColor Red
        }
        $failureCount++
    } else {
        Write-Host "[PASS] $relPath" -ForegroundColor Green
        $passCount++
    }
}

Write-Host "---------------------------------------------------------------------"
Write-Host "Summary: $passCount PASSED, $failureCount FAILED, $skipCount SKIPPED"

if ($failureCount -gt 0) {
    Write-Host "Release integrity verification: FAILED (Exit Code 1)" -ForegroundColor Red
    exit 1
}

Write-Host "Release integrity verification: ALL PASS (Exit Code 0)" -ForegroundColor Green
exit 0
