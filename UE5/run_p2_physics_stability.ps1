[CmdletBinding()]
param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateSet('2-1', '2-2', '2-3')]
    [string]$Level = '2-2',
    [string]$Map = '',
    [string]$Tag = '',
    [switch]$SkipBuild,
    [switch]$IncludePhysicalGoals
)

# P2 塔自稳诊断运行器（F0/F1）。
# 通过 UnrealEditor 的隐藏 Game 运行时加载源码地图；不运行 UAT、Cook、Stage 或 Package。
# 跑满物理测试时长后由 GameMode 输出结果并退出。
# 用法：run_p2_physics_stability.ps1 -Level 2-1 -Map /Game/Naiwa/P2/Maps/L_NaiwaP2_21_Prototype -Tag f0_21

$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$buildScript = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
$editorExecutable = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor.exe'
if (-not $Map) {
    # Current source content includes only L_NaiwaP2_22 and the isolated 2-1 prototype.
    # NaiwaP2Level selects the generated scene, so use the existing non-prototype map by default.
    $Map = '/Game/Naiwa/P2/Maps/L_NaiwaP2_22'
}
$relativeMapPath = $Map -replace '^/Game/', ''
$sourceMap = Join-Path $PSScriptRoot "NaiwaP0/Content/$relativeMapPath.umap"
$testDirectory = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../.workbuddy/work/20261020_全物理改造/stability'))
$ubaDirectory = Join-Path $testDirectory 'uba'
$suffix = if ($Tag) { "_$Tag" } else { "_$($Level -replace '-','')" }
$logFile = [System.IO.Path]::GetFullPath((Join-Path $testDirectory "stability$suffix.log"))

foreach ($requiredPath in @($projectFile, $buildScript, $editorExecutable, $sourceMap)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required stability input not found: $requiredPath" }
}
New-Item -ItemType Directory -Path $testDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ubaDirectory -Force | Out-Null

if (-not $SkipBuild) {
    Write-Output 'Building Nai_LongEditor Win64 Development source target (no cook/package)...'
    & $buildScript Nai_LongEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=4 "-UBARootDir=$ubaDirectory"
    if ($LASTEXITCODE -ne 0) { throw "Nai_LongEditor build failed (exit $LASTEXITCODE)" }
}

Remove-Item -LiteralPath $logFile -Force -ErrorAction SilentlyContinue
$editorArguments = @(
    $projectFile, $sourceMap, '-game', '-PIEVIACONSOLE', '-Multiprocess',
    '-unattended', '-nop4', '-nosplash', '-NullRHI', '-NoSound', '-stdout', '-log',
    '-NaiwaP2PhysicsStability', "-NaiwaP2Level=$Level", "-abslog=$logFile"
)
if ($IncludePhysicalGoals) {
    if ($Level -notin @('2-1', '2-2', '2-3')) {
        throw 'Physical goal stability is currently supported for Levels 2-1, 2-2, and 2-3 only.'
    }
    $editorArguments += '-NaiwaP2PhysicsLevel'
}
$process = Start-Process -FilePath $editorExecutable -ArgumentList $editorArguments `
    -WorkingDirectory (Split-Path -Parent $editorExecutable) -WindowStyle Hidden -PassThru -Wait

$output = if (Test-Path -LiteralPath $logFile) { Get-Content -LiteralPath $logFile -Raw } else { '' }
$summary = [regex]::Matches($output, 'NAIWA_P2_STABILITY pass=\d[^\r\n]*') | Select-Object -Last 1
$blocks = [regex]::Matches($output, 'NAIWA_P2_STABILITY_BLOCK [^\r\n]*') | ForEach-Object { $_.Value }
$goals = [regex]::Matches($output, 'NAIWA_P2_STABILITY_GOAL [^\r\n]*') | ForEach-Object { $_.Value }
$welds = [regex]::Matches($output, 'NAIWA_P2_(?:WELD|FLOATING_BLOCK|BRIDGE_BLOCK|BLOCK_PINNED)[^\r\n]*') | ForEach-Object { $_.Value }

Write-Output "level=$Level map=$Map exit=$($process.ExitCode) log=$logFile"
foreach ($w in $welds) { Write-Output "  $w" }
foreach ($b in $blocks) { Write-Output "  $b" }
foreach ($g in $goals) { Write-Output "  $g" }
if (-not $summary) {
    throw "NAIWA_P2_STABILITY result missing; the editor runtime did not complete the probe. Process exit=$($process.ExitCode), log=$logFile"
}
Write-Output "  $($summary[0].Value)"
if ($process.ExitCode -ne 0) { exit $process.ExitCode }
if ($summary[0].Value -notmatch 'pass=1(?:\s|$)') { exit 1 }
exit $process.ExitCode
