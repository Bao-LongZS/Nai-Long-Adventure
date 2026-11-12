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
$PythonScript = Join-Path $PSScriptRoot 'p4_build_beta6_internal_test_map.py'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p4_alpha\evidence\Beta6'
$ReportPath = Join-Path $EvidenceRoot 'P4_Beta6_InternalTestMap_build_report_v01.json'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$EngineLog = Join-Path $EvidenceRoot "P4_Beta6_InternalTestMap_engine_$Timestamp.log"
$StdoutLog = Join-Path $EvidenceRoot "P4_Beta6_InternalTestMap_stdout_$Timestamp.log"
$StderrLog = Join-Path $EvidenceRoot "P4_Beta6_InternalTestMap_stderr_$Timestamp.log"

foreach ($Path in @($Project, $EditorCommandlet, $PythonScript)) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required Beta6 internal map input was not found: $Path"
    }
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
$PreviousReportUtc = if (Test-Path -LiteralPath $ReportPath -PathType Leaf) {
    (Get-Item -LiteralPath $ReportPath).LastWriteTimeUtc
} else {
    [DateTime]::MinValue
}
$ArgumentLine = '"' + $Project + '" -unattended -nop4 -nosplash -nullrhi -NoMutex -stdout -FullStdOutLogOutput -ExecutePythonScript="' + $PythonScript + '" -AbsLog="' + $EngineLog + '"'
$Process = Start-Process -FilePath $EditorCommandlet -ArgumentList $ArgumentLine `
    -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput $StdoutLog -RedirectStandardError $StderrLog
$Finished = $Process.WaitForExit($TimeoutSeconds * 1000)
if (-not $Finished -and -not $Process.HasExited) {
    Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue
    $Process.WaitForExit()
}
$Process.Refresh()
$EditorExitCode = if ($Process.HasExited -and $null -ne $Process.ExitCode) { [int]$Process.ExitCode } else { $null }
$FreshReport = Test-Path -LiteralPath $ReportPath -PathType Leaf
if ($FreshReport) {
    $FreshReport = (Get-Item -LiteralPath $ReportPath).LastWriteTimeUtc -gt $PreviousReportUtc
}
if (-not $Finished -or -not $Process.HasExited -or -not $FreshReport) {
    throw "Beta6 internal map creation did not finish with a fresh report (editor exit code: $EditorExitCode). Check $EngineLog, $StdoutLog, and $StderrLog"
}

$Report = Get-Content -LiteralPath $ReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($Report.status -ne 'map_saved_pending_runtime_acceptance' -or
    $Report.default_game_mode -ne '/Script/NaiwaRuntime.NaiwaP4Beta6TestGameMode') {
    throw "Beta6 internal map report is incomplete or points at the wrong GameMode. See $ReportPath"
}
if ($null -ne $EditorExitCode -and $EditorExitCode -ne 0) {
    Write-Warning "Unreal returned exit code $EditorExitCode after saving the isolated Beta6 test map and a complete build report; inspect $EngineLog for shutdown diagnostics."
}
[pscustomobject]@{
    Status = $Report.status
    Map = $Report.map
    GameMode = $Report.default_game_mode
    RuntimePlayed = $Report.runtime_played
    EditorExitCode = $EditorExitCode
    EngineLog = $EngineLog
    BuildReport = $ReportPath
} | Format-List
