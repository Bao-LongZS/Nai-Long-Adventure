[CmdletBinding()]
param(
    [ValidateSet('Prepare', 'Play', 'Probe', 'ProbeM2', 'ProbeM3', 'ProbeM4')]
    [string]$Mode = 'Prepare',
    [switch]$SkipBuild,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$projectFile = Join-Path $projectRoot 'NaiwaP0/Nai_Long.uproject'
$buildScript = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
$editorCommand = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor.exe'
$mapScript = Join-Path $projectRoot 'Scripts/p6_build_m1_graybox_map.py'
$mapAsset = Join-Path $projectRoot 'NaiwaP0/Content/Naiwa/P6/Maps/L_NaiwaP6_M1Prototype.umap'
$mapPath = '/Game/Naiwa/P6/Maps/L_NaiwaP6_M1Prototype'
$recordDirectory = Join-Path $projectRoot 'Records/p6_m1'
$mapRecordDirectory = switch ($Mode) {
    'ProbeM3' { Join-Path $projectRoot 'Records/p6_m3'; break }
    'ProbeM4' { Join-Path $projectRoot 'Records/p6_m4'; break }
    default { $recordDirectory }
}
$probeRunStamp = Get-Date -Format 'yyyyMMdd_HHmmss_fff'
$mapReportPrefix = switch ($Mode) {
    'ProbeM3' { 'm3'; break }
    'ProbeM4' { 'm4'; break }
    default { 'm1' }
}
$mapReportSuffix = if ($Mode -in @('ProbeM3', 'ProbeM4')) { "_$probeRunStamp" } else { '' }
$mapReport = Join-Path $mapRecordDirectory "${mapReportPrefix}_map_build_report${mapReportSuffix}.json"
$mapLog = Join-Path $mapRecordDirectory "${mapReportPrefix}_map_build${mapReportSuffix}.log"
$editorBuildLog = Join-Path $mapRecordDirectory "${mapReportPrefix}_editor_build_$probeRunStamp.log"
$gameBuildLog = Join-Path $mapRecordDirectory "${mapReportPrefix}_game_build_$probeRunStamp.log"

foreach ($requiredPath in @($projectFile, $buildScript, $editorCommand, $mapScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required P6 input not found: $requiredPath" }
}
New-Item -ItemType Directory -Path $mapRecordDirectory -Force | Out-Null

if (-not $SkipBuild) {
    Write-Output 'Building Nai_LongEditor Win64 Development...'
    & $buildScript Nai_LongEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=8 2>&1 |
        Tee-Object -FilePath $editorBuildLog
    $editorBuildExitCode = $LASTEXITCODE
    if ($editorBuildExitCode -ne 0) { throw "P6 editor build failed with exit code $editorBuildExitCode; inspect $editorBuildLog" }
    Write-Output 'Building Nai_Long Win64 Development...'
    & $buildScript Nai_Long Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=8 2>&1 |
        Tee-Object -FilePath $gameBuildLog
    $gameBuildExitCode = $LASTEXITCODE
    if ($gameBuildExitCode -ne 0) { throw "P6 game build failed with exit code $gameBuildExitCode; inspect $gameBuildLog" }
    Write-Output "Build logs: $editorBuildLog; $gameBuildLog"
}

if ($Mode -notin @('ProbeM3', 'ProbeM4')) {
    Remove-Item -LiteralPath $mapReport,$mapLog -Force -ErrorAction SilentlyContinue
}
$previousMapReportEnv = $env:NAIWA_P6_MAP_REPORT
try {
    $env:NAIWA_P6_MAP_REPORT = $mapReport
    & $editorCommand $projectFile -run=PythonScript "-script=$mapScript" -unattended -nop4 -nosplash -NoMutex -NullRHI -UTF8Output -stdout -FullStdOutLogOutput "-abslog=$mapLog"
}
finally {
    if ($null -eq $previousMapReportEnv) {
        Remove-Item Env:NAIWA_P6_MAP_REPORT -ErrorAction SilentlyContinue
    }
    else {
        $env:NAIWA_P6_MAP_REPORT = $previousMapReportEnv
    }
}
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $mapReport) -or -not (Test-Path -LiteralPath $mapAsset)) {
	throw "P6 prototype map creation failed; inspect $mapLog and $mapReport"
}
$mapResult = Get-Content -LiteralPath $mapReport -Raw | ConvertFrom-Json
if ($mapResult.status -notin @('new_isolated_map_saved', 'existing_owned_map_validated') -or
    $mapResult.default_game_mode -ne '/Script/NaiwaRuntime.NaiwaP6InvestigationGameMode' -or
    $mapResult.runtime_fixture_stage -ne 'M2' -or $mapResult.interaction_points -ne 14 -or
    $mapResult.clue_cards.Count -ne 6) {
    throw "P6 M2 runtime fixture did not pass map metadata validation: $mapReport"
}

