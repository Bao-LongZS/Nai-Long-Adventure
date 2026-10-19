[CmdletBinding()]
param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')

$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$scriptFile = Join-Path $PSScriptRoot 'Scripts/build_p1_switch.py'
$reportFile = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1/switch_build.json'
$logFile = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1/switch_build.log'
foreach ($required in @($projectFile, $editor, $scriptFile)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required file missing: $required"
    }
}
New-Item -ItemType Directory -Path (Split-Path -Parent $reportFile) -Force | Out-Null
$startedUtc = [DateTime]::UtcNow
$arguments = @($projectFile, '-run=PythonScript', "-script=$scriptFile", '-unattended', '-nop4', '-nosplash', '-NullRHI', "-abslog=$logFile")
& $editor @arguments
if ($LASTEXITCODE -ne 0) {
    throw "P1 Switch map build failed ($LASTEXITCODE); inspect $logFile"
}
if (-not (Test-Path -LiteralPath $reportFile -PathType Leaf)) {
    throw "P1 Switch map build left no report: $reportFile"
}
$reportItem = Get-Item -LiteralPath $reportFile
if ($reportItem.LastWriteTimeUtc -lt $startedUtc) {
    throw "P1 Switch build report is stale: $reportFile"
}
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
if ($report.schema -ne 'NaiwaP1SwitchPrototypeV1' -or $report.target_map -ne '/Game/Naiwa/P1/Maps/L_NaiwaP1_Switch' -or $report.status -ne 'saved_candidate_pending_switch_probe_and_visual_review') {
    throw "P1 Switch build status '$($report.status)'; inspect $reportFile"
}
Write-Output "P1 Switch map saved; report: $reportFile"
