[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectRoot = Join-Path $RepositoryRoot 'UE5\NaiwaP0'
$Project = Join-Path $ProjectRoot 'Nai_Long.uproject'
$EditorCommandlet = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$PythonScript = Join-Path $PSScriptRoot 'reimport_p7_enemy_tuning.py'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$EngineLog = Join-Path $EvidenceRoot "P7_EnemyTuningReimport_$Timestamp.log"
$StdoutLog = Join-Path $EvidenceRoot "P7_EnemyTuningReimport_$Timestamp.stdout.log"
$StderrLog = Join-Path $EvidenceRoot "P7_EnemyTuningReimport_$Timestamp.stderr.log"
$ReportPath = Join-Path $EvidenceRoot "P7_EnemyTuningReimport_$Timestamp.json"

foreach ($Path in @($Project, $EditorCommandlet, $PythonScript)) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required P7 enemy reimport input was not found: $Path"
    }
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

$ArgumentLine = '"' + $Project + '" -unattended -nop4 -nosplash -nullrhi -NoMutex -stdout -FullStdOutLogOutput -log -ExecutePythonScript="' + $PythonScript + '" -AbsLog="' + $EngineLog + '"'
$PreviousReportOverride = $env:NAIWA_P7_ENEMY_REIMPORT_REPORT
$env:NAIWA_P7_ENEMY_REIMPORT_REPORT = $ReportPath
$Process = $null
try {
    $Process = Start-Process -FilePath $EditorCommandlet -ArgumentList $ArgumentLine `
        -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $StdoutLog -RedirectStandardError $StderrLog
    $Finished = $Process.WaitForExit($TimeoutSeconds * 1000)
    if (-not $Finished -and -not $Process.HasExited) {
        Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue
        $Process.WaitForExit()
        throw "P7 enemy DataTable reimport timed out. See $EngineLog"
    }
    $Process.Refresh()
    $ExitCode = $Process.ExitCode
} finally {
    if ($null -eq $PreviousReportOverride) {
        Remove-Item Env:\NAIWA_P7_ENEMY_REIMPORT_REPORT -ErrorAction SilentlyContinue
    } else {
        $env:NAIWA_P7_ENEMY_REIMPORT_REPORT = $PreviousReportOverride
    }
}

if (-not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) {
    throw "P7 enemy DataTable reimport produced no report. Exit: $ExitCode; see $EngineLog"
}
$Report = Get-Content -LiteralPath $ReportPath -Raw | ConvertFrom-Json
$Passed = $ExitCode -eq 0 -and $Report.status -eq 'passed' -and $Report.updated_asset_count -eq 1
[pscustomobject]@{
    Status = if ($Passed) { 'PASS' } else { 'FAIL' }
    ProcessExitCode = $ExitCode
    UpdatedAssets = $Report.updated_asset_count
    Speeds = ($Report.updated_rows | ConvertTo-Json -Compress)
    Report = $ReportPath
    EngineLog = $EngineLog
} | Format-List
if (-not $Passed) {
    throw "P7 enemy DataTable reimport failed. See $ReportPath and $EngineLog"
}