if ($Mode -eq 'Play') {
    $gameArguments = @($projectFile, $mapPath, '-game', '-windowed', '-ResX=1600', '-ResY=900', '-ForceRes', '-nop4', '-nosplash', '-NoMutex')
    Start-Process -FilePath $editor -ArgumentList $gameArguments -WorkingDirectory (Split-Path -Parent $projectFile) -WindowStyle Normal | Out-Null
    Write-Output "P6 M2 launched: $mapPath"
    exit 0
}

if ($Mode -in @('Probe', 'ProbeM2')) {
    $bM2Probe = $Mode -eq 'ProbeM2'
    $probeDirectory = if ($bM2Probe) { Join-Path $projectRoot 'Records/p6_m2' } else { $recordDirectory }
    $probePrefix = if ($bM2Probe) { 'm2' } else { 'm1' }
    $probeMarker = if ($bM2Probe) { 'NAIWA_P6_M2' } else { 'NAIWA_P6_M1' }
    $probeFlag = if ($bM2Probe) { '-NaiwaP6M2Probe' } else { '-NaiwaP6M1Probe' }
    New-Item -ItemType Directory -Path $probeDirectory -Force | Out-Null
    $probeLog = Join-Path $probeDirectory "${probePrefix}_probe.log"
    $probeResult = Join-Path $probeDirectory "${probePrefix}_probe_result.json"
    Remove-Item -LiteralPath $probeLog,$probeResult -Force -ErrorAction SilentlyContinue
    $gameArguments = @($projectFile, $mapPath, '-game', '-NullRHI', '-unattended', '-nop4', '-nosplash', '-NoMutex', $probeFlag, "-abslog=$probeLog")
    $process = Start-Process -FilePath $editorCommand -ArgumentList $gameArguments -WindowStyle Hidden -PassThru -Wait
    $logText = if (Test-Path -LiteralPath $probeLog) { Get-Content -LiteralPath $probeLog -Raw } else { '' }
    $passMatch = [regex]::Match($logText, "$probeMarker`_PASS ([^\r\n]+)")
    $failMatch = [regex]::Match($logText, "$probeMarker`_FAIL ([^\r\n]+)")
    $probeStage = if ($bM2Probe) { 'M2' } else { 'M1' }
    $result = @{
        verdict = $(if ($passMatch.Success -and $process.ExitCode -eq 0) { 'PASS' } else { 'FAIL' })
        map = $mapPath
        probe_stage = $probeStage
        engine = $mapResult.engine
        details = $(if ($passMatch.Success) { $passMatch.Groups[1].Value } elseif ($failMatch.Success) { $failMatch.Groups[1].Value } else { "No P6 $probeStage probe marker in log" })
        exit_code = $process.ExitCode
        log = $probeLog
        user_save_slot_touched = $false
    }
    $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $probeResult -Encoding utf8
    if ($result.verdict -ne 'PASS') { throw "P6 $($result.probe_stage) probe failed; inspect $probeLog and $probeResult" }
    Write-Output "P6 $($result.probe_stage) probe PASS: $($result.details)"
    Write-Output "Report: $probeResult"
    exit 0
}

