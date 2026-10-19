[CmdletBinding()]
param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$scriptFile = Join-Path $PSScriptRoot 'Scripts/build_p1_full.py'
$savedDirectory = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1'
$reportFile = Join-Path $savedDirectory 'full_build.json'
$logFile = Join-Path $savedDirectory 'full_build.log'
$editorCommand = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
foreach ($path in @($projectFile, $scriptFile, $editorCommand)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Required P1 Full input missing: $path" }
}
New-Item -ItemType Directory -Path $savedDirectory -Force | Out-Null

$startedUtc = [DateTime]::UtcNow
$arguments = @($projectFile, '-run=PythonScript', "-script=$scriptFile", '-unattended', '-nop4', '-nosplash', '-NullRHI', "-abslog=$logFile")
& $editorCommand @arguments
if ($LASTEXITCODE -ne 0) { throw "P1 Full generation failed with exit code $LASTEXITCODE; inspect $logFile" }
if (-not (Test-Path -LiteralPath $reportFile)) { throw "P1 Full exited without report: $reportFile" }
if ((Get-Item -LiteralPath $reportFile).LastWriteTimeUtc -lt $startedUtc) {
    throw "P1 Full left a stale report: $reportFile"
}
$report = Get-Content -LiteralPath $reportFile -Raw -Encoding UTF8 | ConvertFrom-Json
if ($report.status -ne 'saved_candidate_pending_full_walk_run_review') {
    throw "P1 Full report status '$($report.status)' is unexpected; inspect $reportFile"
}
Write-Output "P1 Full completed: $($report.layout.status); report: $reportFile"
