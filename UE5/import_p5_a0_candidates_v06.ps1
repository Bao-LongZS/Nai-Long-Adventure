[CmdletBinding()]
param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$projectFile = Join-Path $projectRoot 'NaiwaP0/Nai_Long.uproject'
$editorCommand = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$importScript = Join-Path $projectRoot 'Scripts/p5_import_a0_candidates_v06.py'
$evidenceDir = Join-Path $projectRoot 'Records/p5_m2'
$report = Join-Path $evidenceDir 'P5_A0_ArtSample_v06_asset_import_report_2026-10-30.json'
$runId = Get-Date -Format 'yyyyMMdd_HHmmss_fff'
$stdoutLog = Join-Path $evidenceDir "P5_A0_ArtSample_v06_import_stdout_$runId.log"
$commandletLog = Join-Path $evidenceDir "P5_A0_ArtSample_v06_import_$runId.log"
$reportBefore = if (Test-Path -LiteralPath $report) { (Get-Item -LiteralPath $report).LastWriteTimeUtc } else { $null }
$startedAt = [DateTime]::UtcNow

foreach ($requiredPath in @($projectFile, $editorCommand, $importScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required P5 import input not found: $requiredPath" }
}
New-Item -ItemType Directory -Path $evidenceDir -Force | Out-Null

$arguments = @(
    $projectFile,
    '-run=pythonscript',
    "-script=$importScript",
    '-unattended',
    '-nop4',
    '-nosplash',
    '-NullRHI',
    '-UTF8Output',
    '-stdout',
    '-FullStdOutLogOutput',
    "-abslog=$commandletLog"
)
& $editorCommand @arguments *> $stdoutLog
$commandletExitCode = $LASTEXITCODE

if ($commandletExitCode -ne 0) {
    throw "P5 v06 candidate asset import failed with exit code $commandletExitCode; inspect $stdoutLog and $commandletLog"
}
if (-not (Test-Path -LiteralPath $report)) {
    throw "P5 v06 import command returned without a report; inspect $stdoutLog and $commandletLog"
}
$result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
$reportAfter = (Get-Item -LiteralPath $report).LastWriteTimeUtc
$reportFresh = $null -eq $reportBefore -or $reportAfter -gt $reportBefore
if (-not $reportFresh -or $result.status -ne 'assets_imported_pending_editor_review' -or
	$result.assets.Count -ne 13) {
    throw "P5 v06 candidate asset import did not pass its metadata check: $report"
}
Write-Output "Imported $($result.assets.Count) P5 WIP assets into ArtSample_v06."
Write-Output "Report: $report"
Write-Output "Started UTC: $($startedAt.ToString('o'))"
