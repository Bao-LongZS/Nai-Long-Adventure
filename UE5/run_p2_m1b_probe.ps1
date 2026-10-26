[CmdletBinding()]
param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$buildScript = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
$editorCommand = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$m1bDirectory = Join-Path $repoRoot '.workbuddy/work/20261019_P2测试基线/M1b'
$ubaDirectory = Join-Path $m1bDirectory 'uba'
$designFixtureFile = Join-Path $repoRoot 'UE5/Records/p2_probe_fixtures/p2_design_fixture.json'
$bootstrapScript = Join-Path $PSScriptRoot 'Scripts/p2_probe_bootstrap.py'
$reportFile = Join-Path $m1bDirectory 'p2_probe_shot_result.json'
$logFile = Join-Path $m1bDirectory 'probe_shot.log'

foreach ($requiredPath in @($projectFile, $buildScript, $editorCommand, $designFixtureFile, $bootstrapScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required P2 M1b input not found: $requiredPath" }
}
New-Item -ItemType Directory -Path $m1bDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ubaDirectory -Force | Out-Null
Remove-Item -LiteralPath $reportFile -Force -ErrorAction SilentlyContinue

Write-Output 'Building Nai_LongEditor Win64 Development...'
& $buildScript Nai_LongEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=4 "-UBARootDir=$ubaDirectory"
if ($LASTEXITCODE -ne 0) { throw "P2 M1b native build failed with exit code $LASTEXITCODE" }

$fixtureHash = (Get-FileHash -LiteralPath $designFixtureFile -Algorithm SHA256).Hash.ToLowerInvariant()
$startedUtc = [DateTime]::UtcNow
$arguments = @(
    $projectFile,
    '-run=PythonScript', "-script=$bootstrapScript",
    '-unattended', '-nop4', '-nosplash', '-NullRHI', '-NaiwaP2ProbeShot',
    "-NaiwaP2DesignFixture=$designFixtureFile",
    "-NaiwaP2DesignFixtureHash=$fixtureHash",
    "-NaiwaP2Report=$reportFile",
    "-abslog=$logFile"
)
& $editorCommand @arguments
$editorExitCode = $LASTEXITCODE
if (-not (Test-Path -LiteralPath $reportFile)) {
    throw "ProbeShot exited without a fresh report (exit $editorExitCode); inspect $logFile"
}
$reportItem = Get-Item -LiteralPath $reportFile
if ($reportItem.LastWriteTimeUtc -lt $startedUtc) { throw "ProbeShot left a stale report; inspect $logFile" }
$fixtureHashAfter = (Get-FileHash -LiteralPath $designFixtureFile -Algorithm SHA256).Hash.ToLowerInvariant()
if ($fixtureHashAfter -ne $fixtureHash) { throw 'The read-only design fixture changed while ProbeShot was running.' }
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
if ($editorExitCode -ne 0 -or $report.verdict -ne 'PASS') {
    throw "ProbeShot failed: exit=$editorExitCode verdict=$($report.verdict) first_fail=$($report.first_fail); inspect $reportFile and $logFile"
}
Write-Output "P2 M1b ProbeShot PASS; report: $reportFile"
