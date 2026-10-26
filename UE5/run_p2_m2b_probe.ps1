[CmdletBinding()]
param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$buildScript = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
$gameExecutable = Join-Path $PSScriptRoot 'NaiwaP0/Binaries/Win64/Nai_Long.exe'
$fixtureFile = Join-Path $repoRoot 'UE5/Records/p2_probe_fixtures/p2_design_fixture.json'
$testDirectory = Join-Path $repoRoot '.workbuddy/work/20261019_P2测试基线/M2b'
$ubaDirectory = Join-Path $testDirectory 'uba'
$reportFile = Join-Path $testDirectory 'p2_probe_shot_result.json'
$logFile = Join-Path $testDirectory 'probe_shot.log'

foreach ($requiredPath in @($projectFile, $buildScript, $fixtureFile)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required P2 M2b input not found: $requiredPath" }
}
New-Item -ItemType Directory -Path $testDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ubaDirectory -Force | Out-Null
Remove-Item -LiteralPath $reportFile -Force -ErrorAction SilentlyContinue

Write-Output 'Building the source Development game target (no package)...'
& $buildScript Nai_Long Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=4 "-UBARootDir=$ubaDirectory"
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $gameExecutable)) {
    throw "P2 M2b source game build failed (exit $LASTEXITCODE)"
}

$fixtureHash = (Get-FileHash -LiteralPath $fixtureFile -Algorithm SHA256).Hash.ToLowerInvariant()
$startedUtc = [DateTime]::UtcNow
$gameArguments = @(
    '-unattended', '-nop4', '-nosplash', '-NullRHI', '-NaiwaP2ProbeShot',
    "-NaiwaP2DesignFixture=$fixtureFile",
    "-NaiwaP2DesignFixtureHash=$fixtureHash",
    "-NaiwaP2Report=$reportFile",
    "-abslog=$logFile"
)
$gameProcess = Start-Process -FilePath $gameExecutable -ArgumentList $gameArguments -WindowStyle Hidden -PassThru -Wait
if (-not (Test-Path -LiteralPath $reportFile)) {
    throw "ProbeShot exited without a fresh report (exit $($gameProcess.ExitCode)); inspect $logFile"
}
$reportItem = Get-Item -LiteralPath $reportFile
if ($reportItem.LastWriteTimeUtc -lt $startedUtc) { throw "ProbeShot left a stale report; inspect $logFile" }
$fixtureHashAfter = (Get-FileHash -LiteralPath $fixtureFile -Algorithm SHA256).Hash.ToLowerInvariant()
if ($fixtureHashAfter -ne $fixtureHash) { throw 'The read-only design fixture changed while ProbeShot was running.' }
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
if ($gameProcess.ExitCode -ne 0 -or $report.verdict -ne 'PASS') {
    throw "ProbeShot failed: exit=$($gameProcess.ExitCode) verdict=$($report.verdict) first_fail=$($report.first_fail); inspect $reportFile and $logFile"
}
Write-Output "P2 M2b ProbeShot PASS; report: $reportFile"
