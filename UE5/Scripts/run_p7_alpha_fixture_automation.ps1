[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Project = Join-Path $RepositoryRoot 'UE5\NaiwaP0\Nai_Long.uproject'
$EditorCommandlet = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$EngineLog = Join-Path $EvidenceRoot "P7_AlphaFixtureAutomation_$Timestamp.log"
$ConsoleLog = Join-Path $EvidenceRoot "P7_AlphaFixtureAutomation_$Timestamp.console.log"
$ReportPath = Join-Path $EvidenceRoot "P7_AlphaFixtureAutomation_$Timestamp.json"
$TestPath = 'Naiwa.P7.Data.AlphaFixtureAssetAssembly'

foreach ($path in @($Project, $EditorCommandlet)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required P7 Alpha automation input was not found: $path"
    }
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

$ArgumentLine = @(
    ('"' + $Project + '"'),
    '-unattended', '-nop4', '-nosplash', '-nullrhi', '-NoMutex',
    '-stdout', '-FullStdOutLogOutput',
    ('"-ExecCmds=Automation RunTests ' + $TestPath + '"'),
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
$SuccessPattern = 'Test Completed\. Result=\{Success\} Name=\{AlphaFixtureAssetAssembly\} Path=\{Naiwa\.P7\.Data\.AlphaFixtureAssetAssembly\}'
$FailurePattern = 'Test Completed\. Result=\{Fail\} Name=\{AlphaFixtureAssetAssembly\} Path=\{Naiwa\.P7\.Data\.AlphaFixtureAssetAssembly\}'
$Passed = $Finished -and [regex]::IsMatch($LogText, $SuccessPattern) -and
    -not [regex]::IsMatch($LogText, $FailurePattern)
$Report = [ordered]@{
    test = $TestPath
    status = if ($Passed) { 'PASS' } elseif (-not $Finished) { 'TIMEOUT' } else { 'FAIL' }
    startedAt = $StartedAt.ToString('yyyy-MM-dd HH:mm:ssK')
    elapsedTimeoutSeconds = $TimeoutSeconds
    processExitCode = $ExitCode
    project = $Project
    engineLog = $EngineLog
    consoleLog = $ConsoleLog
    successRecordFound = [regex]::IsMatch($LogText, $SuccessPattern)
    failureRecordFound = [regex]::IsMatch($LogText, $FailurePattern)
}
$Report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ReportPath -Encoding utf8
[pscustomobject]@{
    Status = $Report.status
    ProcessExitCode = $ExitCode
    SuccessRecordFound = $Report.successRecordFound
    FailureRecordFound = $Report.failureRecordFound
    Report = $ReportPath
    EngineLog = $EngineLog
} | Format-List
if (Test-Path -LiteralPath $EngineLog -PathType Leaf) {
    Get-Content -LiteralPath $EngineLog -Tail 25
}
if (-not $Passed) {
    throw "P7 production fixture automation did not pass. See $ReportPath and $EngineLog"
}
