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
$mapScript = Join-Path $projectRoot 'Scripts/p3_build_m2_scenario_test_map.py'
$report = Join-Path $projectRoot 'Records/p3_m2/scenario_test_map_report.json'
$log = Join-Path $projectRoot 'Records/p3_m2/scenario_test_map_create.log'
$mapAsset = Join-Path $projectRoot 'NaiwaP0/Content/Naiwa/P3/Maps/L_NaiwaP3_M2ScenarioTest.umap'
$portraitAssets = @(
    'SourceAssets/P3/Portraits/Processed/SPR_Player_Neutral_A.png',
    'SourceAssets/P3/Portraits/Processed/SPR_Naicha_Neutral_A.png',
    'SourceAssets/P3/Portraits/Processed/SPR_Naisha_Rage_A.png',
    'SourceAssets/P3/Portraits/Processed/SPR_Naihao_Neutral_A.png'
) | ForEach-Object { Join-Path $projectRoot $_ }

foreach ($requiredPath in @($projectFile, $buildScript, $editorCommand, $mapScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required M2 test-map input not found: $requiredPath" }
}
foreach ($portraitPath in $portraitAssets) {
    if (-not (Test-Path -LiteralPath $portraitPath)) { throw "Required approved P3 portrait source not found: $portraitPath" }
}
New-Item -ItemType Directory -Path (Split-Path -Parent $report) -Force | Out-Null

if (-not $SkipBuild) {
    $env:UBA_ROOT = Join-Path (Split-Path -Parent $projectFile) 'Saved/UBARoot'
    & $buildScript Nai_LongEditor Win64 Development "-Project=$projectFile" -NoUBA -WaitMutex -MaxParallelActions=8
    if ($LASTEXITCODE -ne 0) { throw "M2 editor build failed with exit code $LASTEXITCODE" }
}

Remove-Item -LiteralPath $report,$log -Force -ErrorAction SilentlyContinue
& $editorCommand $projectFile -run=PythonScript "-script=$mapScript" -unattended -nop4 -NullRHI -nosplash -stdout -FullStdOutLogOutput "-abslog=$log"
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $report) -or -not (Test-Path -LiteralPath $mapAsset)) {
    throw "M2 scenario map creation failed; inspect $log and $report"
}

$result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
if ($result.status -notin @('new_isolated_scenario_map_saved', 'existing_owned_scenario_map_updated') -or
    $result.default_game_mode -ne '/Script/NaiwaRuntime.NaiwaP3M2GameMode' -or
    $result.layout_seed -ne 320000) {
    throw "M2 scenario map metadata did not pass validation: $report"
}
Write-Output "M2 scenario test map ready: $mapAsset"
Write-Output "GameMode: $($result.default_game_mode); seed: $($result.layout_seed); board: $($result.board_size)"
Write-Output "Creation report: $report"
