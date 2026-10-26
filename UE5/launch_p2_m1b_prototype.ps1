[CmdletBinding()]
param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$editorGame = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor.exe'
$mapPath = '/Game/Naiwa/P2/Maps/L_NaiwaP2_21_Prototype'

foreach ($requiredPath in @($projectFile, $editorGame)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required P2 M1b input not found: $requiredPath" }
}

Write-Output "Launching the isolated 2-1 prototype on $mapPath"
& $editorGame $projectFile $mapPath -game -windowed -ResX=1280 -ResY=720 -log
if ($LASTEXITCODE -ne 0) { throw "P2 M1b prototype exited with code $LASTEXITCODE" }
