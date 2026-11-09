[CmdletBinding()]
param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$projectFile = Join-Path $projectRoot 'NaiwaP0/Nai_Long.uproject'
$editorCommand = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$materialScript = Join-Path $projectRoot 'Scripts/p5_build_a0_art_sample_v05.py'
$report = Join-Path $projectRoot 'Records/p5_m2/P5_A0_BackgroundPlacement_materials_report_2026-10-29.json'
$log = Join-Path $projectRoot 'Records/p5_m2/P5_A0_BackgroundPlacement_materials_2026-10-29.log'

foreach ($requiredPath in @($projectFile, $editorCommand, $materialScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required P5 A0 input not found: $requiredPath" }
}

Remove-Item -LiteralPath $report,$log -Force -ErrorAction SilentlyContinue
& $editorCommand $projectFile -run=pythonscript "-script=$materialScript" -P5MaterialsOnly `
    -unattended -nop4 -nosplash -NullRHI -UTF8Output -stdout -FullStdOutLogOutput "-abslog=$log"
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $report)) {
    throw "P5 A0 v05 material repair failed; inspect $log and $report"
}

$result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
if ($result.status -ne 'materials_rebuilt_without_loading_map' -or $result.texture_count -ne 12) {
    throw "P5 A0 material repair metadata did not pass validation: $report"
}
foreach ($layer in @('far','mid','near')) {
    if ($result.background_blend_modes.$layer -notmatch 'TRANSLUCENT') {
        throw "P5 A0 $layer landscape material is not translucent: $report"
    }
}
Write-Output "P5 A0 background textures and materials rebuilt without loading a map."
Write-Output "Material report: $report"
Write-Output "Commandlet log: $log"
