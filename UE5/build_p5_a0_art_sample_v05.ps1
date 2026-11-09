[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$projectFile = Join-Path $projectRoot 'NaiwaP0/Nai_Long.uproject'
$ubaRootDir = Join-Path (Split-Path -Parent $projectFile) 'Intermediate/P5BackgroundFixUBA'
$buildScript = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
$editorCommand = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$mapScript = Join-Path $projectRoot 'Scripts/p5_build_a0_art_sample_v05.py'
$atlas = Join-Path $projectRoot 'SourceAssets/P5/Characters/Exports/C_Naiwa_Ski_v01_WIP.png'
$mountAtlas = Join-Path $projectRoot 'SourceAssets/P5/Environment/Exports/C_Kangaroo_Ride_v04_WIP.png'
$report = Join-Path $projectRoot 'Records/p5_p6_plan/evidence/P5_A0/P5_A0_EditorSample_v05_build_report.json'
$log = Join-Path $projectRoot 'Records/p5_p6_plan/evidence/P5_A0/P5_A0_EditorSample_v05_build.log'
$mapAsset = Join-Path $projectRoot 'NaiwaP0/Content/Naiwa/P5/Maps/L_NaiwaP5_A0ArtSample_v05.umap'
$evidenceDir = Split-Path -Parent $report
$runId = Get-Date -Format 'yyyyMMdd_HHmmss_fff'
$stdoutLog = Join-Path $evidenceDir "P5_A0_EditorSample_v05_stdout_$runId.log"
$stderrLog = Join-Path $evidenceDir "P5_A0_EditorSample_v05_stderr_$runId.log"
$attemptReport = Join-Path $evidenceDir "P5_A0_EditorSample_v05_attempt_$runId.json"
$editorBuildLog = Join-Path $projectRoot "Records/p5_m2/P5_A0_BackgroundProfileFix_EditorBuild_$runId.log"
$gameBuildLog = Join-Path $projectRoot "Records/p5_m2/P5_A0_BackgroundProfileFix_GameBuild_$runId.log"

foreach ($requiredPath in @($projectFile, $buildScript, $editorCommand, $mapScript, $atlas, $mountAtlas)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required P5 A0 input not found: $requiredPath" }
}

if (-not $SkipBuild) {
    New-Item -ItemType Directory -Path $ubaRootDir -Force | Out-Null
    & $buildScript Nai_LongEditor Win64 Development "-Project=$projectFile" -NoUBA -NoSharedPCH -NoHotReloadFromIDE -NoLiveCoding -WaitMutex -MaxParallelActions=4 "-UBARootDir=$ubaRootDir" *> $editorBuildLog
    $editorBuildExitCode = $LASTEXITCODE
    if ($editorBuildExitCode -ne 0) { throw "P5 A0 Editor build failed with exit code $editorBuildExitCode; inspect $editorBuildLog" }

    & $buildScript Nai_Long Win64 Development "-Project=$projectFile" -NoUBA -NoSharedPCH -NoHotReloadFromIDE -NoLiveCoding -WaitMutex -MaxParallelActions=4 "-UBARootDir=$ubaRootDir" *> $gameBuildLog
    $gameBuildExitCode = $LASTEXITCODE
    if ($gameBuildExitCode -ne 0) { throw "P5 A0 Game build failed with exit code $gameBuildExitCode; inspect $gameBuildLog" }
} else {
    $editorBuildExitCode = $null
    $gameBuildExitCode = $null
}

$reportBefore = if (Test-Path -LiteralPath $report) { (Get-Item -LiteralPath $report).LastWriteTimeUtc } else { $null }
$mapBefore = if (Test-Path -LiteralPath $mapAsset) { (Get-Item -LiteralPath $mapAsset).LastWriteTimeUtc } else { $null }
$startedAtUtc = [DateTime]::UtcNow
$process = $null
$launchError = $null
try {
    $arguments = @(
        $projectFile,
        '-run=pythonscript',
        "-script=$mapScript",
        '-P5BuildMap',
        '-unattended',
        '-nop4',
        '-nosplash',
        '-NullRHI',
        '-UTF8Output',
        '-stdout',
        '-FullStdOutLogOutput',
        "-abslog=$log"
    )
    $process = Start-Process -FilePath $editorCommand -ArgumentList $arguments -NoNewWindow `
        -PassThru -Wait -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog
} catch {
    $launchError = $_.Exception.Message
}

$exitCode = if ($process) { $process.ExitCode } else { -1 }
$reportExists = Test-Path -LiteralPath $report
$mapExists = Test-Path -LiteralPath $mapAsset
$reportAfter = if ($reportExists) { (Get-Item -LiteralPath $report).LastWriteTimeUtc } else { $null }
$mapAfter = if ($mapExists) { (Get-Item -LiteralPath $mapAsset).LastWriteTimeUtc } else { $null }
$reportFresh = $reportExists -and ($null -eq $reportBefore -or $reportAfter -gt $reportBefore)
$mapFresh = $mapExists -and ($null -eq $mapBefore -or $mapAfter -gt $mapBefore)

$attempt = @{
    status = if ($exitCode -eq 0 -and $reportFresh -and $mapFresh) { 'map_rebuilt' } else { 'map_not_rebuilt' }
    started_at_utc = $startedAtUtc.ToString('o')
    command = $editorCommand
    exit_code = $exitCode
    launch_error = $launchError
    editor_build_log = $editorBuildLog
    editor_build_exit_code = $editorBuildExitCode
    game_build_log = $gameBuildLog
    game_build_exit_code = $gameBuildExitCode
    log = $log
    stdout = $stdoutLog
    stderr = $stderrLog
    report = $report
    report_fresh = $reportFresh
    report_last_write_utc = if ($reportAfter) { $reportAfter.ToString('o') } else { $null }
    map_asset = $mapAsset
    map_fresh = $mapFresh
    map_last_write_utc = if ($mapAfter) { $mapAfter.ToString('o') } else { $null }
    previous_map_last_write_utc = if ($mapBefore) { $mapBefore.ToString('o') } else { $null }
}
$attempt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $attemptReport -Encoding UTF8

if ($exitCode -ne 0 -or -not $reportFresh -or -not $mapFresh) {
    throw "P5 A0 map was not rebuilt. Existing evidence was preserved; inspect $stdoutLog, $stderrLog, $log, and $attemptReport"
}

$result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
if ($result.status -ne 'saved_pending_visible_editor_review' -or
    $result.game_mode -ne '/Script/NaiwaRuntime.NaiwaP5M2PrototypeGameMode' -or
    $result.texture_count -ne 12 -or
    $result.background_components_runtime -ne 9) {
	throw "P5 A0 sample metadata did not pass validation: $report; attempt details: $attemptReport"
}
Write-Output "P5 A0 v05 art sample map ready: $mapAsset"
Write-Output "WIP textures imported: $($result.texture_count); GameMode: $($result.game_mode)"
Write-Output "Build report: $report"
Write-Output "Attempt report: $attemptReport"
