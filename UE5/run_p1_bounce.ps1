[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Run', 'Walk')]
    [string]$Gait,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$mapAsset = '/Game/Naiwa/P1/Maps/L_NaiwaP1_Bounce'
$mapFile = Join-Path $PSScriptRoot 'NaiwaP0/Content/Naiwa/P1/Maps/L_NaiwaP1_Bounce.umap'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$saved = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1'
foreach ($required in @($projectFile, $mapFile, $editor)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required file missing: $required" }
}
New-Item -ItemType Directory -Path $saved -Force | Out-Null
$flag = if ($Gait -eq 'Walk') { '-NaiwaP1BounceProbeWalk' } else { '-NaiwaP1BounceProbeRun' }
$logFile = Join-Path $saved ("bounce_probe_{0}_{1}.log" -f $Gait.ToLowerInvariant(), [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))
$startedUtc = [DateTime]::UtcNow
$arguments = @($projectFile, $mapAsset, '-game', '-RenderOffScreen', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', '-unattended', '-nop4', '-nosplash', $flag, '-ExecCmds=t.MaxFPS 60', "-abslog=$logFile")
& $editor @arguments
if ($LASTEXITCODE -ne 0) { throw "P1 Bounce $Gait probe failed ($LASTEXITCODE); inspect $logFile" }
$candidate = Get-ChildItem -LiteralPath $saved -Filter 'bounce_probe_*.json' -File |
    Where-Object LastWriteTimeUtc -ge $startedUtc |
    Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if (-not $candidate) { throw "P1 Bounce $Gait probe left no fresh report; inspect $logFile" }
$report = Get-Content -LiteralPath $candidate.FullName -Raw | ConvertFrom-Json
if ($report.schema -ne 'NaiwaP1BounceProbeV1' -or $report.map -ne 'L_NaiwaP1_Bounce' -or $report.gait_speed -ne ($Gait + $(if ($Gait -eq 'Walk') { '150' } else { '300' }))) {
    throw "Unexpected P1 Bounce report in $($candidate.FullName)"
}
if ($report.status -ne 'PASS') { throw "P1 Bounce $Gait result=$($report.status): $($report.detail); report: $($candidate.FullName)" }
$requiredEvents = @('first_contact_activated_once', 'r_reset_cleared_pad_state_and_restored_start', 'post_reset_contact_activated_once', 'landing_hazard_caused_one_death', 'death_reset_cleared_pad_state_and_restored_start')
foreach ($event in $requiredEvents) {
    if ($report.events -notcontains $event) { throw "P1 Bounce report missing '$event': $($candidate.FullName)" }
}
Write-Output "P1 Bounce $Gait PASS; report: $($candidate.FullName)"
