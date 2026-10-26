[CmdletBinding()]
param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$buildScript = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
$editorCommand = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$m1aDirectory = Join-Path $repoRoot '.workbuddy/work/20261019_P2测试基线/M1a'
$ubaDirectory = Join-Path $m1aDirectory 'uba'
$fixtureFile = Join-Path $repoRoot 'UE5/Records/p2_probe_fixtures/p2_ballistics_fixture.json'
$designFixtureFile = Join-Path $repoRoot 'UE5/Records/p2_probe_fixtures/p2_design_fixture.json'
$bootstrapScript = Join-Path $PSScriptRoot 'Scripts/p2_probe_bootstrap.py'
$reportFile = Join-Path $m1aDirectory 'p2_engine_result.json'
$logFile = Join-Path $m1aDirectory 'probe_traj.log'

foreach ($requiredPath in @($projectFile, $buildScript, $editorCommand, $fixtureFile, $designFixtureFile, $bootstrapScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required P2 M1a input not found: $requiredPath" }
}
New-Item -ItemType Directory -Path $m1aDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ubaDirectory -Force | Out-Null
Remove-Item -LiteralPath $reportFile -Force -ErrorAction SilentlyContinue

Write-Output 'Building Nai_LongEditor Win64 Development...'
& $buildScript Nai_LongEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoUBA -MaxParallelActions=4 "-UBARootDir=$ubaDirectory"
if ($LASTEXITCODE -ne 0) { throw "P2 M1a native build failed with exit code $LASTEXITCODE" }

$startedUtc = [DateTime]::UtcNow
$fixtureHash = (Get-FileHash -LiteralPath $fixtureFile -Algorithm SHA256).Hash.ToLowerInvariant()
$designFixtureHash = (Get-FileHash -LiteralPath $designFixtureFile -Algorithm SHA256).Hash.ToLowerInvariant()
$arguments = @(
    $projectFile,
    '-run=PythonScript', "-script=$bootstrapScript",
    '-unattended', '-nop4', '-nosplash', '-NullRHI', '-NaiwaP2ProbeTraj',
    "-NaiwaP2Fixture=$fixtureFile",
    "-NaiwaP2DesignFixture=$designFixtureFile",
    "-NaiwaP2FixtureHash=$fixtureHash",
    "-NaiwaP2DesignFixtureHash=$designFixtureHash",
    "-NaiwaP2Report=$reportFile",
    "-abslog=$logFile"
)
& $editorCommand @arguments
$editorExitCode = $LASTEXITCODE
if (-not (Test-Path -LiteralPath $reportFile)) {
    throw "ProbeTraj exited without a fresh report (exit $editorExitCode); inspect $logFile"
}
$reportItem = Get-Item -LiteralPath $reportFile
if ($reportItem.LastWriteTimeUtc -lt $startedUtc) { throw "ProbeTraj left a stale report; inspect $logFile" }
$fixtureHashAfter = (Get-FileHash -LiteralPath $fixtureFile -Algorithm SHA256).Hash.ToLowerInvariant()
$designFixtureHashAfter = (Get-FileHash -LiteralPath $designFixtureFile -Algorithm SHA256).Hash.ToLowerInvariant()
if ($fixtureHashAfter -ne $fixtureHash -or $designFixtureHashAfter -ne $designFixtureHash) {
    throw 'A read-only M1a fixture changed while ProbeTraj was running.'
}
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
if ($editorExitCode -ne 0 -or $report.verdict -ne 'PASS') {
    throw "ProbeTraj failed: exit=$editorExitCode verdict=$($report.verdict) first_fail=$($report.first_fail); inspect $reportFile and $logFile"
}
Write-Output "P2 M1a ProbeTraj PASS; report: $reportFile"
