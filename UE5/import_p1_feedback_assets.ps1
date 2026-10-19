[CmdletBinding()]
param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')

$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$scriptFile = Join-Path $PSScriptRoot 'Scripts/import_p1_feedback_assets.py'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$saved = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1'
$reportFile = Join-Path $saved 'feedback_asset_import.json'
$logFile = Join-Path $saved 'feedback_asset_import.log'
foreach ($path in @($projectFile, $scriptFile, $editor)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required import file missing: $path" }
}
New-Item -ItemType Directory -Path $saved -Force | Out-Null
$started = [DateTime]::UtcNow
$arguments = @($projectFile, '-run=PythonScript', "-script=$scriptFile", '-unattended', '-nop4', '-nosplash', '-NullRHI', "-abslog=$logFile")
& $editor @arguments
if ($LASTEXITCODE -ne 0) { throw "Feedback asset import failed with exit code $LASTEXITCODE; inspect $logFile" }
if (-not (Test-Path -LiteralPath $reportFile -PathType Leaf)) {
    throw "Feedback asset import report is missing: $reportFile"
}
$report = Get-Content -LiteralPath $reportFile -Raw -Encoding UTF8 | ConvertFrom-Json
if ([DateTimeOffset]::Parse($report.started_utc).UtcDateTime -lt $started.AddSeconds(-1)) {
    throw "Feedback asset import report is stale: $reportFile"
}
if ($report.status -ne 'imported' -or $report.assets.Count -ne 109 -or $report.death_laugh_frames.Count -ne 96) {
    throw "Feedback asset import report is incomplete: $reportFile"
}
Write-Output "Imported user audio, the pickup color/fade material, and 96 death-animation frames. Report: $reportFile"
