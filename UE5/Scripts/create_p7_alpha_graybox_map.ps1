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
$PythonScript = Join-Path $PSScriptRoot 'create_p7_alpha_graybox_map.py'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$EngineLog = Join-Path $EvidenceRoot "P7_AlphaGrayboxMapCreation_$Timestamp.log"
$StdoutLog = Join-Path $EvidenceRoot "P7_AlphaGrayboxMapCreation_$Timestamp.stdout.log"
$StderrLog = Join-Path $EvidenceRoot "P7_AlphaGrayboxMapCreation_$Timestamp.stderr.log"
$ReportPath = Join-Path $EvidenceRoot "P7_AlphaGrayboxMapCreation_$Timestamp.json"
$MapPackage = Join-Path $ProjectRoot 'Content\Naiwa\P7\Maps\L_P7_Alpha01_Graybox.umap'

foreach ($path in @($Project, $EditorCommandlet, $PythonScript)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required P7 map creation input was not found: $path"
    }
}
if (Test-Path -LiteralPath $MapPackage) {
    throw "Refusing to replace an existing P7 map: $MapPackage"
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

$ArgumentLine = '"' + $Project + '" -unattended -nop4 -nosplash -nullrhi -NoMutex -stdout -FullStdOutLogOutput -log -ExecutePythonScript="' + $PythonScript + '" -AbsLog="' + $EngineLog + '"'
$PreviousReportOverride = $env:NAIWA_P7_MAP_REPORT
$env:NAIWA_P7_MAP_REPORT = $ReportPath
$Process = $null
try {
    $Process = Start-Process -FilePath $EditorCommandlet -ArgumentList $ArgumentLine `
        -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $StdoutLog -RedirectStandardError $StderrLog
    $Finished = $Process.WaitForExit($TimeoutSeconds * 1000)
    if (-not $Finished -and -not $Process.HasExited) {
        Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue
        $Process.WaitForExit()
        throw "P7 map creation timed out. See $EngineLog"
    }
    $Process.Refresh()
    $ExitCode = $Process.ExitCode
} finally {
    if ($null -eq $PreviousReportOverride) {
        Remove-Item Env:\NAIWA_P7_MAP_REPORT -ErrorAction SilentlyContinue
    } else {
        $env:NAIWA_P7_MAP_REPORT = $PreviousReportOverride
    }
}

if (-not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) {
    throw "P7 map creation did not produce a report. Engine exit: $ExitCode; see $EngineLog"
}
$Report = Get-Content -LiteralPath $ReportPath -Raw | ConvertFrom-Json
$Passed = $ExitCode -eq 0 -and $Report.status -eq 'passed' -and (Test-Path -LiteralPath $MapPackage)
[pscustomobject]@{
    Status = if ($Passed) { 'PASS' } else { 'FAIL' }
    ProcessExitCode = $ExitCode
    Map = $Report.map
    GameMode = $Report.game_mode
    MapPackage = $MapPackage
    Report = $ReportPath
    EngineLog = $EngineLog
} | Format-List
if (-not $Passed) {
    throw "P7 Alpha graybox map creation failed. See $ReportPath and $EngineLog"
}
