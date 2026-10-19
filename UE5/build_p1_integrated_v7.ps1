[CmdletBinding()]
param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')

# Execute only after the isolated A/B/C gates. A saved map is not route acceptance.
$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$projectRoot = Join-Path $PSScriptRoot 'NaiwaP0'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$scriptFile = Join-Path $PSScriptRoot 'Scripts/build_p1_integrated_v7.py'
$configFile = Join-Path $PSScriptRoot 'Scripts/p1_integrated_v7_config.json'
$sourceConfigFile = Join-Path $PSScriptRoot 'Scripts/p1_integrated_full_config_v1.json'
$reportFile = Join-Path $projectRoot 'Saved/NaiwaP1/integrated_v7_build.json'
$outerReportFile = Join-Path $projectRoot 'Saved/NaiwaP1/integrated_v7_build_outer.json'
$logFile = Join-Path $projectRoot 'Saved/NaiwaP1/integrated_v7_build.log'
foreach ($required in @($projectFile, $editor, $scriptFile, $configFile, $sourceConfigFile)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required build file missing: $required" }
}
$config = Get-Content -LiteralPath $configFile -Raw -Encoding UTF8 | ConvertFrom-Json
if ($config.schema -ne 'NaiwaP1IntegratedV7V1' -or
    $config.phase -ne 'P1-5b_v7_candidate' -or
    $config.map -ne '/Game/Naiwa/P1/Maps/L_NaiwaP1_v7' -or
    $config.source_map -ne '/Game/Naiwa/P1/Maps/L_NaiwaP1_Integrated' -or
    $config.extension_actors.Count -ne 59 -or
    $config.required_pickup_count -ne 1 -or
    $config.checkpoints.Count -ne 8 -or
    $config.gap_chains.Count -ne 3 -or
    $config.timed_c_design.role -ne 'mandatory_timed_transition' -or
    $config.timed_c_design.run_only_gate -ne $false -or
    $config.timed_c_design.walk_landing_after_disable_allowed -ne $true -or
    $config.protected_packages.Count -ne 12) { throw 'Unexpected v7 build config' }
