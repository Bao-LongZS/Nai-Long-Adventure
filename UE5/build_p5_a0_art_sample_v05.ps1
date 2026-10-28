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
$mapScript = Join-Path $projectRoot 'Scripts/p5_build_a0_art_sample_v05.py'
$atlas = Join-Path $projectRoot 'SourceAssets/P5/Characters/Exports/C_Naiwa_Ski_v01_WIP.png'
$report = Join-Path $projectRoot 'Records/p5_p6_plan/evidence/P5_A0/P5_A0_EditorSample_v05_build_report.json'
$log = Join-Path $projectRoot 'Records/p5_p6_plan/evidence/P5_A0/P5_A0_EditorSample_v05_build.log'
$mapAsset = Join-Path $projectRoot 'NaiwaP0/Content/Naiwa/P5/Maps/L_NaiwaP5_A0ArtSample_v05.umap'

foreach ($requiredPath in @($projectFile, $buildScript, $editorCommand, $mapScript, $atlas)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required P5 A0 input not found: $requiredPath" }
}

if (-not $SkipBuild) {
    $env:UBA_ROOT = Join-Path (Split-Path -Parent $projectFile) 'Saved/UBARoot'
    & $buildScript Nai_LongEditor Win64 Development "-Project=$projectFile" -NoUBA -NoHotReloadFromIDE -NoLiveCoding -WaitMutex -MaxParallelActions=8
    if ($LASTEXITCODE -ne 0) { throw "P5 A0 Editor build failed with exit code $LASTEXITCODE" }
}

Remove-Item -LiteralPath $report,$log -Force -ErrorAction SilentlyContinue
& $editorCommand $projectFile -run=pythonscript "-script=$mapScript" -unattended -nop4 -nosplash -NullRHI -UTF8Output -stdout -FullStdOutLogOutput "-abslog=$log"
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $report) -or -not (Test-Path -LiteralPath $mapAsset)) {
    throw "P5 A0 sample build failed; inspect $log and $report"
}

$result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
if ($result.status -ne 'saved_pending_visible_editor_review' -or
    $result.game_mode -ne '/Script/NaiwaRuntime.NaiwaP5M2PrototypeGameMode' -or
    $result.texture_count -ne 12 -or
    $result.background_components_runtime -ne 9) {
    throw "P5 A0 sample metadata did not pass validation: $report"
}
Write-Output "P5 A0 v05 art sample map ready: $mapAsset"
Write-Output "WIP textures imported: $($result.texture_count); GameMode: $($result.game_mode)"
Write-Output "Build report: $report"
