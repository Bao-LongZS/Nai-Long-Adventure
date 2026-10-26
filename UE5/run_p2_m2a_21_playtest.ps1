[CmdletBinding()]
param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$buildScript = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
$gameExecutable = Join-Path $PSScriptRoot 'NaiwaP0/Binaries/Win64/Nai_Long.exe'
$mapPath = '/Game/Naiwa/P2/Maps/L_NaiwaP2_21_Prototype'
$testDirectory = Join-Path $repoRoot '.workbuddy/work/20261019_P2测试基线/M2a'
$ubaDirectory = Join-Path $testDirectory 'uba'
$testLog = Join-Path $testDirectory 'playtest_21_structure.log'
$refundLog = Join-Path $testDirectory 'playtest_21_refund.log'
$testReport = Join-Path $testDirectory 'playtest_21_structure_result.json'

foreach ($requiredPath in @($projectFile, $buildScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required P2 M2a input not found: $requiredPath" }
}
New-Item -ItemType Directory -Path $testDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ubaDirectory -Force | Out-Null

Write-Output 'Building the source Development game target (no package)...'
& $buildScript Nai_Long Win64 Development "-Project=$projectFile" -WaitMutex -NoUBA -MaxParallelActions=4 "-UBARootDir=$ubaDirectory"
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $gameExecutable)) {
    throw "P2 M2a source game build failed (exit $LASTEXITCODE)"
}

Remove-Item -LiteralPath $testLog -Force -ErrorAction SilentlyContinue

$gameArguments = @(
    "-Map=$mapPath", '-unattended', '-nop4', '-nosplash', '-NullRHI',
    '-NaiwaP2StructurePlayTest', "-abslog=$testLog"
)
$gameProcess = Start-Process -FilePath $gameExecutable -ArgumentList $gameArguments -WindowStyle Hidden -PassThru -Wait
$testOutput = if (Test-Path -LiteralPath $testLog) { Get-Content -LiteralPath $testLog -Raw } else { '' }
$supportDebrisPassed = $testOutput.Contains('NAIWA_P2_STRUCTURE_CHECK support_debris=1 upper_stack_dropped=1 remaining_goals=1')
$collapseCompleted = $testOutput.Contains('NAIWA_P2_STRUCTURE_COLLAPSE_DONE shot=1') -and
    $testOutput.Contains('NAIWA_P2_STRUCTURE_COLLAPSE_DONE shot=2')
$standardChainPassed = $testOutput.Contains('NAIWA_P2_STRUCTURE_PLAYTEST_PASS shots=2 goals=0 pillar_debris=1 structure_reset=1')
$structureResetPassed = $testOutput.Contains('NAIWA_P2_STRUCTURE_RESET_CHECK pass=1 shots=0 goals=4')
$structureProcessPassed = $gameProcess.ExitCode -eq 0 -and $supportDebrisPassed -and $collapseCompleted -and
    $standardChainPassed -and $structureResetPassed

Remove-Item -LiteralPath $refundLog -Force -ErrorAction SilentlyContinue
$refundArguments = @(
    "-Map=$mapPath", '-unattended', '-nop4', '-nosplash', '-NullRHI',
    '-NaiwaP2RefundPlayTest', "-abslog=$refundLog"
)
$refundProcess = Start-Process -FilePath $gameExecutable -ArgumentList $refundArguments -WindowStyle Hidden -PassThru -Wait
$refundOutput = if (Test-Path -LiteralPath $refundLog) { Get-Content -LiteralPath $refundLog -Raw } else { '' }
$refundScenarioPassed = $refundOutput.Contains('NAIWA_P2_REFUND no_new_goals=1 shot=1 t_d3_hit=1 structure_restored=1 shots=0 entitlement=0')
$refundStatePassed = $refundOutput.Contains('NAIWA_P2_REFUND_CHECK pass=1 scene_restored=1 actor_restored=1 shots=0 goals=4 entitlement=0')
$refundPlayTestPassed = $refundOutput.Contains('NAIWA_P2_REFUND_PLAYTEST_PASS')
$refundPassed = $refundProcess.ExitCode -eq 0 -and $refundScenarioPassed -and $refundStatePassed -and $refundPlayTestPassed
$pass = $structureProcessPassed -and $refundPassed

$report = @{
    verdict = if ($pass) { 'PASS' } else { 'FAIL' }
    map = $mapPath
    scenario = '2-1 standard two-shot chain, persistent structure debris, R reset, and zero-kill structure-hit refund'
    checks = @(
        @{ id = 'first_shot_collapses_upper_stack_and_keeps_only_P1'; pass = $supportDebrisPassed },
        @{ id = 'both_structure_effects_finish'; pass = $collapseCompleted },
        @{ id = 'second_shot_clears_level_and_retains_pillar_debris'; pass = $standardChainPassed },
        @{ id = 'R_restores_all_structure_boxes_and_targets'; pass = $structureResetPassed },
        @{ id = 'first_zero_kill_structure_hit_restores_the_full_shot_transaction'; pass = $refundPassed }
    )
    processExitCode = $gameProcess.ExitCode
    refundProcessExitCode = $refundProcess.ExitCode
    log = $testLog
    refundLog = $refundLog
}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $testReport -Encoding utf8

if (-not $pass) { throw "P2 M2a structure playtest failed; inspect $testLog" }
Write-Output "P2 M2a 2-1 structure, refund, and R checks PASS; report=$testReport; logs=$testLog,$refundLog"
