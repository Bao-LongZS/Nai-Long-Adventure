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
$PythonScript = Join-Path $PSScriptRoot 'import_p7_alpha_fixture.py'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$EngineLog = Join-Path $EvidenceRoot "P7_AlphaFixtureImport_$Timestamp.log"
$StdoutLog = Join-Path $EvidenceRoot "P7_AlphaFixtureImport_$Timestamp.stdout.log"
$StderrLog = Join-Path $EvidenceRoot "P7_AlphaFixtureImport_$Timestamp.stderr.log"
$ReportPath = Join-Path $EvidenceRoot "P7_AlphaFixtureImport_$Timestamp.json"

foreach ($path in @($Project, $EditorCommandlet, $PythonScript)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required P7 Alpha fixture import input was not found: $path"
    }
}
$DataFolder = Join-Path $ProjectRoot 'Content\Naiwa\P7\Data'
if (Test-Path -LiteralPath $DataFolder) {
    $ExistingItems = @(Get-ChildItem -LiteralPath $DataFolder -Force -ErrorAction Stop)
    if ($ExistingItems.Count -gt 0) {
        throw "Refusing to replace existing P7 Data assets: $DataFolder"
    }
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

$ArgumentLine = '"' + $Project + '" -unattended -nop4 -nosplash -nullrhi -NoMutex -stdout -FullStdOutLogOutput -log -ExecutePythonScript="' + $PythonScript + '" -AbsLog="' + $EngineLog + '"'
$PreviousReportOverride = $env:NAIWA_P7_ALPHA_IMPORT_REPORT
$env:NAIWA_P7_ALPHA_IMPORT_REPORT = $ReportPath
$StartedAt = Get-Date
$Process = $null
try {
    $Process = Start-Process -FilePath $EditorCommandlet -ArgumentList $ArgumentLine `
        -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $StdoutLog -RedirectStandardError $StderrLog
    $Finished = $Process.WaitForExit($TimeoutSeconds * 1000)
    if (-not $Finished -and -not $Process.HasExited) {
        Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue
        $Process.WaitForExit()
        throw "P7 Alpha fixture import timed out. See $EngineLog"
    }
    $Process.Refresh()
    $ExitCode = $Process.ExitCode
} finally {
    if ($null -eq $PreviousReportOverride) {
        Remove-Item Env:\NAIWA_P7_ALPHA_IMPORT_REPORT -ErrorAction SilentlyContinue
    } else {
        $env:NAIWA_P7_ALPHA_IMPORT_REPORT = $PreviousReportOverride
    }
}

if (-not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) {
    throw "P7 Alpha fixture import did not produce a report. Engine exit: $ExitCode; see $EngineLog"
}
$Report = Get-Content -LiteralPath $ReportPath -Raw | ConvertFrom-Json
$Passed = $ExitCode -eq 0 -and $Report.status -eq 'passed' -and $Report.table_count -eq 5
[pscustomobject]@{
    Status = if ($Passed) { 'PASS' } else { 'FAIL' }
    ProcessExitCode = $ExitCode
    TableCount = $Report.table_count
    Report = $ReportPath
    EngineLog = $EngineLog
} | Format-List
if (-not $Passed) {
    throw "P7 Alpha fixture import failed. See $ReportPath and $EngineLog"
}
