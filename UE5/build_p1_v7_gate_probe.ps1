[CmdletBinding()]
param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')

$ErrorActionPreference = 'Stop'
$project = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'Scripts/build_p1_v7_gate_probe.py'
$reportFile = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1/v7_gate_probe_build_v2.json'
$logFile = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1/v7_gate_probe_build.log'
foreach ($path in @($project, $editor, $script)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing: $path" }
}
New-Item -ItemType Directory -Path (Split-Path -Parent $reportFile) -Force | Out-Null
$started = [DateTime]::UtcNow
& $editor @($project, '-run=PythonScript', "-script=$script", '-unattended', '-nop4', '-nosplash', '-NullRHI', "-abslog=$logFile")
if ($LASTEXITCODE -ne 0) { throw "Gate fixture builder exited $LASTEXITCODE; inspect $logFile" }
if (-not (Test-Path -LiteralPath $reportFile -PathType Leaf)) { throw 'Gate fixture builder produced no report' }
if ((Get-Item -LiteralPath $reportFile).LastWriteTimeUtc -lt $started) { throw 'Gate fixture report is stale' }
$report = Get-Content -LiteralPath $reportFile -Raw -Encoding UTF8 | ConvertFrom-Json
if ($report.schema -ne 'NaiwaP1V7GateProbeBuildV2' -or
    $report.status -ne 'saved_isolated_fixture_pending_runtime' -or
    $report.map -ne '/Game/Naiwa/P1/Maps/L_NaiwaP1_v7_GateProbe' -or
    $report.actor_count -ne 18 -or $report.isolated_only -ne $true -or
    $report.checkpoint_ids.A -ne 'IntegratedCheckpoint02' -or
    $report.checkpoint_ids.B -ne 'IntegratedCheckpoint04' -or
    $report.checkpoint_ids.C -ne 'IntegratedCheckpoint06') {
    throw "Gate fixture is invalid: $($report.status) $($report.error)"
}
$mapFile = Join-Path $PSScriptRoot 'NaiwaP0/Content/Naiwa/P1/Maps/L_NaiwaP1_v7_GateProbe.umap'
if ((Get-FileHash -LiteralPath $mapFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $report.map_sha256) {
    throw 'Gate fixture map SHA-256 differs from the fresh build report'
}
Write-Output "Isolated v7 gate fixture saved: $reportFile"
