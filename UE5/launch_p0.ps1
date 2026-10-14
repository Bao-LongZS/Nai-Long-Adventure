[CmdletBinding()]
param([switch]$Editor, [switch]$InputEvidence, [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$editorExecutable = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $editorExecutable)) { throw "Unreal Editor not found: $editorExecutable" }
$launchArguments = @(('"' + $projectFile + '"'), '/Game/Naiwa/Maps/L_NaiwaGraybox', '-nop4', '-nosplash')
if (-not $Editor) {
    $qualityCommands = 'sg.ViewDistanceQuality 3,sg.AntiAliasingQuality 3,sg.ShadowQuality 3,sg.GlobalIlluminationQuality 3,sg.ReflectionQuality 3,sg.PostProcessQuality 3,sg.TextureQuality 3,sg.EffectsQuality 3,sg.FoliageQuality 3,sg.ShadingQuality 3,r.ScreenPercentage 100,r.MotionBlurQuality 0,r.VSync 0,t.MaxFPS 0'
    $launchArguments += @('-game', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', ('-ExecCmds="' + $qualityCommands + '"'))
}
if ($InputEvidence) { $launchArguments += '-NaiwaInputEvidence' }
# This launcher opens an interactive game/editor for the user to see and control.
Start-Process -FilePath $editorExecutable -ArgumentList $launchArguments -WorkingDirectory (Split-Path -Parent $projectFile) -WindowStyle Normal
