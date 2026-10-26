[CmdletBinding()]
param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$buildScript = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
$editorCommand = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$gameExecutable = Join-Path $PSScriptRoot 'NaiwaP0/Binaries/Win64/Nai_Long.exe'
$m1bDirectory = Join-Path $repoRoot '.workbuddy/work/20261019_P2测试基线/M1b'
$ubaDirectory = Join-Path $m1bDirectory 'uba'
$mapScript = Join-Path $PSScriptRoot 'Scripts/p2_build_21_prototype.py'
$mapReport = Join-Path $PSScriptRoot 'Records/p2_m1b_map_result.json'
$playLog = Join-Path $m1bDirectory 'playtest_fail_retry.log'
$mapPath = '/Game/Naiwa/P2/Maps/L_NaiwaP2_21_Prototype'

foreach ($requiredPath in @($projectFile, $buildScript, $editorCommand, $mapScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required P2 M1b input not found: $requiredPath" }
}
New-Item -ItemType Directory -Path $m1bDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ubaDirectory -Force | Out-Null

Write-Output 'Building Nai_Long Win64 Development (source, no package)...'
& $buildScript Nai_Long Win64 Development "-Project=$projectFile" -WaitMutex -NoUBA -MaxParallelActions=4 "-UBARootDir=$ubaDirectory"
if ($LASTEXITCODE -ne 0) { throw "P2 M1b playable prototype build failed with exit code $LASTEXITCODE" }
if (-not (Test-Path -LiteralPath $gameExecutable)) { throw "P2 M1b Development executable not found: $gameExecutable" }

Remove-Item -LiteralPath $mapReport -Force -ErrorAction SilentlyContinue
$mapLog = Join-Path $m1bDirectory 'build_21_map.log'
$mapArguments = @(
    $projectFile,
    '-run=PythonScript', "-script=$mapScript",
    '-unattended', '-nop4', '-nosplash', '-NullRHI',
    "-abslog=$mapLog"
)
& $editorCommand @mapArguments
$mapExitCode = $LASTEXITCODE
$mapResult = $null
if (Test-Path -LiteralPath $mapReport) {
    $mapResult = Get-Content -LiteralPath $mapReport -Raw | ConvertFrom-Json
} elseif (Test-Path -LiteralPath $mapLog) {
    $mapLogText = Get-Content -LiteralPath $mapLog -Raw
    $mapStatusLine = [regex]::Match($mapLogText, 'NAIWA_P2_M1B_MAP_OK (\{[^\r\n]+\})')
    if ($mapStatusLine.Success) {
        $mapResult = $mapStatusLine.Groups[1].Value | ConvertFrom-Json
        $mapResult | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $mapReport -Encoding utf8
    }
}
if ($mapExitCode -ne 0 -or -not $mapResult) {
    throw "P2 M1b map build failed (exit $mapExitCode); inspect $mapLog and $mapReport"
}
if ($mapResult.status -notin @('new_isolated_map_saved', 'existing_owned_map_validated')) {
    throw "P2 M1b map validation failed: $($mapResult.status)"
}

Remove-Item -LiteralPath $playLog -Force -ErrorAction SilentlyContinue
$gameArguments = @(
    "-Map=$mapPath", '-unattended', '-nop4', '-nosplash', '-NullRHI',
    '-NaiwaP2PlayTest', "-abslog=$playLog"
)
$gameProcess = Start-Process -FilePath $gameExecutable -ArgumentList $gameArguments -WindowStyle Hidden -PassThru -Wait
$gameExitCode = $gameProcess.ExitCode
$playTestPassed = $false
$playOutput = ''
for ($attempt = 0; $attempt -lt 10; $attempt++) {
    if (Test-Path -LiteralPath $playLog) {
        $playOutput = Get-Content -LiteralPath $playLog -Raw
        if ($playOutput.Contains('NAIWA_P2_PLAYTEST_PASS')) {
            $playTestPassed = $true
            break
        }
    }
    Start-Sleep -Milliseconds 500
}
$zeroHitResolves = [regex]::Matches($playOutput, 'NAIWA_P2_SHOT_RESOLVE shot=[1-4] hits=0 killed=0 remaining_goals=4').Count
$firstMissRefunded = $playOutput.Contains('NAIWA_P2_REFUND no_new_goals=1 shot=1 t_d3_hit=0 structure_restored=1 shots=0 entitlement=0')
$retryResetPassed = $playOutput.Contains('NAIWA_P2_RETRY_RESET shots=0 goals=4')
if ($gameExitCode -ne 0 -or -not $playTestPassed -or -not $firstMissRefunded -or
    $zeroHitResolves -ne 3 -or -not $retryResetPassed) {
    throw "P2 M1b fail/retry playtest failed (exit $gameExitCode); inspect $playLog"
}

$playReportFile = Join-Path $m1bDirectory 'playtest_fail_retry_result.json'
$playReport = @{
    verdict = 'PASS'
    ruleset_version = '2026-10-19-click-v2'
    engine_build = $mapResult.engine
    map = $mapPath
    scenario = 'first deliberate miss consumes the level refund, three charged misses cause failure, then R retry'
    checks = @(
        @{ id = 'three_zero_hit_shots_resolve'; pass = ($zeroHitResolves -eq 3); actual = $zeroHitResolves; expected = 3; tol = 0 },
        @{ id = 'first_zero_hit_is_refunded_once'; pass = $firstMissRefunded; actual = $firstMissRefunded; expected = $true; tol = 0 },
        @{ id = 'R_restores_initial_attempt'; pass = $retryResetPassed; actual = 'AIMING; shots=0; goals=4'; expected = 'AIMING; shots=0; goals=4'; tol = 0 }
    )
    log = $playLog
}
$playReport | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $playReportFile -Encoding utf8

Write-Output "P2 M1b fail/retry playtest PASS; map=$mapPath; report=$playReportFile; log=$playLog"
