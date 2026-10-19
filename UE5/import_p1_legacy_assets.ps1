[CmdletBinding()]
param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')

$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$scriptFile = Join-Path $PSScriptRoot 'Scripts/import_p1_legacy_assets.py'
$editorCommand = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$savedDirectory = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1'
$reportFile = Join-Path $savedDirectory 'legacy_asset_import.json'
$logFile = Join-Path $savedDirectory 'legacy_asset_import.log'

foreach ($path in @($projectFile, $scriptFile, $editorCommand)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required file missing: $path" }
}
New-Item -ItemType Directory -Path $savedDirectory -Force | Out-Null
$startedUtc = [DateTime]::UtcNow
$arguments = @($projectFile, '-run=PythonScript', "-script=$scriptFile", '-unattended', '-nop4', '-nosplash', '-NullRHI', "-abslog=$logFile")
& $editorCommand @arguments
if ($LASTEXITCODE -ne 0) { throw "P1 legacy import failed with exit code $LASTEXITCODE; inspect $logFile" }
if (-not (Test-Path -LiteralPath $reportFile -PathType Leaf)) { throw "P1 legacy import left no report: $reportFile" }
if ((Get-Item -LiteralPath $reportFile).LastWriteTimeUtc -lt $startedUtc) { throw "P1 legacy import left a stale report: $reportFile" }
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
if ($report.status -ne 'imported_or_verified' -or $report.selected_count -ne 9) {
    throw "P1 legacy import report is incomplete; inspect $reportFile"
}
Write-Output "P1 legacy import completed: $($report.new_count) new, $($report.existing_count) existing; report: $reportFile"
