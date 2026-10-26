[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Run', 'Walk')]
    [string]$Gait,
    [ValidateSet('Mechanics', 'NaturalFailure')]
    [string]$Scenario = 'Mechanics',
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8',
    [switch]$Capture
)

$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$mapFile = Join-Path $PSScriptRoot 'NaiwaP0/Content/Naiwa/P1/Maps/L_NaiwaP1_Timed.umap'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$saved = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1'
foreach ($required in @($projectFile, $mapFile, $editor)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required file missing: $required" }
}
if ($Scenario -eq 'NaturalFailure' -and $Capture) { throw 'Capture is supported by the Mechanics scenario only' }
New-Item -ItemType Directory -Path $saved -Force | Out-Null
$walk = $Gait -eq 'Walk'
$expectedGait = if ($walk) { 'Walk150' } else { 'Run300' }
if ($Scenario -eq 'Mechanics') {
    $flag = if ($walk) { '-NaiwaP1TimedProbeWalk' } else { '-NaiwaP1TimedProbeRun' }
    $pattern = 'timed_probe_*.json'
    $schema = 'NaiwaP1TimedProbeV1'
} else {
    $flag = if ($walk) { '-NaiwaP1TimedFailProbeWalk' } else { '-NaiwaP1TimedFailProbeRun' }
    $pattern = 'timed_fail_probe_*.json'
    $schema = 'NaiwaP1TimedFailProbeV1'
}
$logFile = Join-Path $saved ("timed_{0}_{1}_{2}.log" -f $Scenario.ToLowerInvariant(), $Gait.ToLowerInvariant(), [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))
$startedUtc = [DateTime]::UtcNow
$arguments = @($projectFile, '/Game/Naiwa/P1/Maps/L_NaiwaP1_Timed', '-game', '-RenderOffScreen', '-windowed', '-ResX=1600', '-ResY=900', '-ForceRes', '-unattended', '-nop4', '-nosplash', '-NaiwaP1Standalone', $flag, '-ExecCmds=t.MaxFPS 60', "-abslog=$logFile")
if ($Capture) { $arguments += @('-d3d11', '-NaiwaP1TimedCapture') }
& $editor @arguments
if ($LASTEXITCODE -ne 0) { throw "P1 Timed $Scenario $Gait probe failed ($LASTEXITCODE); inspect $logFile" }
$candidate = Get-ChildItem -LiteralPath $saved -Filter $pattern -File |
    Where-Object LastWriteTimeUtc -ge $startedUtc |
    Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if (-not $candidate) { throw "P1 Timed $Scenario $Gait probe left no fresh report; inspect $logFile" }
$report = Get-Content -LiteralPath $candidate.FullName -Raw | ConvertFrom-Json
if ($report.schema -ne $schema -or $report.map -ne 'L_NaiwaP1_Timed' -or $report.gait_speed -ne $expectedGait) {
    throw "Unexpected P1 Timed report in $($candidate.FullName)"
}
if ($report.status -ne 'PASS') { throw "P1 Timed $Scenario $Gait result=$($report.status): $($report.detail); report: $($candidate.FullName)" }
if ($Scenario -eq 'Mechanics') {
    foreach ($field in @('first_real_crossing_passed', 'retry_real_crossing_passed', 'standing_did_not_extend_countdown', 'pause_freeze_passed', 'fell_after_disable', 'occupied_recovery_guard_passed', 'r_reset_passed', 'death_reset_passed')) {
        if (-not $report.$field) { throw "P1 Timed $Gait missing $field`: $($candidate.FullName)" }
    }
    if ($report.ended_while_paused -or $report.paused_countdown_delta_s -gt 0.03) {
        throw "P1 Timed $Gait pause check failed: $($candidate.FullName)"
    }
    if ($Capture -and -not $report.all_requested_captures_exist) {
        throw "P1 Timed $Gait captures missing: $($candidate.FullName)"
    }
} else {
    foreach ($field in @('fell_after_disable', 'saw_dying', 'respawn_playable_and_floor_ready', 'real_retry_crossed')) {
        if (-not $report.$field) { throw "P1 Timed $Gait natural failure missing $field`: $($candidate.FullName)" }
    }
    if ($report.death_events_delta -ne 1 -or $report.probe_direct_teleports -ne 0 -or $report.world_kill_z -ne -1000) {
        throw "P1 Timed $Gait natural failure counts invalid: $($candidate.FullName)"
    }
}
Write-Output "P1 Timed $Scenario $Gait PASS; report: $($candidate.FullName)"
