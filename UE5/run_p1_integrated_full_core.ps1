[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Walk', 'Run')]
    [string]$Gait,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

# Candidate wrapper: deploy at UE5/run_p1_integrated_full_core.ps1 after review.
# This is one fresh-process, automated core route. It supplies neither real-keyboard
# nor camera/visual nor checkpoint-death/recovery nor full P1-5b acceptance evidence.
$ErrorActionPreference = 'Stop'
$expectedMap = '/Game/Naiwa/P1/Maps/L_NaiwaP1_Integrated'
$expectedConfigHash = '3aad9c28e6a88397aeb6e1652559fb8d60496185fe255633ab259d7e72f2aa5d'
$expectedMapHash = 'ee5c82788fa2290089f27f4021f1f8539f9bfa979cf357910a4eabcdde73a350'
$expectedGait = if ($Gait -eq 'Walk') { 'Walk150' } else { 'Run300' }
$projectRoot = Join-Path $PSScriptRoot 'NaiwaP0'
$projectFile = Join-Path $projectRoot 'Nai_Long.uproject'
$mapFile = Join-Path $projectRoot 'Content/Naiwa/P1/Maps/L_NaiwaP1_Integrated.umap'
$configFile = Join-Path $PSScriptRoot 'Scripts/p1_integrated_full_config_v1.json'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$saved = Join-Path $projectRoot 'Saved/NaiwaP1'

function Assert-FullCoreReport {
    param([Parameter(Mandatory = $true)]$Report, [string]$ExpectedGait,
          [string]$ExpectedMap, [string]$ExpectedConfigHash)
    if ($Report.schema -ne 'NaiwaP1IntegratedFullCoreRouteV1' -or
        $Report.scope -ne 'P1-5b_full_candidate_core_route_only' -or
        $Report.map -ne 'L_NaiwaP1_Integrated' -or $Report.map_package -ne $ExpectedMap -or
        $Report.bound_config_sha256 -ne $ExpectedConfigHash -or $Report.gait_speed -ne $ExpectedGait -or
        $Report.full_p1_5b_acceptance -ne $false -or
        $Report.real_keyboard_input_evidence -ne $false -or $Report.screenshots_in_this_process -ne $false) {
        throw 'Core route identity/scope/map/config/gait or evidence boundary mismatch'
    }
    if ($Report.status -ne 'PASS') { throw "Core route result=$($Report.status): $($Report.detail)" }
    if ($Report.strict_preflight_passed -ne $true -or
        $Report.required_pickup_physically_collected -ne $true -or $Report.pickup_count -ne 1 -or
        $Report.final_state -ne 'Won' -or $Report.final_step -ne 22 -or $Report.ordinary_jump_calls -ne 2) {
        throw 'Core route did not finish all stages, physical pickup and Won with exactly two hazard jumps'
    }
    foreach ($field in @('death_events', 'probe_character_teleports', 'direct_trigger_injections',
                         'platform_control_injections', 'production_respawn_placements')) {
        if ($Report.$field -ne 0) { throw "Core route requires zero ${field}" }
    }
    if ($Report.control_method -ne 'SetP1Gait; SetAutomatedMoveInput; Jump/StopJumping only at the two hidden hazards') {
        throw 'Unexpected automated movement method'
    }
    if (@($Report.sections).Count -ne 2 -or @($Report.checkpoints_physically_reached).Count -ne 3 -or
        @($Report.checkpoints_physically_reached | Where-Object { $_ -ne $true }).Count -ne 0) {
        throw 'Two mechanism sections and three physically reached checkpoints are required'
    }
    $sectionNames = @($Report.sections | ForEach-Object { $_.section })
    if ($sectionNames[0] -ne 'teaching' -or $sectionNames[1] -ne 'combination') {
        throw 'Teaching/combination section identity mismatch'
    }
    foreach ($section in $Report.sections) {
        foreach ($field in @('hidden_hazard_jump_and_floor_landing', 'real_shuttle_boarding',
                             'supported_departure_cue', 'supported_mid_gap_ride', 'supported_right_dock',
                             'real_upward_pad_launch', 'actual_target_floor_landing',
                             'timed_actual_floor_support', 'timed_countdown_declined', 'timed_safe_bank_reached')) {
            if ($section.$field -ne $true) { throw "Section $($section.section) lacks ${field}" }
        }
        if ($section.pad_activation_count -ne 1) { throw "Section $($section.section) must activate its pad exactly once" }
        foreach ($field in @('timed_trigger_world_time_s', 'timed_trigger_to_safe_bank_s', 'timed_remaining_at_safe_bank_s')) {
            if ($null -eq $section.$field -or [double]::IsNaN([double]$section.$field) -or
                [double]::IsInfinity([double]$section.$field)) { throw "Section $($section.section) has invalid ${field}" }
        }
        if ($section.timed_trigger_world_time_s -lt 0 -or $section.timed_trigger_to_safe_bank_s -le 0 -or
            $section.timed_remaining_at_safe_bank_s -le 0.05 -or
            [Math]::Abs($section.timed_trigger_to_safe_bank_s + $section.timed_remaining_at_safe_bank_s - 4.3) -gt 0.11) {
            throw "Section $($section.section) timed crossing lacks a positive measured exit margin or a consistent 4.3s clock"
        }
    }
    $pending = @('each_checkpoint_natural_failure_and_recovery', 'restart_and_pause_matrix',
                 'both_side_recall_after_failure', 'ordinary_window_real_keyboard',
                 'visual_camera_readability', 'bounce_bypass_assessment')
    if (@($Report.outside_this_core_route).Count -ne $pending.Count -or
        @($pending | Where-Object { $_ -notin @($Report.outside_this_core_route) }).Count -ne 0) {
        throw 'Core report must explicitly preserve the six pending acceptance areas'
    }
    $stations = @($Report.stations)
    if ($stations.Count -lt 23 -or $stations[0].station -ne 'full_config_and_unique_actor_preflight_grounded_spawn' -or
        $stations[-1].station -ne 'full_probe_finished') { throw 'Missing fresh full-route station sequence' }
    $previousStep = 1
    $previousWall = -1.0
    $observedSteps = @()
    foreach ($station in $stations) {
        if ($null -eq $station.step -or $station.step -lt $previousStep -or $station.step -gt 22 -or
            $null -eq $station.wall_time_s -or $station.wall_time_s -lt $previousWall -or
            $station.death_events -ne 0 -or $station.pickup_count -lt 0 -or $station.pickup_count -gt 1) {
            throw 'Stage/time ordering or station event counts are invalid'
        }
        $previousStep = $station.step
        $previousWall = $station.wall_time_s
        $observedSteps += $station.step
    }
    foreach ($step in 1..22) { if ($step -notin $observedSteps) { throw "Core route did not record stage $step" } }
    if ($stations[-1].state -ne 'Won' -or $stations[-1].pickup_count -ne 1 -or
        $stations[-1].checkpoint_id -ne 'IntegratedCheckpoint03') { throw 'Final station lacks the final checkpoint, pickup and Won' }
    foreach ($checkpointId in @('IntegratedCheckpoint01', 'IntegratedCheckpoint02', 'IntegratedCheckpoint03')) {
        if (@($stations | Where-Object { $_.checkpoint_id -eq $checkpointId -and $_.grounded -eq $true }).Count -eq 0) {
            throw "No grounded physical station for $checkpointId"
        }
    }
}

foreach ($required in @($projectFile, $mapFile, $configFile, $editor)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required deployed file missing: $required" }
}
$config = Get-Content -LiteralPath $configFile -Raw -Encoding UTF8 | ConvertFrom-Json
if ($config.schema -ne 'NaiwaP1IntegratedFullV1' -or $config.map -ne $expectedMap -or
    $config.phase -ne 'P1-5b_full_candidate' -or
    (Get-FileHash -LiteralPath $configFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedConfigHash -or
    (Get-FileHash -LiteralPath $mapFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedMapHash) {
    throw 'The deployed map/config differs from the frozen full candidate; review and rebind the wrapper before running'
}
$resolvedProjectRoot = [IO.Path]::GetFullPath($projectRoot).TrimEnd('\') + '\'
function Get-CoreProtectedHashes {
    $result = [ordered]@{}
    foreach ($relativePath in @($config.protected_packages) + @('Content/Naiwa/P1/Maps/L_NaiwaP1_Integrated.umap')) {
        $target = [IO.Path]::GetFullPath((Join-Path $projectRoot $relativePath))
        if (-not $target.StartsWith($resolvedProjectRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Protected path escaped the project' }
        if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { throw "Protected package missing: $target" }
        $result[$relativePath] = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $result['../Scripts/p1_integrated_full_config_v1.json'] = (Get-FileHash -LiteralPath $configFile -Algorithm SHA256).Hash.ToLowerInvariant()
    return $result
}
$before = Get-CoreProtectedHashes
New-Item -ItemType Directory -Path $saved -Force | Out-Null
$stamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ') + '_' + [Guid]::NewGuid().ToString('N')
$logFile = Join-Path $saved ("integrated_full_core_{0}_{1}.log" -f $Gait.ToLowerInvariant(), $stamp)
$outerFile = Join-Path $saved ("integrated_full_core_outer_{0}_{1}.json" -f $Gait.ToLowerInvariant(), $stamp)
$existingReports = @(Get-ChildItem -LiteralPath $saved -Filter 'integrated_full_core_route_*.json' -File | ForEach-Object FullName)
$startedUtc = [DateTime]::UtcNow
$outer = [ordered]@{
    schema = 'NaiwaP1IntegratedFullCoreOuterV1'; status = 'running'; scope = 'P1-5b_full_candidate_core_route_only'
    gait_speed = $expectedGait; map_package = $expectedMap; config_sha256 = $expectedConfigHash
    map_sha256 = $expectedMapHash; started_utc = $startedUtc.ToString('o'); log_file = $logFile
    protected_sha256_before = $before; full_p1_5b_acceptance = $false
    runtime_marker_validation = 'strict_preflight_passed checks exact base/full config markers and rejects partial marker; markers are not independently exported by this schema'
    expected_full_config_tag = 'NaiwaP1IntegratedFullCfg_3aad9c28e6a88397'
    real_keyboard_input_evidence = $false; visual_acceptance = $false
}
$failure = $null
$classification = 'infrastructure_failure'
try {
    $flag = if ($Gait -eq 'Walk') { '-NaiwaP1IntegratedFullProbeWalk' } else { '-NaiwaP1IntegratedFullProbeRun' }
    $arguments = @($projectFile, $expectedMap, '-game', '-RenderOffScreen', '-windowed', '-ResX=1600', '-ResY=900',
                   '-ForceRes', '-unattended', '-nop4', '-nosplash', '-NaiwaP1Standalone', $flag, '-ExecCmds=t.MaxFPS 60', "-abslog=$logFile")
    & $editor @arguments
    $outer.editor_exit_code = $LASTEXITCODE
    if ($LASTEXITCODE -ne 0) { throw "Core route process exited $LASTEXITCODE; inspect $logFile" }
    $classification = 'capture_failure'
    $fresh = @(Get-ChildItem -LiteralPath $saved -Filter 'integrated_full_core_route_*.json' -File |
        Where-Object { $_.FullName -notin $existingReports -and $_.LastWriteTimeUtc -ge $startedUtc })
    if ($fresh.Count -ne 1) { throw "Expected exactly one new fresh core report, found $($fresh.Count); inspect $logFile" }
    $report = Get-Content -LiteralPath $fresh[0].FullName -Raw -Encoding UTF8 | ConvertFrom-Json
    $outer.report_file = $fresh[0].FullName
    $outer.report_sha256 = (Get-FileHash -LiteralPath $fresh[0].FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $outer.raw_result = $report.status
    $classification = if ($report.status -eq 'FAIL') { 'gameplay_defect' } else { 'fixture_invalid' }
    Assert-FullCoreReport -Report $report -ExpectedGait $expectedGait -ExpectedMap $expectedMap -ExpectedConfigHash $expectedConfigHash
    $outer.status = 'PASS'; $outer.classification = 'core_route_pass_only'
    $outer.final_state = $report.final_state; $outer.sections = $report.sections
    $outer.outside_this_core_route = $report.outside_this_core_route
} catch {
    $failure = $_; $outer.status = 'INCOMPLETE'; $outer.classification = $classification; $outer.error = $_.ToString()
    if ($classification -eq 'gameplay_defect') { $outer.status = 'FAIL' }
} finally {
    try {
        $after = Get-CoreProtectedHashes
        $outer.protected_sha256_after = $after
        $changed = @($before.Keys | Where-Object { $before[$_] -ne $after[$_] })
        $outer.protected_packages_unchanged = $changed.Count -eq 0
        if ($changed.Count -gt 0) {
            $outer.status = 'FAIL'; $outer.classification = 'protected_asset_changed'; $outer.changed_packages = $changed
            $failure = 'A protected package or config changed during the core route'
        }
    } catch {
        $outer.status = 'INCOMPLETE'; $outer.classification = 'infrastructure_failure'; $outer.hash_audit_error = $_.ToString(); $failure = $_
    }
    $outer.finished_utc = [DateTime]::UtcNow.ToString('o')
    $outer | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $outerFile -Encoding UTF8
}
if ($failure) { throw $failure }
Write-Output "Integrated full $expectedGait core route PASS; report: $($outer.report_file)"
Write-Output "Protected-hash audit/core-only summary: $outerFile"
Write-Output 'Checkpoint failure/recovery, Pause/R, recall after failure, real keyboard, visuals and bounce bypass remain pending.'
