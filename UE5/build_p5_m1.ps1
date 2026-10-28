[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$projectFile = Join-Path $projectRoot 'NaiwaP0/Nai_Long.uproject'
$buildScript = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
$editorCommand = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$mapScript = Join-Path $projectRoot 'Scripts/p5_build_m1_graybox_map.py'
$report = Join-Path $projectRoot 'Records/p5_m1/m1_map_build_report.json'
$log = Join-Path $projectRoot 'Records/p5_m1/m1_map_build.log'
$mapAsset = Join-Path $projectRoot 'NaiwaP0/Content/Naiwa/P5/Maps/L_NaiwaP5_M1Prototype.umap'

foreach ($requiredPath in @($projectFile, $buildScript, $editorCommand, $mapScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required P5 M1 input not found: $requiredPath" }
}
New-Item -ItemType Directory -Path (Split-Path -Parent $report) -Force | Out-Null

if (-not $SkipBuild) {
    $env:UBA_ROOT = Join-Path (Split-Path -Parent $projectFile) 'Saved/UBARoot'
    & $buildScript Nai_LongEditor Win64 Development "-Project=$projectFile" -NoUBA -NoHotReloadFromIDE -NoLiveCoding -WaitMutex -MaxParallelActions=8
    if ($LASTEXITCODE -ne 0) { throw "P5 editor build failed with exit code $LASTEXITCODE" }
}

Remove-Item -LiteralPath $report,$log -Force -ErrorAction SilentlyContinue
& $editorCommand $projectFile -run=pythonscript "-script=$mapScript" -unattended -nop4 -nosplash -NullRHI -UTF8Output -stdout -FullStdOutLogOutput "-abslog=$log"
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $report) -or -not (Test-Path -LiteralPath $mapAsset)) {
    throw "P5 M1 map creation failed; inspect $log and $report"
}

$result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
if ($result.status -notin @('new_isolated_map_saved', 'existing_owned_map_validated') -or
    $result.default_game_mode -ne '/Script/NaiwaRuntime.NaiwaP5PrototypeGameMode' -or
    $result.runtime_pawn -ne '/Script/NaiwaRuntime.NaiwaP5PrototypePawn') {
    throw "P5 M1 map metadata did not pass validation: $report"
}
Write-Output "P5 M1 prototype map ready: $mapAsset"
Write-Output "GameMode: $($result.default_game_mode); runtime pawn: $($result.runtime_pawn)"
Write-Output "Creation report: $report"
