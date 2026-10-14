# Run one resumable Blender stage. Python exceptions must fail the stage.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Script,
    [Parameter(Mandatory = $true)][string]$Log,
    [Parameter(Mandatory = $true)][string]$ExpectedMarker,
    [string[]]$ScriptArgs = @(),
    [string]$Blender = 'D:\STEAM\steamapps\common\Blender\blender.exe'
)
$ErrorActionPreference = 'Stop'
$resolvedScript = (Resolve-Path -LiteralPath $Script).Path
$resolvedLog = [System.IO.Path]::GetFullPath($Log)
if (-not (Test-Path -LiteralPath $Blender -PathType Leaf)) {
    throw "Blender executable is missing: $Blender"
}
if (-not (Test-Path -LiteralPath (Split-Path -Parent $resolvedLog) -PathType Container)) {
    throw 'Create the stage output directory before running Blender.'
}
$arguments = @('--background', '--python-exit-code', '1', '--python', $resolvedScript)
if ($ScriptArgs.Count -gt 0) { $arguments += @('--') + $ScriptArgs }
# Keep full details on disk, and return only a brief stage result to the chat.
$stageOutput = & $Blender @arguments 2>&1
$stageExitCode = $LASTEXITCODE
$stageOutput | Set-Content -LiteralPath $resolvedLog -Encoding UTF8
$stageText = ($stageOutput | ForEach-Object { $_.ToString() }) -join "`n"
if ($stageExitCode -ne 0) {
    $stageOutput | Select-Object -Last 12 | Write-Output
    throw "Blender stage failed with exit code $stageExitCode. See $resolvedLog"
}
if (-not $stageText.Contains($ExpectedMarker)) {
    throw "Blender exited without the expected completion marker '$ExpectedMarker'. See $resolvedLog"
}
Write-Output "BLENDER_STAGE_OK $ExpectedMarker"
Write-Output "Log: $resolvedLog"
