[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('State', 'Route', 'Hazards', 'Matrix')]
    [string]$Scenario,
    [ValidateSet('Slice', 'Full')]
    [string]$Map = 'Full',
    [ValidateSet('Run', 'Walk')]
    [string]$Gait = 'Run',
    [switch]$Capture,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$mapAsset = if ($Map -eq 'Full') { '/Game/Naiwa/P1/Maps/L_NaiwaP1_Full' } else { '/Game/Naiwa/P1/Maps/L_NaiwaP1_Slice' }
$mapName = $mapAsset.Split('/')[-1]
$mapFile = Join-Path $PSScriptRoot "NaiwaP0/Content/Naiwa/P1/Maps/$mapName.umap"
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$saved = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1'
foreach ($required in @($projectFile, $mapFile, $editor)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required file missing: $required" }
}
New-Item -ItemType Directory -Path $saved -Force | Out-Null
if ($Capture -and $Scenario -ne 'Route') { throw '-Capture requires -Scenario Route' }

$flag = switch ($Scenario) {
    'State' { '-NaiwaP1Smoke' }
    'Route' { if ($Gait -eq 'Walk') { '-NaiwaP1RouteWalk' } else { '-NaiwaP1RouteRun' } }
    'Hazards' { '-NaiwaP1AllHazards' }
    'Matrix' { '-NaiwaP1Matrix' }
}
$prefix = switch ($Scenario) { 'State' { 'smoke_' } 'Route' { 'route_' } 'Hazards' { 'hazards_' } 'Matrix' { 'matrix_' } }
$schema = switch ($Scenario) {
    'State' { 'NaiwaP1SmokeV1' }
    'Route' { if ($Map -eq 'Full') { 'NaiwaP1FullRouteV1' } else { 'NaiwaP1RouteV1' } }
    'Hazards' { 'NaiwaP1HazardAuditV1' }
    'Matrix' { 'NaiwaP1StateMatrixV1' }
}
$logFile = Join-Path $saved ("runtime_{0}_{1}_{2}.log" -f $Map.ToLowerInvariant(), $Scenario.ToLowerInvariant(), $Gait.ToLowerInvariant())
$startedUtc = [DateTime]::UtcNow
$arguments = @($projectFile, $mapAsset, '-game', '-RenderOffScreen', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', '-unattended', '-nop4', '-nosplash', $flag, '-ExecCmds=t.MaxFPS 60', "-abslog=$logFile")
if ($Capture) { $arguments += '-NaiwaP1Capture' }
& $editor @arguments
if ($LASTEXITCODE -ne 0) { throw "P1 $Map $Scenario process failed ($LASTEXITCODE); inspect $logFile" }
$candidate = Get-ChildItem -LiteralPath $saved -Filter "$prefix*.json" -File |
    Where-Object LastWriteTimeUtc -ge $startedUtc |
    Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if (-not $candidate) { throw "P1 $Map $Scenario left no fresh report; inspect $logFile" }
$report = Get-Content -LiteralPath $candidate.FullName -Raw | ConvertFrom-Json
if ($report.schema -ne $schema -or $report.map -ne $mapName) {
    throw "Unexpected P1 report in $($candidate.FullName): schema=$($report.schema), map=$($report.map)"
}
if ($Scenario -eq 'Route' -and $report.gait_speed -ne ($Gait + $(if ($Gait -eq 'Walk') { '150' } else { '300' }))) {
    throw "P1 route report gait mismatch: $($report.gait_speed)"
}
if ($report.status -ne 'PASS') { throw "P1 $Map $Scenario result=$($report.status): $($report.detail); report: $($candidate.FullName)" }
Write-Output "P1 $Map $Scenario PASS; report: $($candidate.FullName)"
