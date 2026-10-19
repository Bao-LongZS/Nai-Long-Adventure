[CmdletBinding()]
param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')

# Deploy only after review: wrapper -> UE5/, Python and JSON -> UE5/Scripts/.
# Parsing this staged file does not invoke UE. A build is never a route/visual PASS.
$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$projectRoot = Join-Path $PSScriptRoot 'NaiwaP0'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$scriptFile = Join-Path $PSScriptRoot 'Scripts/build_p1_integrated_full.py'
$configFile = Join-Path $PSScriptRoot 'Scripts/p1_integrated_full_config_v1.json'
$reportFile = Join-Path $projectRoot 'Saved/NaiwaP1/integrated_full_build.json'
$outerReportFile = Join-Path $projectRoot 'Saved/NaiwaP1/integrated_full_build_outer.json'
$logFile = Join-Path $projectRoot 'Saved/NaiwaP1/integrated_full_build.log'
foreach ($required in @($projectFile, $editor, $scriptFile, $configFile)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required deployed file missing: $required" }
}
$config = Get-Content -LiteralPath $configFile -Raw -Encoding UTF8 | ConvertFrom-Json
if ($config.schema -ne 'NaiwaP1IntegratedFullV1' -or
    $config.phase -ne 'P1-5b_full_candidate' -or
    $config.map -ne '/Game/Naiwa/P1/Maps/L_NaiwaP1_Integrated' -or
    $config.extension_actors.Count -ne 13 -or
    $config.protected_packages.Count -ne 11) { throw 'Unexpected full candidate config' }
$configHash = (Get-FileHash -LiteralPath $configFile -Algorithm SHA256).Hash.ToLowerInvariant()
$builderHash = (Get-FileHash -LiteralPath $scriptFile -Algorithm SHA256).Hash.ToLowerInvariant()
$resolvedProjectRoot = [IO.Path]::GetFullPath($projectRoot).TrimEnd('\') + '\'
function Get-ProtectedHashes {
    $result = [ordered]@{}
    foreach ($relativePath in $config.protected_packages) {
        $target = [IO.Path]::GetFullPath((Join-Path $projectRoot $relativePath))
        if (-not $target.StartsWith($resolvedProjectRoot, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Protected target escaped the project: $relativePath"
        }
        if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { throw "Protected package missing: $target" }
        $result[$relativePath] = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $result
}
$before = Get-ProtectedHashes
New-Item -ItemType Directory -Path (Split-Path -Parent $reportFile) -Force | Out-Null
$startedUtc = [DateTime]::UtcNow
$outer = [ordered]@{
    schema = 'NaiwaP1IntegratedFullOuterBuildV1'
    target_map = $config.map
    status = 'running'
    started_utc = $startedUtc.ToString('o')
    config_sha256 = $configHash
    builder_sha256 = $builderHash
    protected_sha256_before = $before
    acceptance = 'pending_new_map_route_checkpoint_and_visual_review'
}
$failure = $null
try {
    $arguments = @($projectFile, '-run=PythonScript', "-script=$scriptFile", '-unattended', '-nop4', '-nosplash', '-NullRHI', "-abslog=$logFile")
    & $editor @arguments
    $outer.editor_exit_code = $LASTEXITCODE
    if ($LASTEXITCODE -ne 0) { throw "P1 integrated full build exited $LASTEXITCODE; inspect $logFile" }
    if (-not (Test-Path -LiteralPath $reportFile -PathType Leaf)) { throw 'Build left no report' }
    if ((Get-Item -LiteralPath $reportFile).LastWriteTimeUtc -lt $startedUtc) { throw 'Build report is stale' }
    $report = Get-Content -LiteralPath $reportFile -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($report.schema -ne 'NaiwaP1IntegratedFullV1' -or
        $report.target_map -ne $config.map -or $report.phase -ne $config.phase -or
        $report.config_sha256 -ne $configHash -or $report.builder_sha256 -ne $builderHash -or
        $report.status -ne 'saved_full_candidate_pending_route_and_visual_review' -or
        $report.acceptance -ne 'pending_new_map_route_checkpoint_and_visual_review' -or
        $report.protected_packages_unchanged -ne $true -or
        $report.layout.kill_z -ne -1000.0 -or $report.layout.required_pickup_count -ne 1 -or
        $report.layout.original_actor_count_preserved -ne 12 -or $report.layout.verified_actor_count -ne 25 -or
        $report.layout.saved_semantics.transform_reconciliation -ne 'all_owned_actors_match_config_after_save_reload') {
        throw "Unexpected full build report/status '$($report.status)'"
    }
    $targetMap = Join-Path $projectRoot 'Content/Naiwa/P1/Maps/L_NaiwaP1_Integrated.umap'
    $mapHash = (Get-FileHash -LiteralPath $targetMap -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($report.target_map_sha256_after -ne $mapHash) { throw 'Saved map hash differs from builder report' }
    $outer.target_map_sha256_after = $mapHash
    $outer.status = 'saved_full_candidate_pending_route_and_visual_review'
} catch {
    $failure = $_
    $outer.status = 'failed'
    $outer.error = $_.ToString()
} finally {
    try {
        $after = Get-ProtectedHashes
        $outer.protected_sha256_after = $after
        $changed = @($before.Keys | Where-Object { $before[$_] -ne $after[$_] })
        $outer.protected_packages_unchanged = $changed.Count -eq 0
        if ($changed.Count -gt 0) {
            $outer.status = 'failed_protected_asset_changed'
            $outer.changed_packages = $changed
            $failure = "Protected P0/old-map/prototype/Blueprint package changed: $($changed -join ', ')"
        }
    } catch {
        $outer.status = 'failed_protected_hash_audit'
        $outer.hash_audit_error = $_.ToString()
        $failure = $_
    }
    $outer.finished_utc = [DateTime]::UtcNow.ToString('o')
    $outer | ConvertTo-Json -Depth 15 | Set-Content -LiteralPath $outerReportFile -Encoding UTF8
}
if ($failure) { throw $failure }
Write-Output "P1 full candidate saved; route/checkpoint/visual review pending. Report: $reportFile"
Write-Output "Independent protected-package audit: $outerReportFile"
