[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Walk', 'Run')]
    [string]$Gait,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$cursor = (Resolve-Path -LiteralPath $PSScriptRoot).Path
while (-not (Test-Path -LiteralPath (Join-Path $cursor 'UE5/NaiwaP0/Nai_Long.uproject') -PathType Leaf)) {
    $parent = Split-Path -Path $cursor -Parent
    if (-not $parent -or $parent -eq $cursor) { throw 'Could not locate Nai-Long-Adventure project root' }
    $cursor = $parent
}
$ue = Join-Path $cursor 'UE5'
$projectFile = Join-Path $ue 'NaiwaP0/Nai_Long.uproject'
$mapFile = Join-Path $ue 'NaiwaP0/Content/Naiwa/P1/Maps/L_NaiwaP1_Integrated.umap'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$saved = Join-Path $ue 'NaiwaP0/Saved/NaiwaP1'
$visualDir = Join-Path $saved 'Visuals'
foreach ($required in @($projectFile, $mapFile, $editor)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required file missing: $required" }
}
New-Item -ItemType Directory -Path $saved -Force | Out-Null
$flag = if ($Gait -eq 'Walk') { '-NaiwaP1IntegratedVisualWalk' } else { '-NaiwaP1IntegratedVisualRun' }
$logFile = Join-Path $saved ("integrated_teaching_visual_{0}_{1}.log" -f $Gait.ToLowerInvariant(), [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))
$startedUtc = [DateTime]::UtcNow
# Deliberately a separate visual process: no functional-probe flag and no HighResShot.
$arguments = @($projectFile, '/Game/Naiwa/P1/Maps/L_NaiwaP1_Integrated', '-game', '-RenderOffScreen',
    '-d3d11', '-windowed', '-ResX=1280', '-ResY=720', '-ForceRes', '-unattended', '-nop4',
    '-NaiwaP1Standalone',
    '-nosplash', $flag, '-ExecCmds=t.MaxFPS 60', "-abslog=$logFile")
& $editor @arguments
if ($LASTEXITCODE -ne 0) { throw "Integrated visual $Gait process failed ($LASTEXITCODE); inspect $logFile" }
$candidate = Get-ChildItem -LiteralPath $saved -Filter 'integrated_teaching_visual_*.json' -File |
    Where-Object LastWriteTimeUtc -ge $startedUtc |
    Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if (-not $candidate) { throw "Integrated visual $Gait left no fresh manifest; inspect $logFile" }
$report = Get-Content -LiteralPath $candidate.FullName -Raw | ConvertFrom-Json
$expectedGait = if ($Gait -eq 'Walk') { 'Walk150' } else { 'Run300' }
if ($report.schema -ne 'NaiwaP1IntegratedTeachingVisualV1' -or
    $report.map_package -ne '/Game/Naiwa/P1/Maps/L_NaiwaP1_Integrated' -or
    $report.scope -ne 'P1-5b_partial_teaching_slice_visual_only' -or
    $report.gait_speed -ne $expectedGait -or
    $report.visual_only -ne $true -or $report.functional_probe_evidence -ne $false -or
    $report.full_p1_5b_acceptance -ne $false) {
    throw "Unexpected visual manifest: $($candidate.FullName)"
}
if ($report.status -ne 'CAPTURE_COMPLETE' -or -not $report.all_requested_captures_exist -or
    -not $report.checkpoint_reached -or $report.frames.Count -ne 6) {
    throw "Integrated visual $Gait incomplete: $($report.detail); manifest: $($candidate.FullName)"
}
$expectedLabels = @('spawn_hazard_hint', 'shuttle_departure_cue', 'shuttle_mid_gap',
    'bounce_near_apex', 'bounce_safe_landing', 'checkpoint_grounded')
$visualPrefix = [System.IO.Path]::GetFullPath($visualDir).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
foreach ($label in $expectedLabels) {
    $matching = @($report.frames | Where-Object label -eq $label)
    if ($matching.Count -ne 1 -or -not $matching[0].requested -or -not $matching[0].saved) {
        throw "Missing visual frame '$label': $($candidate.FullName)"
    }
    $framePath = [System.IO.Path]::GetFullPath([string]$matching[0].file)
    if (-not $framePath.StartsWith($visualPrefix, [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $framePath -PathType Leaf) -or
        (Get-Item -LiteralPath $framePath).Length -le 0) {
        throw "Invalid visual frame '$label': $framePath"
    }
}
Write-Output "Integrated visual $Gait CAPTURE_COMPLETE; manifest: $($candidate.FullName)"
