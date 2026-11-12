[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Project = Join-Path $RepositoryRoot 'UE5\NaiwaP0\Nai_Long.uproject'
$ImportScript = Join-Path $PSScriptRoot 'import_p7_selected_result_badge_v02.py'
$EditorCommandlet = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$SafeTempRoot = Join-Path $RepositoryRoot 'tmp\P7SelectedResultBadgeImportTemp'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$LogPath = Join-Path $EvidenceRoot "P7_A1_SelectedResultBadgeV02Import_Commandlet_$Timestamp.log"
$StdoutPath = Join-Path $EvidenceRoot "P7_A1_SelectedResultBadgeV02Import_Commandlet_$Timestamp.stdout.log"
$StderrPath = Join-Path $EvidenceRoot "P7_A1_SelectedResultBadgeV02Import_Commandlet_$Timestamp.stderr.log"
foreach ($path in @($Project, $ImportScript, $EditorCommandlet)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required input missing: $path" }
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot,$SafeTempRoot | Out-Null
"P7 user-selected result badge v02 import - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" | Set-Content -LiteralPath $LogPath -Encoding utf8
$PreviousTemp = $env:TEMP
$PreviousTmp = $env:TMP
try {
    $env:TEMP = $SafeTempRoot
    $env:TMP = $SafeTempRoot
    $ArgumentLine = '"' + $Project + '" -unattended -nop4 -nosplash -nullrhi -NoMutex -stdout -FullStdOutLogOutput -log -ExecutePythonScript="' + $ImportScript + '" -AbsLog="' + $LogPath + '"'
    $Process = Start-Process -FilePath $EditorCommandlet -ArgumentList $ArgumentLine -WorkingDirectory (Split-Path -Parent $Project) -WindowStyle Hidden -PassThru -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath
    $Finished = $Process.WaitForExit(180000)
    if (-not $Finished -and -not $Process.HasExited) { Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue; $Process.WaitForExit() }
    $Process.Refresh()
    $ExitCode = if ($Finished) { $Process.ExitCode } else { $null }
} finally {
    $env:TEMP = $PreviousTemp
    $env:TMP = $PreviousTmp
}
$Report = Get-ChildItem -LiteralPath $EvidenceRoot -Filter 'P7_A1_SelectedResultBadgeV02Import_*.json' | Where-Object { $_.LastWriteTime -ge (Get-Item -LiteralPath $LogPath).LastWriteTime.AddMinutes(-2) } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $Report) { Get-Content -LiteralPath $LogPath -Tail 50; throw "No fresh import report; see $LogPath" }
$ReportData = Get-Content -LiteralPath $Report.FullName -Raw | ConvertFrom-Json
if ($ReportData.status -ne 'imported_and_wired_user_selected_result_badge_v02' -or @($ReportData.assets).Count -ne 1) {
    Get-Content -LiteralPath $LogPath -Tail 50
    if (Test-Path -LiteralPath $StderrPath) { Get-Content -LiteralPath $StderrPath -Tail 25 }
    Get-Content -LiteralPath $Report.FullName -Raw
    throw "Selected result badge import did not verify. See $($Report.FullName) (exit $ExitCode)"
}
$ReportData.assets | Select-Object asset_id,asset_path,dimensions_px,action | Format-Table -AutoSize
Write-Output "Report: $($Report.FullName)"
Write-Output "Commandlet log: $LogPath"
