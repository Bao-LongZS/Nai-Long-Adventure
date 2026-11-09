[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Project = Join-Path $RepositoryRoot 'UE5\NaiwaP0\Nai_Long.uproject'
$EditorCommandlet = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$EngineLog = Join-Path $EvidenceRoot "P7_Automation_MainProject_$Timestamp.log"
$ConsoleLog = Join-Path $EvidenceRoot "P7_Automation_MainProject_$Timestamp.console.log"
$ReportPath = Join-Path $EvidenceRoot "P7_Automation_MainProject_$Timestamp.json"

$ExpectedP7Tests = @(
    'Naiwa.P7.Board.LevelDefinitionValidation'
    'Naiwa.P7.Board.MappingAcrossResolutions'
    'Naiwa.P7.Combat.AreaDamageAndSlowSupport'
    'Naiwa.P7.Combat.BuildUpgradeObstacleUnlock'
    'Naiwa.P7.Combat.InterferenceStableTargetAndExpiry'
    'Naiwa.P7.Data.AlphaSchemaValidation'
    'Naiwa.P7.Data.AlphaFixtureAssetAssembly'
    'Naiwa.P7.Data.CsvArrayImport'
    'Naiwa.P7.Data.DataTableFixtureAssembly'
    'Naiwa.P7.Presentation.BoardRectResolutionParity'
    'Naiwa.P7.Simulation.CatchUpBacklogIsRetained'
    'Naiwa.P7.Simulation.JarReachesZeroOnLeak'
    'Naiwa.P7.Simulation.RouteMovementAndReplay'
    'Naiwa.P7.Waves.DelayIntervalAndPendingCompletionGate'
)

foreach ($path in @($Project, $EditorCommandlet)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required P7 automation input was not found: $path"
    }
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

$ArgumentLine = @(
    ('"' + $Project + '"'),
    '-unattended', '-nop4', '-nosplash', '-nullrhi', '-NoMutex',
    '-stdout', '-FullStdOutLogOutput',
    '"-ExecCmds=Automation RunTests Naiwa.P7"',
    '"-TestExit=Automation Test Queue Empty"',
    "-AbsLog=$EngineLog"
)
$StartedAt = Get-Date
$Process = Start-Process -FilePath $EditorCommandlet -ArgumentList $ArgumentLine `
    -WorkingDirectory (Split-Path -Parent $Project) -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput $ConsoleLog -RedirectStandardError ($ConsoleLog + '.stderr')
$Finished = $Process.WaitForExit($TimeoutSeconds * 1000)
if (-not $Finished -and -not $Process.HasExited) {
    Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue
    $Process.WaitForExit()
}
$Process.Refresh()
$ExitCode = if ($Finished) { $Process.ExitCode } else { $null }
$LogText = if (Test-Path -LiteralPath $EngineLog -PathType Leaf) {
    Get-Content -LiteralPath $EngineLog -Raw
} else {
    ''
}
$CompletedTests = [regex]::Matches(
    $LogText,
    'Test Completed\. Result=\{Success\} Name=\{[^}]+\} Path=\{(?<Path>Naiwa\.P7\.[^}]+)\}'
) | ForEach-Object { $_.Groups['Path'].Value }
$FailedTests = [regex]::Matches(
    $LogText,
    'Test Completed\. Result=\{Fail\} Name=\{[^}]+\} Path=\{(?<Path>Naiwa\.P7\.[^}]+)\}'
) | ForEach-Object { $_.Groups['Path'].Value }
$MissingTests = @($ExpectedP7Tests | Where-Object { $_ -notin $CompletedTests })
$UnexpectedTests = @($CompletedTests | Where-Object { $_ -notin $ExpectedP7Tests })
$Passed = $Finished -and $ExitCode -eq 0 -and $FailedTests.Count -eq 0 -and
    $MissingTests.Count -eq 0 -and $UnexpectedTests.Count -eq 0
$Report = [ordered]@{
    test = 'P7 automation suite in the main Nai_Long project'
    status = if ($Passed) { 'PASS' } elseif (-not $Finished) { 'TIMEOUT' } else { 'FAIL' }
    startedAt = $StartedAt.ToString('yyyy-MM-dd HH:mm:ssK')
    timeoutSeconds = $TimeoutSeconds
    project = $Project
    processExitCode = $ExitCode
    expectedCount = $ExpectedP7Tests.Count
    completed = @($CompletedTests)
    failed = @($FailedTests)
    missing = @($MissingTests)
    unexpected = @($UnexpectedTests)
    engineLog = $EngineLog
    consoleLog = $ConsoleLog
}
$Report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ReportPath -Encoding utf8
[pscustomobject]@{
    Status = $Report.status
    ProcessExitCode = $ExitCode
    PassedTests = $CompletedTests.Count
    FailedTests = $FailedTests.Count
    MissingTests = $MissingTests.Count
    Report = $ReportPath
    EngineLog = $EngineLog
} | Format-List
if (Test-Path -LiteralPath $EngineLog -PathType Leaf) {
    Get-Content -LiteralPath $EngineLog -Tail 25
}
if (-not $Passed) {
    throw "P7 main-project automation suite did not pass. See $ReportPath and $EngineLog"
}
