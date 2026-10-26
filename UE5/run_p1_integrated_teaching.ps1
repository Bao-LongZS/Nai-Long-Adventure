[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Walk', 'Run')]
    [string]$Gait,
    [switch]$CheckpointFailure,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$mapFile = Join-Path $PSScriptRoot 'NaiwaP0/Content/Naiwa/P1/Maps/L_NaiwaP1_Integrated.umap'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$saved = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1'
foreach ($required in @($projectFile, $mapFile, $editor)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required file missing: $required" }
}
New-Item -ItemType Directory -Path $saved -Force | Out-Null
$flag = if ($Gait -eq 'Walk') { '-NaiwaP1IntegratedProbeWalk' } else { '-NaiwaP1IntegratedProbeRun' }
$scenario = if ($CheckpointFailure) { 'checkpoint_failure' } else { 'route' }
$logFile = Join-Path $saved ("integrated_teaching_{0}_{1}_{2}.log" -f $Gait.ToLowerInvariant(), $scenario, [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))
$startedUtc = [DateTime]::UtcNow
$arguments = @($projectFile, '/Game/Naiwa/P1/Maps/L_NaiwaP1_Integrated', '-game', '-RenderOffScreen', '-windowed', '-ResX=1600', '-ResY=900', '-ForceRes', '-unattended', '-nop4', '-nosplash', '-NaiwaP1Standalone', $flag, '-ExecCmds=t.MaxFPS 60', "-abslog=$logFile")
if ($CheckpointFailure) { $arguments += '-NaiwaP1IntegratedProbeCheckpointFail' }
& $editor @arguments
if ($LASTEXITCODE -ne 0) { throw "Integrated teaching $Gait $scenario process failed ($LASTEXITCODE); inspect $logFile" }
$candidate = Get-ChildItem -LiteralPath $saved -Filter 'integrated_teaching_probe_*.json' -File |
    Where-Object LastWriteTimeUtc -ge $startedUtc |
    Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if (-not $candidate) { throw "Integrated teaching $Gait $scenario left no fresh report; inspect $logFile" }
$report = Get-Content -LiteralPath $candidate.FullName -Raw | ConvertFrom-Json
$expectedGait = if ($Gait -eq 'Walk') { 'Walk150' } else { 'Run300' }
if ($report.schema -ne 'NaiwaP1IntegratedTeachingProbeV1' -or
    $report.map_package -ne '/Game/Naiwa/P1/Maps/L_NaiwaP1_Integrated' -or
    $report.scope -ne 'P1-5b_partial_teaching_slice' -or
    $report.gait_speed -ne $expectedGait -or
    $report.checkpoint_failure_requested -ne [bool]$CheckpointFailure -or
    $report.full_p1_5b_acceptance -ne $false) {
    throw "Unexpected integrated teaching report: $($candidate.FullName)"
}
if ($report.status -ne 'PASS') { throw "Integrated teaching $Gait $scenario result=$($report.status): $($report.detail); report: $($candidate.FullName)" }
foreach ($field in @('strict_preflight_passed', 'cue_no_collision_verified', 'hidden_hazard_jumped_and_grounded', 'left_switch_touched',
    'right_switch_touched', 'real_shuttle_boarding', 'supported_departure_cue_seen',
    'supported_mid_gap_ride_seen', 'supported_right_dock_reached', 'real_upward_pad_launch_seen',
    'first_high_platform_floor_contact_seen', 'checkpoint_grounded_route_complete')) {
    if (-not $report.$field) { throw "Integrated teaching report missing ${field}: $($candidate.FullName)" }
}
if ($report.pad_activations_delta -ne 1 -or $report.probe_character_teleports -ne 0 -or
    $report.direct_trigger_injections -ne 0 -or $report.platform_control_injections -ne 0 -or
    $report.final_state -ne 'Playing') {
    throw "Integrated teaching event/locomotion contract failed: $($candidate.FullName)"
}
if ($CheckpointFailure) {
    if ($report.death_events_delta -ne 1 -or $report.production_respawn_placements -ne 1 -or
        -not $report.natural_unsupported_fall_seen -or -not $report.world_kill_z_dying_seen -or
        -not $report.checkpoint_respawn_grounded_playable) {
        throw "Integrated teaching checkpoint recovery failed: $($candidate.FullName)"
    }
} elseif ($report.death_events_delta -ne 0 -or $report.production_respawn_placements -ne 0) {
    throw "Integrated teaching ordinary route had an unexpected death or respawn: $($candidate.FullName)"
}
Write-Output "Integrated teaching $Gait $scenario PASS; report: $($candidate.FullName)"
