[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [int]$TimeoutSeconds = 240
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectRoot = Join-Path $RepositoryRoot 'UE5\NaiwaP0'
$Project = Join-Path $ProjectRoot 'Nai_Long.uproject'
$EditorCommandlet = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$PythonScript = Join-Path $PSScriptRoot 'p4_import_beta6_naitiao_turnaround.py'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p4_alpha\evidence\Beta6'
$ReportPath = Join-Path $EvidenceRoot 'P4_Beta6_NaitiaoTurnaround_import_report_v01.json'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$EngineLog = Join-Path $EvidenceRoot "P4_Beta6_NaitiaoTurnaround_engine_$Timestamp.log"
$StdoutLog = Join-Path $EvidenceRoot "P4_Beta6_NaitiaoTurnaround_stdout_$Timestamp.log"
$StderrLog = Join-Path $EvidenceRoot "P4_Beta6_NaitiaoTurnaround_stderr_$Timestamp.log"

foreach ($Path in @($Project, $EditorCommandlet, $PythonScript)) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required Naitiao turnaround import input was not found: $Path"
    }
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
$PreviousReportUtc = if (Test-Path -LiteralPath $ReportPath -PathType Leaf) {
    (Get-Item -LiteralPath $ReportPath).LastWriteTimeUtc
} else {
    [DateTime]::MinValue
}
$ArgumentLine = '"' + $Project + '" -unattended -nop4 -nosplash -nullrhi -NoMutex -stdout -FullStdOutLogOutput -run=PythonScript -Script="' + $PythonScript + '" -AbsLog="' + $EngineLog + '"'
$Process = Start-Process -FilePath $EditorCommandlet -ArgumentList $ArgumentLine `
    -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput $StdoutLog -RedirectStandardError $StderrLog
$Finished = $Process.WaitForExit($TimeoutSeconds * 1000)
if (-not $Finished -and -not $Process.HasExited) {
    Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue
    $Process.WaitForExit()
}
$Process.Refresh()
if (-not $Finished -or -not $Process.HasExited) {
    throw "Naitiao turnaround import timed out. Check $EngineLog, $StdoutLog, and $StderrLog"
}
$FreshReport = Test-Path -LiteralPath $ReportPath -PathType Leaf
if ($FreshReport) {
    $FreshReport = (Get-Item -LiteralPath $ReportPath).LastWriteTimeUtc -gt $PreviousReportUtc
}
if (-not $FreshReport) {
    throw "Naitiao turnaround import did not finish with a fresh report. Check $EngineLog, $StdoutLog, and $StderrLog"
}
$Report = Get-Content -LiteralPath $ReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($Report.status -ne 'reference_texture_imported_pending_owner_review') {
    throw "Naitiao turnaround texture import is incomplete. See $ReportPath"
}
$RelativeAsset = $Report.texture_asset -replace '^/Game/', ''
$AssetFile = Join-Path $ProjectRoot ('Content\' + ($RelativeAsset -replace '/', '\') + '.uasset')
if (-not (Test-Path -LiteralPath $AssetFile -PathType Leaf)) {
    throw "Naitiao turnaround reference texture was not saved: $AssetFile"
}
[pscustomobject]@{
    Status = $Report.status
    Texture = $Report.texture_asset
    RuntimeReference = $Report.runtime_reference
    ImportReport = $ReportPath
    EngineLog = $EngineLog
} | Format-List
