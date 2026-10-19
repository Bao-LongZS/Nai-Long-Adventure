[CmdletBinding()]
param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')

$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$scriptFile = Join-Path $PSScriptRoot 'Scripts/build_p1_timed.py'
$configFile = Join-Path $PSScriptRoot 'Scripts/p1_timed_prototype_config_v1.json'
$reportFile = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1/timed_build.json'
$logFile = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1/timed_build.log'
foreach ($required in @($projectFile, $editor, $scriptFile, $configFile)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required file missing: $required"
    }
}
New-Item -ItemType Directory -Path (Split-Path -Parent $reportFile) -Force | Out-Null
$startedUtc = [DateTime]::UtcNow
$arguments = @($projectFile, '-run=PythonScript', "-script=$scriptFile", '-unattended', '-nop4', '-nosplash', '-NullRHI', "-abslog=$logFile")
& $editor @arguments
if ($LASTEXITCODE -ne 0) {
    throw "P1 Timed map build failed ($LASTEXITCODE); inspect $logFile"
}
if (-not (Test-Path -LiteralPath $reportFile -PathType Leaf)) {
    throw "P1 Timed map build left no report: $reportFile"
}
$reportItem = Get-Item -LiteralPath $reportFile
if ($reportItem.LastWriteTimeUtc -lt $startedUtc) {
    throw "P1 Timed build report is stale: $reportFile"
}
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
if ($report.schema -ne 'NaiwaP1TimedPrototypeV1' -or $report.target_map -ne '/Game/Naiwa/P1/Maps/L_NaiwaP1_Timed' -or $report.status -ne 'saved_candidate_pending_timed_probe_and_visual_review') {
    throw "P1 Timed build status '$($report.status)'; inspect $reportFile"
}
if ($report.hash_algorithm -ne 'SHA-256' -or -not $report.target_map_sha256 -or -not $report.protected_sha256_before -or -not $report.protected_sha256_after) {
    throw "P1 Timed build report lacks independent hash evidence: $reportFile"
}
$actualConfigHash = (Get-FileHash -LiteralPath $configFile -Algorithm SHA256).Hash.ToLowerInvariant()
$actualBuilderHash = (Get-FileHash -LiteralPath $scriptFile -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualConfigHash -ne $report.config_sha256 -or $actualBuilderHash -ne $report.builder_sha256) {
    throw "P1 Timed source hashes do not match build report: $reportFile"
}
$mapPackage = Join-Path (Join-Path $PSScriptRoot 'NaiwaP0') $report.target_map_package
if (-not (Test-Path -LiteralPath $mapPackage -PathType Leaf)) {
    throw "P1 Timed map package missing: $mapPackage"
}
$actualMapHash = (Get-FileHash -LiteralPath $mapPackage -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualMapHash -ne $report.target_map_sha256) {
    throw "P1 Timed map hash does not match build report: $mapPackage"
}
foreach ($protected in $report.protected_sha256_before.PSObject.Properties) {
    $afterEntry = $report.protected_sha256_after.PSObject.Properties[$protected.Name]
    if (-not $afterEntry -or $protected.Value -ne $afterEntry.Value) {
        throw "Protected asset changed during P1 Timed build: $($protected.Name)"
    }
    $protectedPath = Join-Path (Join-Path $PSScriptRoot 'NaiwaP0') $protected.Name
    $actualHash = (Get-FileHash -LiteralPath $protectedPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -ne $afterEntry.Value) {
        throw "Protected asset hash does not match build report: $protectedPath"
    }
}
Write-Output "P1 Timed map saved; report: $reportFile"