if ($Mode -eq 'ProbeM3') {
    $probeDirectory = Join-Path $projectRoot 'Records/p6_m3'
    New-Item -ItemType Directory -Path $probeDirectory -Force | Out-Null
    $probeLog = Join-Path $probeDirectory "m3_probe_$probeRunStamp.log"
    $probeResult = Join-Path $probeDirectory "m3_probe_result_$probeRunStamp.json"
    $gameArguments = @($projectFile, $mapPath, '-game', '-NullRHI', '-unattended', '-nop4', '-nosplash', '-NoMutex',
        '-NaiwaP6M3Probe', "-abslog=$probeLog")
    $process = Start-Process -FilePath $editorCommand -ArgumentList $gameArguments -WorkingDirectory (Split-Path -Parent $projectFile) -WindowStyle Hidden -PassThru -Wait
    $logText = if (Test-Path -LiteralPath $probeLog) { Get-Content -LiteralPath $probeLog -Raw } else { '' }
    $passMatch = [regex]::Match($logText, 'NAIWA_P6_M3_PASS ([^\r\n]+)')
    $failMatch = [regex]::Match($logText, 'NAIWA_P6_M3_FAIL ([^\r\n]+)')
    $result = @{
        verdict = $(if ($passMatch.Success -and $process.ExitCode -eq 0) { 'PASS' } else { 'FAIL' })
        map = $mapPath
        probe_stage = 'M3'
        engine = $mapResult.engine
        details = $(if ($passMatch.Success) { $passMatch.Groups[1].Value } elseif ($failMatch.Success) { $failMatch.Groups[1].Value } else { 'No P6 M3 probe marker in log' })
        exit_code = $process.ExitCode
        log = $probeLog
        user_save_slot_touched = $false
    }
    $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $probeResult -Encoding utf8
    if ($result.verdict -ne 'PASS') { throw "P6 M3 probe failed; inspect $probeLog and $probeResult" }
    Write-Output "P6 M3 probe PASS: $($result.details)"
    Write-Output "Report: $probeResult"
    exit 0
}

if ($Mode -eq 'ProbeM4') {
    $probeDirectory = Join-Path $projectRoot 'Records/p6_m4'
    New-Item -ItemType Directory -Path $probeDirectory -Force | Out-Null
    $scenarioResults = @()
    foreach ($scenario in @('Perfect', 'Clear', 'Failure', 'Morning')) {
        $scenarioKey = $scenario.ToLowerInvariant()
        $probeLog = Join-Path $probeDirectory "m4_${scenarioKey}_probe_$probeRunStamp.log"
        $probeResult = Join-Path $probeDirectory "m4_${scenarioKey}_probe_result_$probeRunStamp.json"
        $gameArguments = @($projectFile, $mapPath, '-game', '-NullRHI', '-unattended', '-nop4', '-nosplash', '-NoMutex',
            '-NaiwaP6M4Probe', "-NaiwaP6M4Scenario=$scenario", "-abslog=$probeLog")
        $process = Start-Process -FilePath $editorCommand -ArgumentList $gameArguments -WorkingDirectory (Split-Path -Parent $projectFile) -WindowStyle Hidden -PassThru -Wait
        $logText = if (Test-Path -LiteralPath $probeLog) { Get-Content -LiteralPath $probeLog -Raw } else { '' }
        $marker = "NAIWA_P6_M4_$($scenario.ToUpperInvariant())"
        $passMatch = [regex]::Match($logText, "$marker`_PASS ([^\r\n]+)")
        $failMatch = [regex]::Match($logText, "$marker`_FAIL ([^\r\n]+)")
        $result = @{
            verdict = $(if ($passMatch.Success -and $process.ExitCode -eq 0) { 'PASS' } else { 'FAIL' })
            map = $mapPath
            probe_stage = 'M4'
            scenario = $scenario
            engine = $mapResult.engine
            details = $(if ($passMatch.Success) { $passMatch.Groups[1].Value } elseif ($failMatch.Success) { $failMatch.Groups[1].Value } else { "No $marker marker in log" })
            exit_code = $process.ExitCode
            log = $probeLog
            user_save_slot_touched = $false
        }
        $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $probeResult -Encoding utf8
        $scenarioResults += $result
        Write-Output "P6 M4 $scenario $($result.verdict): $($result.details)"
    }
    $aggregateResult = Join-Path $probeDirectory "m4_probe_result_$probeRunStamp.json"
    $aggregate = @{
        verdict = $(if (@($scenarioResults | Where-Object { $_.verdict -ne 'PASS' }).Count -eq 0) { 'PASS' } else { 'FAIL' })
        probe_stage = 'M4'
        map = $mapPath
        scenarios = $scenarioResults
        user_save_slot_touched = $false
    }
    $aggregate | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $aggregateResult -Encoding utf8
    if ($aggregate.verdict -ne 'PASS') { throw "P6 M4 probes failed; inspect $aggregateResult and scenario logs" }
    Write-Output "Report: $aggregateResult"
    exit 0
}

Write-Output "P6 M2 graybox map ready: $mapAsset"
Write-Output "GameMode: $($mapResult.default_game_mode); runtime pawn: $($mapResult.runtime_pawn)"
Write-Output "Map report: $mapReport"
