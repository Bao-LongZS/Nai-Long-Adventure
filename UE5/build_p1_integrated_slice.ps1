[CmdletBinding()]
param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')

# Deploy this wrapper at UE5/build_p1_integrated_slice.ps1, with its Python and
# JSON companions at UE5/Scripts/. A saved map is still a partial candidate.
$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$scriptFile = Join-Path $PSScriptRoot 'Scripts/build_p1_integrated_slice.py'
$configFile = Join-Path $PSScriptRoot 'Scripts/p1_integrated_slice_config_v1.json'
$reportFile = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1/integrated_slice_build.json'
$logFile = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1/integrated_slice_build.log'
foreach ($required in @($projectFile, $editor, $scriptFile, $configFile)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required file missing: $required"
    }
}
New-Item -ItemType Directory -Path (Split-Path -Parent $reportFile) -Force | Out-Null
$startedUtc = [DateTime]::UtcNow
$arguments = @($projectFile, '-run=PythonScript', "-script=$scriptFile", '-unattended', '-nop4', '-nosplash', '-NullRHI', "-abslog=$logFile")
& $editor @arguments
if ($LASTEXITCODE -ne 0) {
    throw "P1 integrated teaching slice build failed ($LASTEXITCODE); inspect $logFile"
}
if (-not (Test-Path -LiteralPath $reportFile -PathType Leaf)) {
    throw "P1 integrated teaching slice build left no report: $reportFile"
}
$reportItem = Get-Item -LiteralPath $reportFile
if ($reportItem.LastWriteTimeUtc -lt $startedUtc) {
    throw "P1 integrated teaching slice build report is stale: $reportFile"
}
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
if ($report.schema -ne 'NaiwaP1IntegratedTeachingSliceV1' -or
    $report.target_map -ne '/Game/Naiwa/P1/Maps/L_NaiwaP1_Integrated' -or
    $report.phase -ne 'P1-5b_partial_teaching_slice' -or
    $report.status -ne 'saved_partial_teaching_slice_pending_runtime_and_visual_review' -or
    $report.acceptance -ne 'not_tested_partial_teaching_slice_only' -or
    $report.layout.kill_z -ne -1000.0) {
    throw "P1 integrated teaching slice report status '$($report.status)'; inspect $reportFile"
}
Write-Output "P1 integrated teaching slice saved (partial candidate); report: $reportFile"
