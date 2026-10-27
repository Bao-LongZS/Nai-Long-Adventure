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
$mapScript = Join-Path $projectRoot 'Scripts/p3_build_formal_maps.py'
$report = Join-Path $projectRoot 'Records/p3_formal/formal_map_build_report.json'
$log = Join-Path $projectRoot 'Records/p3_formal/formal_map_build.log'
$maps = @(
    (Join-Path $projectRoot 'NaiwaP0/Content/Naiwa/P3/Maps/L_NaiwaP3_31.umap'),
    (Join-Path $projectRoot 'NaiwaP0/Content/Naiwa/P3/Maps/L_NaiwaP3_32.umap')
)

foreach ($requiredPath in @($projectFile, $buildScript, $editorCommand, $mapScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required P3 formal-map input not found: $requiredPath" }
}
New-Item -ItemType Directory -Path (Split-Path -Parent $report) -Force | Out-Null

if (-not $SkipBuild) {
    $env:UBA_ROOT = Join-Path (Split-Path -Parent $projectFile) 'Saved/UBARoot'
    & $buildScript Nai_LongEditor Win64 Development "-Project=$projectFile" -NoUBA -NoHotReloadFromIDE -NoLiveCoding -WaitMutex -MaxParallelActions=8
    if ($LASTEXITCODE -ne 0) { throw "P3 editor build failed with exit code $LASTEXITCODE" }
}

Remove-Item -LiteralPath $report,$log -Force -ErrorAction SilentlyContinue
& $editorCommand $projectFile -run=PythonScript "-script=$mapScript" -unattended -nop4 -NullRHI -nosplash -stdout -FullStdOutLogOutput "-abslog=$log"
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $report)) {
    throw "P3 formal map creation failed; inspect $log and $report"
}

$result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
if ($result.status -ne 'formal_maps_created' -or
    $result.level_31.board_size -ne '11x9' -or $result.level_32.board_size -ne '23x17' -or
    $result.level_31_fog -ne $false -or $result.level_32_fog -ne $true) {
    throw "P3 formal map metadata did not match the approved plan: $report"
}
foreach ($mapPath in $maps) {
    if (-not (Test-Path -LiteralPath $mapPath)) { throw "Expected formal map asset not found: $mapPath" }
}
Write-Output "P3 formal maps ready: $($maps -join ', ')"
Write-Output "3-1: fixed 11x9 teaching map, no fog; 3-2: 23x17 seeded map, fog enabled."