$gapCount = @($config.gap_chains | ForEach-Object { $_.gaps.Count } | Measure-Object -Sum)[0].Sum
if ($gapCount -ne 11) { throw 'V7 requires exactly eleven route gaps' }
$configHash = (Get-FileHash -LiteralPath $configFile -Algorithm SHA256).Hash.ToLowerInvariant()
$builderHash = (Get-FileHash -LiteralPath $scriptFile -Algorithm SHA256).Hash.ToLowerInvariant()
$sourceConfigHash = (Get-FileHash -LiteralPath $sourceConfigFile -Algorithm SHA256).Hash.ToLowerInvariant()
if ($sourceConfigHash -ne $config.source_full_config_sha256) { throw 'Source full config hash drift' }
$sourceMapFile = Join-Path $projectRoot 'Content/Naiwa/P1/Maps/L_NaiwaP1_Integrated.umap'
if (-not (Test-Path -LiteralPath $sourceMapFile -PathType Leaf)) { throw 'Integrated source map missing' }
$sourceMapHash = (Get-FileHash -LiteralPath $sourceMapFile -Algorithm SHA256).Hash.ToLowerInvariant()
if ($sourceMapHash -ne $config.source_map_sha256) { throw 'Integrated source map hash drift' }
$resolvedProjectRoot = [IO.Path]::GetFullPath($projectRoot).TrimEnd('\') + '\'
function Get-ProtectedHashes {
    $result = [ordered]@{}
    foreach ($relativePath in $config.protected_packages) {
        $target = [IO.Path]::GetFullPath((Join-Path $projectRoot $relativePath))
        if (-not $target.StartsWith($resolvedProjectRoot, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Protected target escaped project: $relativePath"
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
    schema = 'NaiwaP1IntegratedV7OuterBuildV1'
    target_map = $config.map
    source_map = $config.source_map
    status = 'running'
    started_utc = $startedUtc.ToString('o')
    config_sha256 = $configHash
    builder_sha256 = $builderHash
    source_map_sha256_before = $sourceMapHash
    protected_sha256_before = $before
    acceptance = 'pending_A_B_C_route_checkpoint_failure_and_visual_review'
}
$failure = $null
try {
    $arguments = @($projectFile, '-run=PythonScript', "-script=$scriptFile", '-unattended',
        '-nop4', '-nosplash', '-NullRHI', "-abslog=$logFile")
    & $editor @arguments
    $outer.editor_exit_code = $LASTEXITCODE
    if ($LASTEXITCODE -ne 0) { throw "P1 v7 build exited $LASTEXITCODE; inspect $logFile" }
    if (-not (Test-Path -LiteralPath $reportFile -PathType Leaf)) { throw 'Build left no report' }
    if ((Get-Item -LiteralPath $reportFile).LastWriteTimeUtc -lt $startedUtc) { throw 'Build report is stale' }
    $report = Get-Content -LiteralPath $reportFile -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($report.schema -ne $config.schema -or
        $report.target_map -ne $config.map -or
        $report.source_map -ne $config.source_map -or
        $report.phase -ne $config.phase -or
        $report.config_sha256 -ne $configHash -or
        $report.builder_sha256 -ne $builderHash -or
        $report.status -ne 'saved_v7_candidate_pending_route_and_visual_review' -or
        $report.acceptance -ne 'pending_A_B_C_route_checkpoint_failure_and_visual_review' -or
        $report.protected_packages_unchanged -ne $true -or
        $report.layout.verified_actor_count -ne 73 -or
        $report.layout.retained_teaching_actor_count -ne 14 -or
        $report.layout.extension_actor_count -ne 59 -or
        $report.layout.checkpoint_count -ne 8 -or
        $report.layout.gap_count -ne 11 -or
        $report.layout.visible_pit_hazard_count -ne 11 -or
        $report.layout.hidden_terminal_pit_hazard_count -ne 4 -or
        $report.layout.visible_pit_spike_points -ne 33 -or
        $report.layout.checkpoint_labels_enabled -ne $true -or
        $report.layout.retained_teaching_bounce_rise_cm -ne 300 -or
        @($report.layout.random_shuttle_traps_per_departure).Count -ne 2 -or
        [int]$report.layout.random_shuttle_traps_per_departure[0] -ne 1 -or
        [int]$report.layout.random_shuttle_traps_per_departure[1] -ne 2 -or
        $report.layout.shuttle_speed_profile.base_cm_s -ne 300 -or
        $report.layout.shuttle_speed_profile.variation_cm_s -ne 100 -or
        $report.layout.shuttle_speed_profile.cycles_per_crossing -ne 2 -or
        @($report.layout.shuttle_speed_profile.sampled_range_cm_s).Count -ne 2 -or
        [double]$report.layout.shuttle_speed_profile.sampled_range_cm_s[0] -ne 200 -or
        [double]$report.layout.shuttle_speed_profile.sampled_range_cm_s[1] -ne 400 -or
        @($report.layout.c_shuttle_endpoint_top_z_cm).Count -ne 2 -or
        [double]$report.layout.c_shuttle_endpoint_top_z_cm[0] -ne 240 -or
        [double]$report.layout.c_shuttle_endpoint_top_z_cm[1] -ne 340 -or
    $report.layout.timed_c_design.role -ne 'mandatory_timed_transition' -or
    $report.layout.timed_c_design.run_only_gate -ne $false -or
    $report.layout.timed_c_design.walk_landing_after_disable_allowed -ne $true -or
        $report.layout.kill_z -ne -1000 -or
        $report.layout.required_pickup_count -ne 1 -or
        $report.layout.physical_pickup_count -ne 4 -or
        $report.layout.saved_semantics.transform_reconciliation -ne 'all_owned_actors_match_config_after_save_reload') {
        throw "Unexpected v7 build report/status '$($report.status)'"
    }
    $targetMap = Join-Path $projectRoot 'Content/Naiwa/P1/Maps/L_NaiwaP1_v7.umap'
    $mapHash = (Get-FileHash -LiteralPath $targetMap -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($report.target_map_sha256_after -ne $mapHash) { throw 'Saved v7 map hash differs from builder report' }
    $outer.target_map_sha256_after = $mapHash
    $outer.status = 'saved_v7_candidate_pending_route_and_visual_review'
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
            $failure = "Protected package changed: $($changed -join ', ')"
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
Write-Output "P1 v7 candidate saved; physical route and visual gates remain pending. Report: $reportFile"
Write-Output "Independent protected-package audit: $outerReportFile"
