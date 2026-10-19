[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Run', 'Walk')]
    [string]$Gait,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8',
    [switch]$Capture
)

$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$mapFile = Join-Path $PSScriptRoot 'NaiwaP0/Content/Naiwa/P1/Maps/L_NaiwaP1_Switch.umap'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$saved = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1'
foreach ($required in @($projectFile, $mapFile, $editor)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required file missing: $required" }
}
New-Item -ItemType Directory -Path $saved -Force | Out-Null
$flag = if ($Gait -eq 'Walk') { '-NaiwaP1SwitchProbeWalk' } else { '-NaiwaP1SwitchProbeRun' }
$logFile = Join-Path $saved ("switch_probe_{0}_{1}.log" -f $Gait.ToLowerInvariant(), [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))
$startedUtc = [DateTime]::UtcNow
$arguments = @($projectFile, '/Game/Naiwa/P1/Maps/L_NaiwaP1_Switch', '-game', '-RenderOffScreen', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', '-unattended', '-nop4', '-nosplash', $flag, '-ExecCmds=t.MaxFPS 60', "-abslog=$logFile")
if ($Capture) { $arguments += @('-d3d11', '-NaiwaP1SwitchCapture') }
& $editor @arguments
if ($LASTEXITCODE -ne 0) { throw "P1 Switch $Gait probe failed ($LASTEXITCODE); inspect $logFile" }
$candidate = Get-ChildItem -LiteralPath $saved -Filter 'switch_probe_*.json' -File |
    Where-Object LastWriteTimeUtc -ge $startedUtc |
    Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if (-not $candidate) { throw "P1 Switch $Gait probe left no fresh report; inspect $logFile" }
$report = Get-Content -LiteralPath $candidate.FullName -Raw | ConvertFrom-Json
$expectedGait = if ($Gait -eq 'Walk') { 'Walk150' } else { 'Run300' }
if ($report.schema -ne 'NaiwaP1SwitchProbeV1' -or $report.map -ne 'L_NaiwaP1_Switch' -or $report.gait_speed -ne $expectedGait) {
    throw "Unexpected P1 Switch report in $($candidate.FullName)"
}
if ($report.status -ne 'PASS') { throw "P1 Switch $Gait result=$($report.status): $($report.detail); report: $($candidate.FullName)" }
if (-not $report.rider_supported_midway -or -not $report.death_reset_passed -or -not $report.r_reset_passed -or $report.ended_while_paused -or $report.paused_platform_delta_x -gt 1.0) {
    throw "P1 Switch $Gait report omitted a required behavior: $($candidate.FullName)"
}
$requiredEvents = @(
    'real_walk_boarded_and_stopped_on_platform',
    'stable_boarding_started_visible_departure_cue',
    'rider_supported_midway_and_arrived_at_right_dock',
    'right_dock_held_without_repeat_then_left_switch_called_back',
    'opposite_switch_touch_ignored_during_leftward_travel',
    'right_switch_reentry_called_platform_right_once',
    'travel_froze_during_pause_and_resumed',
    'death_reset_restored_left_dock_and_right_switch_started_new_trip',
    'r_reset_restored_playable_start_and_left_dock'
)
foreach ($event in $requiredEvents) {
    if ($report.events -notcontains $event) { throw "P1 Switch report missing '$event': $($candidate.FullName)" }
}
if ($Capture) {
    if ($report.capture_paths.Count -lt 2) { throw "P1 Switch capture left fewer than two frames: $($candidate.FullName)" }
    foreach ($path in $report.capture_paths) {
        $imageFile = Join-Path (Join-Path $saved 'Visuals') (Split-Path -Leaf $path)
        if (-not (Test-Path -LiteralPath $imageFile -PathType Leaf)) { throw "P1 Switch capture missing: $imageFile" }
    }
}
Write-Output "P1 Switch $Gait PASS; report: $($candidate.FullName)"
