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
$PythonScript = Join-Path $PSScriptRoot 'p4_import_beta6_environment_art.py'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p4_alpha\evidence\Beta6'
$ReportPath = Join-Path $EvidenceRoot 'P4_Beta6_EnvironmentArt_import_report_v01.json'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$EngineLog = Join-Path $EvidenceRoot "P4_Beta6_EnvironmentArt_engine_$Timestamp.log"
$StdoutLog = Join-Path $EvidenceRoot "P4_Beta6_EnvironmentArt_stdout_$Timestamp.log"
$StderrLog = Join-Path $EvidenceRoot "P4_Beta6_EnvironmentArt_stderr_$Timestamp.log"

foreach ($Path in @($Project, $EditorCommandlet, $PythonScript)) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required environment art import input was not found: $Path"
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
    throw "Beta6 environment art import timed out. Check $EngineLog, $StdoutLog, and $StderrLog"
}
$FreshReport = Test-Path -LiteralPath $ReportPath -PathType Leaf
if ($FreshReport) {
    $FreshReport = (Get-Item -LiteralPath $ReportPath).LastWriteTimeUtc -gt $PreviousReportUtc
}
if (-not $FreshReport) {
    throw "Beta6 environment art import did not finish with a fresh report. Check $EngineLog, $StdoutLog, and $StderrLog"
}

$Report = Get-Content -LiteralPath $ReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($Report.status -ne 'textures_imported_pending_engineering_theme_binding_and_readability_review' -or $Report.asset_count -ne 5) {
    throw "Beta6 environment texture import is incomplete. See $ReportPath"
}
$MissingAssets = @($Report.assets | ForEach-Object {
    $RelativeAsset = $_.texture_asset -replace '^/Game/', ''
    $AssetFile = Join-Path $ProjectRoot ('Content\' + ($RelativeAsset -replace '/', '\') + '.uasset')
    if (-not (Test-Path -LiteralPath $AssetFile -PathType Leaf)) { $AssetFile }
})
if ($MissingAssets.Count -gt 0) {
    throw "The import report references missing UE texture assets: $($MissingAssets -join ', ')"
}
$EditorExitCode = $Process.ExitCode
if ($null -ne $EditorExitCode -and [int]$EditorExitCode -ne 0) {
    Write-Warning "Unreal Editor returned exit code $EditorExitCode after writing a complete import report and all five texture assets; inspect $EngineLog for unrelated startup/automation diagnostics."
}
[pscustomobject]@{
    Status = $Report.status
    TextureCount = $Report.asset_count
    Textures = @($Report.assets | ForEach-Object { $_.texture_asset })
    EditorExitCode = if ($null -eq $EditorExitCode) { 'not reported by process wrapper' } else { $EditorExitCode }
    EngineLog = $EngineLog
    ImportReport = $ReportPath
} | Format-List
