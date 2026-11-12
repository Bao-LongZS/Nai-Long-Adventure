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
$PythonScript = Join-Path $PSScriptRoot 'p4_build_beta6_environment_review_maps.py'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p4_alpha\evidence\Beta6'
$ReportPath = Join-Path $EvidenceRoot 'P4_Beta6_EnvironmentReviewMaps_build_report_v01.json'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$EngineLog = Join-Path $EvidenceRoot "P4_Beta6_EnvironmentReviewMaps_engine_$Timestamp.log"
$StdoutLog = Join-Path $EvidenceRoot "P4_Beta6_EnvironmentReviewMaps_stdout_$Timestamp.log"
$StderrLog = Join-Path $EvidenceRoot "P4_Beta6_EnvironmentReviewMaps_stderr_$Timestamp.log"

foreach ($Path in @($Project, $EditorCommandlet, $PythonScript)) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required Beta6 environment review input was not found: $Path"
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
	throw "Beta6 environment review map build did not finish with a fresh report (editor exit code: $EditorExitCode). Check $EngineLog, $StdoutLog, and $StderrLog"
}

$Report = Get-Content -LiteralPath $ReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($Report.status -ne 'review_maps_saved_pending_fixed_camera_readability_review' -or $Report.map_count -ne 5) {
    throw "Beta6 environment review map count or status is incomplete. See $ReportPath"
}
$MissingAssets = [System.Collections.Generic.List[string]]::new()
foreach ($Item in $Report.items) {
    $Candidates = @(
        [pscustomobject]@{ Path = [string]$Item.texture_asset; Extension = '.uasset' },
        [pscustomobject]@{ Path = [string]$Item.material_asset; Extension = '.uasset' },
        [pscustomobject]@{ Path = [string]$Item.review_map; Extension = '.umap' }
    )
    foreach ($Candidate in $Candidates) {
        $RelativeAsset = $Candidate.Path -replace '^/Game/', ''
        $AssetFile = Join-Path $ProjectRoot ('Content\' + ($RelativeAsset -replace '/', '\') + $Candidate.Extension)
        if (-not (Test-Path -LiteralPath $AssetFile -PathType Leaf)) { $MissingAssets.Add($AssetFile) }
    }
}
if ($MissingAssets.Count -gt 0) {
	throw "The environment review report references missing UE assets: $($MissingAssets -join ', ')"
}
if ($null -ne $EditorExitCode -and $EditorExitCode -ne 0) {
    Write-Warning "Unreal returned exit code $EditorExitCode after saving all five maps and a complete build report; inspect $EngineLog for shutdown diagnostics."
}
[pscustomobject]@{
    Status = $Report.status
    MapCount = $Report.map_count
    Maps = @($Report.items | ForEach-Object { $_.review_map })
    RuntimeBound = $false
    EditorExitCode = $EditorExitCode
    EngineLog = $EngineLog
    BuildReport = $ReportPath
} | Format-List
