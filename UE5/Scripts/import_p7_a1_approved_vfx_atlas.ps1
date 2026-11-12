[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Project = Join-Path $RepositoryRoot 'UE5\NaiwaP0\Nai_Long.uproject'
$ImportScript = Join-Path $RepositoryRoot 'UE5\Scripts\import_p7_a1_approved_vfx_atlas.py'
$EditorCommandlet = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$SafeTempRoot = Join-Path $RepositoryRoot 'tmp\P7VFXImportTemp'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$LogPath = Join-Path $EvidenceRoot "P7_A1_ApprovedVFXAtlasImport_Commandlet_$Timestamp.log"
$StdoutPath = Join-Path $EvidenceRoot "P7_A1_ApprovedVFXAtlasImport_Commandlet_$Timestamp.stdout.log"
$StderrPath = Join-Path $EvidenceRoot "P7_A1_ApprovedVFXAtlasImport_Commandlet_$Timestamp.stderr.log"

foreach ($path in @($Project, $ImportScript, $EditorCommandlet)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required P7 VFX import input was not found: $path"
    }
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
New-Item -ItemType Directory -Force -Path $SafeTempRoot | Out-Null

"P7 A1 approved VFX atlas import - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" |
    Set-Content -LiteralPath $LogPath -Encoding utf8
$PreviousTemp = $env:TEMP
$PreviousTmp = $env:TMP
$Process = $null
try {
    $env:TEMP = $SafeTempRoot
    $env:TMP = $SafeTempRoot
    $ArgumentLine = '"' + $Project + '" -unattended -nop4 -nosplash -nullrhi -NoMutex -stdout -FullStdOutLogOutput -log -ExecutePythonScript="' + $ImportScript + '" -AbsLog="' + $LogPath + '"'
    $Process = Start-Process -FilePath $EditorCommandlet -ArgumentList $ArgumentLine `
        -WorkingDirectory (Split-Path -Parent $Project) -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath
    $Finished = $Process.WaitForExit(180000)
    if (-not $Finished -and -not $Process.HasExited) {
        Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue
        $Process.WaitForExit()
    }
    $Process.Refresh()
    $ExitCode = if ($Finished) { $Process.ExitCode } else { $null }
} finally {
    $env:TEMP = $PreviousTemp
    $env:TMP = $PreviousTmp
}
if ($ExitCode -ne 0) {
    Get-Content -LiteralPath $LogPath -Tail 45
    if (Test-Path -LiteralPath $StderrPath) { Get-Content -LiteralPath $StderrPath -Tail 25 }
    throw "P7 VFX atlas import commandlet failed or timed out (exit code $ExitCode). See $LogPath"
}

$Reports = Get-ChildItem -LiteralPath $EvidenceRoot -Filter 'P7_A1_ApprovedVFXAtlasImport_*.json' |
    Where-Object { $_.LastWriteTime -ge (Get-Item -LiteralPath $LogPath).LastWriteTime.AddMinutes(-2) } |
    Sort-Object LastWriteTime -Descending
$Report = $Reports | Select-Object -First 1
if (-not $Report) {
    Get-Content -LiteralPath $LogPath -Tail 45
    if (Test-Path -LiteralPath $StdoutPath) { Get-Content -LiteralPath $StdoutPath -Tail 25 }
    throw "P7 VFX import returned $ExitCode but generated no fresh report. See $LogPath"
}
$ReportData = Get-Content -LiteralPath $Report.FullName -Raw | ConvertFrom-Json
if ($ReportData.status -ne 'imported_approved_vfx_atlas_runtime_playback_pending' -or
    @($ReportData.assets).Count -ne 1 -or @($ReportData.preflight.approved_frame_count) -ne 16) {
    Get-Content -LiteralPath $Report.FullName -Raw
    throw "P7 VFX import did not report the approved 16-frame atlas. See $($Report.FullName)"
}

Write-Output "P7 A1 import succeeded: 16 approved frames in one Texture2D atlas."
Write-Output "Destination: /Game/Naiwa/P7/VFX/A1/T_P7_A1_VFXAtlas_v07"
Write-Output "Report: $($Report.FullName)"
Write-Output "Commandlet log: $LogPath"
