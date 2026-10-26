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
$mapPath = '/Game/Naiwa/P2/Maps/L_NaiwaP2_22'
$testDirectory = Join-Path $repoRoot '.workbuddy/work/20261019_P2测试基线/M2b'
$ubaDirectory = Join-Path $testDirectory 'uba'
$cookedMap = Join-Path $PSScriptRoot 'NaiwaP0/Saved/Cooked/Windows/Nai_Long/Content/Naiwa/P2/Maps/L_NaiwaP2_22.umap'
$cookLog = Join-Path $testDirectory 'cook_22.log'
$standardLog = Join-Path $testDirectory 'playtest_22_standard.log'
$refundLog = Join-Path $testDirectory 'playtest_22_refund.log'
$resultPath = Join-Path $testDirectory 'p2_m2b_runtime_result.json'

foreach ($requiredPath in @($projectFile, $buildScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required P2 M2b input not found: $requiredPath" }
}
if (-not (Test-Path -LiteralPath $editorCommand)) { throw "UE editor commandlet not found: $editorCommand" }
New-Item -ItemType Directory -Path $testDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ubaDirectory -Force | Out-Null

Write-Output 'Building the source Development game target (no package)...'
& $buildScript Nai_Long Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=4 "-UBARootDir=$ubaDirectory"
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $gameExecutable)) {
    throw "P2 M2b source game build failed (exit $LASTEXITCODE)"
}

$sourceMap = Join-Path $PSScriptRoot 'NaiwaP0/Content/Naiwa/P2/Maps/L_NaiwaP2_22.umap'
if (-not (Test-Path -LiteralPath $sourceMap)) { throw "P2 M2b source map not found: $sourceMap" }
Write-Output 'Cooking the 2-2 map and runtime-loaded content only (no UAT, staging, or package)...'
Remove-Item -LiteralPath $cookLog -Force -ErrorAction SilentlyContinue
$cookArguments = @(
    $projectFile, '-run=Cook', '-TargetPlatform=Windows', "-Map=$mapPath",
    '-CookDir=/Game/Naiwa/P2/Art', '-CookDir=/Game/Naiwa/P1/Materials',
    '-CookDir=/Game/Naiwa/Materials', '-SkipEditorContent', '-Iterate',
    '-unattended', '-nop4', '-nosplash', '-NullRHI', "-abslog=$cookLog"
)
$cookProcess = Start-Process -FilePath $editorCommand -ArgumentList $cookArguments -WindowStyle Hidden -PassThru -Wait
if ($cookProcess.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $cookedMap)) {
    throw "P2 M2b targeted Development cook failed (exit $($cookProcess.ExitCode)); inspect $cookLog"
}

Remove-Item -LiteralPath $standardLog,$refundLog -Force -ErrorAction SilentlyContinue
$standardArguments = @(
    "-Map=$mapPath", '-unattended', '-nop4', '-nosplash', '-NullRHI', '-NaiwaP2_22PlayTest',
    "-abslog=$standardLog"
)
$standardProcess = Start-Process -FilePath $gameExecutable -ArgumentList $standardArguments -WindowStyle Hidden -PassThru -Wait
$standardOutput = if (Test-Path -LiteralPath $standardLog) { Get-Content -LiteralPath $standardLog -Raw } else { '' }
$mouseSkillPassed = $standardOutput.Contains('NAIWA_P2_SKILL_TRIGGER egg=MOUSE tick=72 split_paths=3')
$splitVisualsPassed = $standardOutput.Contains('NAIWA_P2_SPLIT_VISUAL_CHECK pass=1 early_path=1 children=3')
$chickSkillPassed = $standardOutput.Contains('NAIWA_P2_SKILL_TRIGGER egg=CHICK tick=46 split_paths=0')
$standardChainPassed = $standardOutput.Contains('NAIWA_P2_22_PLAYTEST_PASS shots=3 goals=0 tokens=2 retry=1')
$standardPassed = $standardProcess.ExitCode -eq 0 -and $mouseSkillPassed -and $splitVisualsPassed -and $chickSkillPassed -and $standardChainPassed

$refundArguments = @(
    "-Map=$mapPath", '-unattended', '-nop4', '-nosplash', '-NullRHI', '-NaiwaP2_22RefundPlayTest',
    "-abslog=$refundLog"
)
$refundProcess = Start-Process -FilePath $gameExecutable -ArgumentList $refundArguments -WindowStyle Hidden -PassThru -Wait
$refundOutput = if (Test-Path -LiteralPath $refundLog) { Get-Content -LiteralPath $refundLog -Raw } else { '' }
$tokenRefundPassed = $refundOutput.Contains('NAIWA_P2_22_REFUND_CHECK pass=1 shots=0 goals=10 tokens=5 retry=0')
$resetPassed = $refundOutput.Contains('NAIWA_P2_22_REFUND_PLAYTEST_PASS shots=0 goals=10 tokens=5 retry=1')
$refundPassed = $refundProcess.ExitCode -eq 0 -and $tokenRefundPassed -and $resetPassed

$report = [ordered]@{
    verdict = if ($standardPassed -and $refundPassed) { 'PASS' } else { 'FAIL' }
    map = $mapPath
    engine_target = 'Nai_Long Win64 Development source target'
    targeted_development_cook = (Test-Path -LiteralPath $cookedMap)
    packaged = $false
    standard_chain = [ordered]@{
        pass = $standardChainPassed
        mouse_split_at_tick_72 = $mouseSkillPassed
        split_children_hold_terminal_pose = $splitVisualsPassed
        chick_flap_at_tick_46 = $chickSkillPassed
        process_exit_code = $standardProcess.ExitCode
        log = $standardLog
    }
    zero_kill_refund_and_R = [ordered]@{
        pass = $refundPassed
        token_and_shot_restored = $tokenRefundPassed
        R_restores_tokens_and_entitlement = $resetPassed
        process_exit_code = $refundProcess.ExitCode
        log = $refundLog
    }
}
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $resultPath -Encoding utf8

if ($report.verdict -ne 'PASS') { throw "P2 M2b runtime test failed; inspect $resultPath and the logs" }
Write-Output "P2 M2b standard chain, click skills, zero-kill token refund, and R checks PASS; report=$resultPath"
