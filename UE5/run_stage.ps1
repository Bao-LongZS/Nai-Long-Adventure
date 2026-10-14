[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Import', 'RunImport', 'Visuals', 'Build', 'Setup', 'Audit', 'RunAudit', 'Reimport', 'Test', 'Record', 'ClipReview', 'ClipPlayback')]
    [string]$Stage,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(0, 240)]
    [int]$FrameRateCap = 0,
    [switch]$Offscreen
)
$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$recordDirectory = Join-Path $PSScriptRoot 'Records'
New-Item -ItemType Directory -Path $recordDirectory -Force | Out-Null
$editorCommand = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
if ($Stage -eq 'Build') {
    & (Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat') Nai_LongEditor Win64 Development "-Project=$projectFile" -WaitMutex
} elseif ($Stage -in @('Test', 'Record', 'ClipReview', 'ClipPlayback')) {
    $qualityCommands = 'sg.ViewDistanceQuality 3,sg.AntiAliasingQuality 3,sg.ShadowQuality 3,sg.GlobalIlluminationQuality 3,sg.ReflectionQuality 3,sg.PostProcessQuality 3,sg.TextureQuality 3,sg.EffectsQuality 3,sg.FoliageQuality 3,sg.ShadingQuality 3,r.ScreenPercentage 100,r.MotionBlurQuality 0,r.VSync 0,t.MaxFPS 0'
    $qualityCommands = $qualityCommands -replace 't.MaxFPS 0', "t.MaxFPS $FrameRateCap"
    $logName = $Stage.ToLowerInvariant() + $(if ($FrameRateCap -gt 0) { "_fps$FrameRateCap" } else { '' }) + $(if ($Offscreen) { '_offscreen' } else { '' }) + '.log'
    $testArguments = @($projectFile, '/Game/Naiwa/Maps/L_NaiwaGraybox', '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', '-unattended', '-nop4', '-nosplash', "-ExecCmds=$qualityCommands,r.ScreenshotDelegate 1", "-abslog=$(Join-Path $recordDirectory $logName)")
    if ($Stage -in @('Test', 'Record')) { $testArguments += @('-NaiwaAutoTest', '-NaiwaTestQuit', '-NaiwaTestDelay=15') }
    else { $testArguments += @('-NaiwaVisualReview', '-NaiwaReviewQuit') }
    if ($Stage -eq 'ClipPlayback') { $testArguments += '-NaiwaReviewPlayback' }
    if ($Stage -eq 'Record') { $testArguments += '-NaiwaRecordFrames' }
    # Keep automated screenshot readbacks independent of a minimized desktop window.
    # Ordinary keyboard/focus acceptance continues to use launch_p0.ps1.
    if ($Offscreen) { $testArguments += '-RenderOffScreen' }
    & $editorCommand @testArguments
} else {
    $stageScript = switch ($Stage) {
        'Import' { 'import_e2.py' }
        'RunImport' { 'import_run.py' }
        'Visuals' { 'configure_gameplay_visuals.py' }
        'Setup' { 'build_graybox.py' }
        'Audit' { 'validate_import.py' }
        'RunAudit' { 'validate_run.py' }
        'Reimport' { 'reimport_validation.py' }
    }
    & $editorCommand $projectFile -run=PythonScript "-script=$(Join-Path $PSScriptRoot ('Scripts/' + $stageScript))" -unattended -nop4 -nosplash -NullRHI "-abslog=$(Join-Path $recordDirectory ($Stage.ToLowerInvariant() + '.log'))"
}
if ($LASTEXITCODE -ne 0) { throw "$Stage failed with exit code $LASTEXITCODE; inspect Records logs." }
