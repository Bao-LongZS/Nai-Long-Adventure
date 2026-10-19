[CmdletBinding()]
param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')

$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$scriptFile = Join-Path $PSScriptRoot 'Scripts/build_p1_bounce.py'
$reportFile = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1/bounce_build.json'
$logFile = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1/bounce_build.log'
foreach ($required in @($projectFile, $editor, $scriptFile)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required file missing: $required" }
}
New-Item -ItemType Directory -Path (Split-Path -Parent $reportFile) -Force | Out-Null
$startedUtc = [DateTime]::UtcNow
$arguments = @($projectFile, '-run=PythonScript', "-script=$scriptFile", '-unattended', '-nop4', '-nosplash', '-NullRHI', "-abslog=$logFile")
& $editor @arguments
if ($LASTEXITCODE -ne 0) { throw "P1 Bounce map build failed ($LASTEXITCODE); inspect $logFile" }
if (-not (Test-Path -LiteralPath $reportFile -PathType Leaf)) { throw "P1 Bounce map build left no report: $reportFile" }
$reportItem = Get-Item -LiteralPath $reportFile
if ($reportItem.LastWriteTimeUtc -lt $startedUtc) { throw "P1 Bounce build report is stale: $reportFile" }
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
if ($report.schema -ne 'NaiwaP1BouncePrototypeV1' -or $report.status -ne 'saved_candidate_pending_walk_run_and_visual_review') {
    throw "P1 Bounce build status '$($report.status)'; inspect $reportFile"
}
Write-Output "P1 Bounce map saved; report: $reportFile"
