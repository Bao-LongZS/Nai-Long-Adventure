[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Project = Join-Path $RepositoryRoot 'UE5\NaiwaP0\Nai_Long.uproject'
$ImportScript = Join-Path $RepositoryRoot 'UE5\Scripts\import_p7_a1_approved_towers.py'
$EditorCommandlet = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$LogPath = Join-Path $EvidenceRoot "P7_A1_ApprovedTowerImport_Commandlet_$Timestamp.log"

foreach ($path in @($Project, $ImportScript, $EditorCommandlet)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required P7 import input was not found: $path"
    }
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

"P7 A1 approved tower import - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" |
    Set-Content -LiteralPath $LogPath -Encoding utf8
$Arguments = @(
    $Project,
    '-run=PythonScript',
    "-script=$ImportScript",
    '-unattended',
    '-nop4',
    '-nosplash',
    '-NullRHI',
    "-abslog=$LogPath"
)
$Output = & $EditorCommandlet @Arguments 2>&1
$ExitCode = $LASTEXITCODE
$Output | Out-File -LiteralPath $LogPath -Append -Encoding utf8
if ($ExitCode -ne 0) {
    Get-Content -LiteralPath $LogPath -Tail 35
    throw "P7 A1 texture import commandlet failed with exit code $ExitCode. See $LogPath"
}

$Report = Get-ChildItem -LiteralPath $EvidenceRoot -Filter 'P7_A1_ApprovedTowerImport_*.json' |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $Report) {
    Get-Content -LiteralPath $LogPath -Tail 35
    throw "P7 import returned $ExitCode but generated no report. See $LogPath"
}
$ReportData = Get-Content -LiteralPath $Report.FullName -Raw | ConvertFrom-Json
if ($ReportData.status -ne 'imported_approved_tower_textures_pending_scale_review' -or
    @($ReportData.assets).Count -ne 9) {
    Get-Content -LiteralPath $Report.FullName -Raw
    throw "P7 import did not report all nine approved tower textures. See $($Report.FullName)"
}

Write-Output "P7 A1 import succeeded: $(@($ReportData.assets).Count) approved tower textures."
Write-Output "Destination: /Game/Naiwa/P7/Towers/A1"
Write-Output "Report: $($Report.FullName)"
Write-Output "Commandlet log: $LogPath"
