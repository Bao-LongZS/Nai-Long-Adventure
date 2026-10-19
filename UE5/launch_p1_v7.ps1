[CmdletBinding()]
param(
    [switch]$Editor,
    [switch]$InputEvidence,
    [switch]$D3D11,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

# Opens the saved v7 map for a real-keyboard, visible-window playthrough.
# This launcher does not classify the playthrough as PASS or modify the map.
$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$mapName = 'L_NaiwaP1_v7'
$mapAsset = "/Game/Naiwa/P1/Maps/$mapName"
$mapFile = Join-Path $PSScriptRoot "NaiwaP0/Content/Naiwa/P1/Maps/$mapName.umap"
$editorExecutable = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor.exe'
foreach ($required in @($projectFile, $mapFile, $editorExecutable)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required v7 launch file missing: $required"
    }
}

$arguments = @(('"' + $projectFile + '"'), $mapAsset, '-nop4', '-nosplash')
if (-not $Editor) {
    $qualityCommands = 'sg.ViewDistanceQuality 3,sg.AntiAliasingQuality 3,sg.ShadowQuality 3,sg.GlobalIlluminationQuality 3,sg.ReflectionQuality 3,sg.PostProcessQuality 3,sg.TextureQuality 3,sg.EffectsQuality 3,sg.FoliageQuality 3,r.ScreenPercentage 100,r.MotionBlurQuality 0,r.VSync 0,t.MaxFPS 0'
    $arguments += @('-game', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', ('-ExecCmds="' + $qualityCommands + '"'))
}
if ($InputEvidence) { $arguments += '-NaiwaInputEvidence' }
if ($D3D11) { $arguments += '-d3d11' }

$process = Start-Process -FilePath $editorExecutable -ArgumentList $arguments `
    -WorkingDirectory (Split-Path -Parent $projectFile) -WindowStyle Normal -PassThru
Write-Output "Launched $mapAsset in visible window (PID $($process.Id))."
