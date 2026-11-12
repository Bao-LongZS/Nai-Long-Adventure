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
$PythonScript = Join-Path $PSScriptRoot 'p4_build_beta6_scene_previews.py'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p4_alpha\evidence\Beta6'
$ReportPath = Join-Path $EvidenceRoot 'P4_Beta6_ScenePreview_build_report_v01.json'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$EngineLog = Join-Path $EvidenceRoot "P4_Beta6_ScenePreview_engine_$Timestamp.log"
$StdoutLog = Join-Path $EvidenceRoot "P4_Beta6_ScenePreview_stdout_$Timestamp.log"
$StderrLog = Join-Path $EvidenceRoot "P4_Beta6_ScenePreview_stderr_$Timestamp.log"

foreach ($Path in @($Project, $EditorCommandlet, $PythonScript)) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required Beta6 scene preview input was not found: $Path"
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
$FreshReport = Test-Path -LiteralPath $ReportPath -PathType Leaf
if ($FreshReport) {
    $FreshReport = (Get-Item -LiteralPath $ReportPath).LastWriteTimeUtc -gt $PreviousReportUtc
}
if (-not $Finished -or -not $Process.HasExited -or $Process.ExitCode -ne 0 -or -not $FreshReport) {
    throw "Beta6 scene preview import did not finish with a fresh report. Check $EngineLog, $StdoutLog, and $StderrLog"
}

$Report = Get-Content -LiteralPath $ReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($Report.status -ne 'maps_saved_pending_visual_review' -or $Report.maps.Count -ne 5) {
    throw "Beta6 scene map count or build status is incomplete. See $ReportPath"
}
[pscustomobject]@{
    Status = $Report.status
    MapCount = $Report.maps.Count
    Maps = @($Report.maps | ForEach-Object { $_.review_map })
    EngineLog = $EngineLog
    ImportReport = $ReportPath
} | Format-List
