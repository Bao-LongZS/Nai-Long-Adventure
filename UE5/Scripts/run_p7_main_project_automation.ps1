[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [int]$TimeoutSeconds = 180,
    [ValidateRange(1, 120)]
    [int]$GracefulExitSeconds = 15
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Project = Join-Path $RepositoryRoot 'UE5\NaiwaP0\Nai_Long.uproject'
$EditorCommandlet = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$AutomationTempCandidate = Join-Path $RepositoryRoot 'tmp\P7ProbeTemp'
$PreviousTemp = $env:TEMP
$PreviousTmp = $env:TMP
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
    'Naiwa.P7.Presentation.RuntimeInputBindings'
    'Naiwa.P7.Presentation.RuntimeInputDispatch'
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
New-Item -ItemType Directory -Force -Path $AutomationTempCandidate | Out-Null
$AutomationTemp = (Resolve-Path -LiteralPath $AutomationTempCandidate).Path
if (-not $AutomationTemp.StartsWith($RepositoryRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) -or
    $AutomationTemp.Contains('!')) {
    throw "Unsafe temporary root for Unreal automation: $AutomationTemp"
}
$env:TEMP = $AutomationTemp
$env:TMP = $AutomationTemp

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
$Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
$FinishedNaturally = $false
$TestsFinishedInLog = $false
do {
    $Process.Refresh()
    $ProgressText = if (Test-Path -LiteralPath $EngineLog -PathType Leaf) {
        Get-Content -LiteralPath $EngineLog -Raw
    } else {
        ''
    }
    $CompletedSoFar = [regex]::Matches(
        $ProgressText,
        'Test Completed\. Result=\{Success\} Name=\{[^}]+\} Path=\{Naiwa\.P7\.[^}]+\}'
    ).Count
    $FailedSoFar = [regex]::Matches(
        $ProgressText,
        'Test Completed\. Result=\{Fail\} Name=\{[^}]+\} Path=\{Naiwa\.P7\.[^}]+\}'
    ).Count
    $TestsFinishedInLog = $CompletedSoFar -ge $ExpectedP7Tests.Count -and $FailedSoFar -eq 0
    if ($Process.HasExited) {
        $FinishedNaturally = $true
        break
    }
    if ($TestsFinishedInLog) {
        $FinishedNaturally = $Process.WaitForExit($GracefulExitSeconds * 1000)
        break
    }
    if ((Get-Date) -ge $Deadline) {
        break
    }
    Start-Sleep -Milliseconds 500
} while ($true)

$Process.Refresh()
$ProcessClosedByHarness = -not $Process.HasExited
if ($ProcessClosedByHarness) {
    Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue
    $Process.WaitForExit()
}
$Process.Refresh()
$ExitCode = if ($FinishedNaturally) { $Process.ExitCode } else { $null }
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
$ExitCodeSource = if ($null -ne $ExitCode) { 'process' } else { 'unavailable' }
if ($null -eq $ExitCode) {
    $EngineExitStatus = [regex]::Match(
        $LogText,
        'RequestExitWithStatus\(\d+,\s*(?<ExitCode>-?\d+),\s*FEngineLoop::Tick\.GScopedTestExit\)'
    )
    if ($EngineExitStatus.Success) {
        $ExitCode = [int]$EngineExitStatus.Groups['ExitCode'].Value
        $ExitCodeSource = 'engine-log'
    }
}
$TestsPassed = $FailedTests.Count -eq 0 -and $MissingTests.Count -eq 0 -and
    $UnexpectedTests.Count -eq 0 -and $CompletedTests.Count -eq $ExpectedP7Tests.Count
$Passed = $TestsPassed -and $FinishedNaturally -and $ExitCode -eq 0
$Report = [ordered]@{
    test = 'P7 automation suite in the main Nai_Long project'
    status = if ($Passed -and $ExitCode -eq 0) { 'PASS' } elseif ($Passed) { 'PASS_TESTS_EXIT_CODE_UNKNOWN' } elseif ($TestsPassed -and $ProcessClosedByHarness) { 'PASS_TESTS_SHUTDOWN_TIMEOUT' } elseif (-not $TestsFinishedInLog) { 'TIMEOUT' } else { 'FAIL' }
    startedAt = $StartedAt.ToString('yyyy-MM-dd HH:mm:ssK')
    timeoutSeconds = $TimeoutSeconds
    project = $Project
    tempRoot = $AutomationTemp
    processExitCode = $ExitCode
    processExitCodeSource = $ExitCodeSource
    processFinishedNaturally = $FinishedNaturally
    testsPassed = $TestsPassed
    processClosedByHarness = $ProcessClosedByHarness
    expectedCount = $ExpectedP7Tests.Count
    completed = @($CompletedTests)
    failed = @($FailedTests)
    missing = @($MissingTests)
    unexpected = @($UnexpectedTests)
    engineLog = $EngineLog
    consoleLog = $ConsoleLog
}
$Report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ReportPath -Encoding utf8
$env:TEMP = $PreviousTemp
$env:TMP = $PreviousTmp
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
if (-not $TestsPassed) {
    throw "P7 main-project automation suite did not pass. See $ReportPath and $EngineLog"
}
