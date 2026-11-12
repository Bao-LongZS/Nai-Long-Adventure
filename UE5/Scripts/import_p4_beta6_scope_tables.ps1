[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [int]$TimeoutSeconds = 240
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectRoot = Join-Path $RepositoryRoot 'UE5\NaiwaP0'
$Project = Join-Path $ProjectRoot 'Nai_Long.uproject'
$BuildBatch = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$EditorCommandlet = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$PythonScript = Join-Path $PSScriptRoot 'p4_import_beta6_scope_tables.py'
$BuildTempRoot = Join-Path $ProjectRoot 'Saved\Beta6BuildTemp'
$UbaRoot = Join-Path $ProjectRoot 'Saved\Beta6Uba'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p4_alpha\evidence\Beta6'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$BuildLog = Join-Path $EvidenceRoot "P4_Beta6_EditorBuild_$Timestamp.log"
$EngineLog = Join-Path $EvidenceRoot "P4_Beta6_DataTableImport_$Timestamp.log"
$StdoutLog = Join-Path $EvidenceRoot "P4_Beta6_DataTableImport_$Timestamp.stdout.log"
$StderrLog = Join-Path $EvidenceRoot "P4_Beta6_DataTableImport_$Timestamp.stderr.log"
$ImportReport = Join-Path $EvidenceRoot 'P4_Beta6_DataTableImport.json'

foreach ($Path in @($Project, $BuildBatch, $EditorCommandlet, $PythonScript)) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required Beta6 import input was not found: $Path"
    }
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
New-Item -ItemType Directory -Force -Path $BuildTempRoot | Out-Null
New-Item -ItemType Directory -Force -Path $UbaRoot | Out-Null

"P4 Beta6 editor build - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" | Set-Content -LiteralPath $BuildLog -Encoding utf8
$PreviousTemp = $env:TEMP
$PreviousTmp = $env:TMP
$PreviousUbaRoot = $env:UBA_ROOT
$BuildExitCode = -1
try {
	# Build.bat enables delayed expansion while deriving its lock-file path. The
	# sandbox TEMP contains "#!sandbox", whose exclamation mark gets stripped and
	# leaves a nonexistent lock directory. Use the ignored project Saved folder.
	$env:TEMP = $BuildTempRoot
	$env:TMP = $BuildTempRoot
	# UE 5.8 still starts the local UBA executor with -NoUBA; route its storage
	# away from the unwritable ProgramData default in this managed environment.
	$env:UBA_ROOT = $UbaRoot
	& $BuildBatch Nai_LongEditor Win64 Development $Project `
		-NoUBA -NoHotReloadFromIDE -NoLiveCoding -WaitMutex -MaxParallelActions=4 *>> $BuildLog
	$BuildExitCode = $LASTEXITCODE
    Get-Content -LiteralPath $BuildLog -Tail 60
} finally {
	if ($null -eq $PreviousTemp) { Remove-Item Env:\TEMP -ErrorAction SilentlyContinue }
	else { $env:TEMP = $PreviousTemp }
	if ($null -eq $PreviousTmp) { Remove-Item Env:\TMP -ErrorAction SilentlyContinue }
	else { $env:TMP = $PreviousTmp }
	if ($null -eq $PreviousUbaRoot) { Remove-Item Env:\UBA_ROOT -ErrorAction SilentlyContinue }
	else { $env:UBA_ROOT = $PreviousUbaRoot }
}
if ($BuildExitCode -ne 0) {
    throw "P4 Beta6 editor build failed with exit code $BuildExitCode. See $BuildLog"
}
$BuildText = (Get-Content -LiteralPath $BuildLog -Raw).Replace([string][char]0, '')
if ($BuildText -notmatch 'Result: Succeeded' -and $BuildText -notmatch 'Target is up to date') {
    throw "P4 Beta6 editor build has no success marker. See $BuildLog"
}

$PreviousReportUtc = if (Test-Path -LiteralPath $ImportReport -PathType Leaf) {
    (Get-Item -LiteralPath $ImportReport).LastWriteTimeUtc
} else {
    [DateTime]::MinValue
}
$PreviousReportOverride = $env:NAIWA_P4_BETA6_IMPORT_REPORT
$env:NAIWA_P4_BETA6_IMPORT_REPORT = $ImportReport
$ArgumentLine = '"' + $Project + '" -unattended -nop4 -nosplash -nullrhi -NoMutex -stdout -FullStdOutLogOutput -ExecutePythonScript="' + $PythonScript + '" -AbsLog="' + $EngineLog + '"'
$Process = $null
$Finished = $false
try {
	$Process = Start-Process -FilePath $EditorCommandlet -ArgumentList $ArgumentLine `
        -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $StdoutLog -RedirectStandardError $StderrLog
    $Finished = $Process.WaitForExit($TimeoutSeconds * 1000)
    if (-not $Finished -and -not $Process.HasExited) {
        Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue
        $Process.WaitForExit()
    }
    $Process.Refresh()
} finally {
    if ($null -eq $PreviousReportOverride) {
        Remove-Item Env:\NAIWA_P4_BETA6_IMPORT_REPORT -ErrorAction SilentlyContinue
    } else {
        $env:NAIWA_P4_BETA6_IMPORT_REPORT = $PreviousReportOverride
    }
}

$FreshReport = Test-Path -LiteralPath $ImportReport -PathType Leaf
if ($FreshReport) {
    $FreshReport = (Get-Item -LiteralPath $ImportReport).LastWriteTimeUtc -gt $PreviousReportUtc
}
if (-not $Finished -or -not $Process -or -not $Process.HasExited) {
    throw "Unreal CSV import did not finish. Check $EngineLog, $StdoutLog, and $StderrLog"
}
if (-not $FreshReport) {
    throw "Unreal CSV import did not write a fresh report. Check $EngineLog, $StdoutLog, and $StderrLog"
}

$Report = Get-Content -LiteralPath $ImportReport -Raw -Encoding UTF8 | ConvertFrom-Json
if ($Report.status -ne 'passed' -or $Report.imported_tables.chapters.row_count -ne 6 -or
    $Report.imported_tables.themes.row_count -ne 6 -or $Report.imported_tables.wave_spawns.row_count -ne 66 -or
    $Report.imported_tables.enemies.row_count -ne 12 -or $Report.imported_tables.bosses.row_count -ne 2 -or
    $Report.imported_tables.boss_attacks.row_count -ne 8) {
    throw "P4 Beta6 DataTable import/readback did not match expected schema and rows. See $ImportReport"
}
if ($null -ne $Process.ExitCode -and [int]$Process.ExitCode -ne 0) {
    Write-Warning "Unreal returned exit code $($Process.ExitCode) after writing a complete passed import report; inspect $EngineLog for shutdown diagnostics."
}

[pscustomobject]@{
    Status = $Report.status
    ChapterRows = $Report.imported_tables.chapters.row_count
    ThemeRows = $Report.imported_tables.themes.row_count
    WaveSpawnRows = $Report.imported_tables.wave_spawns.row_count
    EnemyRows = $Report.imported_tables.enemies.row_count
    BossRows = $Report.imported_tables.bosses.row_count
    BossAttackRows = $Report.imported_tables.boss_attacks.row_count
    BuildLog = $BuildLog
    EngineLog = $EngineLog
    ImportReport = $ImportReport
} | Format-List
