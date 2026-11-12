[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$StageDirectory,
    [int]$TimeoutSeconds = 60,
    [string]$MapName = '/Game/Naiwa/UI/L_NaiwaMainMenu',
    [switch]$RequireP7Fixture
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$StageRoot = (Resolve-Path -LiteralPath $StageDirectory).Path
$WindowsRoot = if ((Split-Path -Leaf $StageRoot) -eq 'Windows') {
    $StageRoot
} else {
    Join-Path $StageRoot 'Windows'
}
$Executable = Join-Path $WindowsRoot 'Nai_Long.exe'
$ContentPak = Join-Path $WindowsRoot 'Nai_Long\Content\Paks\Nai_Long-Windows.pak'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$EngineLog = Join-Path $EvidenceRoot "P7_MainGameCookedStartup_$Timestamp.log"
$StdoutLog = Join-Path $EvidenceRoot "P7_MainGameCookedStartup_$Timestamp.stdout.log"
$StderrLog = Join-Path $EvidenceRoot "P7_MainGameCookedStartup_$Timestamp.stderr.log"
$ReportPath = Join-Path $EvidenceRoot "P7_MainGameCookedStartup_$Timestamp.json"

foreach ($path in @($Executable, $ContentPak)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required cooked P7 startup input was not found: $path"
    }
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

$ArgumentLine = "-unattended -nop4 -nosplash -NullRHI -Map=$MapName -ExecCmds=Quit -stdout -FullStdOutLogOutput -AbsLog=`"$EngineLog`""
$StartedAt = Get-Date
$Process = Start-Process -FilePath $Executable -ArgumentList $ArgumentLine `
    -WorkingDirectory $WindowsRoot -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput $StdoutLog -RedirectStandardError $StderrLog
$Finished = $Process.WaitForExit($TimeoutSeconds * 1000)
if (-not $Finished -and -not $Process.HasExited) {
    Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue
    $Process.WaitForExit()
    $StartedProcesses = Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq 'Nai_Long.exe' -and $_.CommandLine -like "*$WindowsRoot*" }
    foreach ($StartedProcess in $StartedProcesses) {
        Stop-Process -Id $StartedProcess.ProcessId -Force -ErrorAction SilentlyContinue
    }
}
$Process.Refresh()
$ExitCode = if ($Finished) { $Process.ExitCode } else { $null }

$EngineOutput = if (Test-Path -LiteralPath $EngineLog) { Get-Content -LiteralPath $EngineLog -Raw } else { '' }
$LoadedWorld = $EngineOutput -match ('(?m)LogWorld: Bringing World .*' + [regex]::Escape($MapName))
$FixtureLoaded = $EngineOutput -match 'P7 graybox loaded level P7_Alpha01 from five configured DataTables\.'
$FixtureLoadFailed = $EngineOutput -match 'P7 graybox could not load its configured level|P7 graybox could not find its simulation subsystem'
$CleanExit = $EngineOutput -match '(?m)LogExit: Exiting\.'
$ExitStatusAccepted = $ExitCode -eq 0 -or ($null -eq $ExitCode -and $Finished -and $CleanExit)
$Passed = $Finished -and $ExitStatusAccepted -and $LoadedWorld -and $CleanExit -and
    (-not $RequireP7Fixture -or ($FixtureLoaded -and -not $FixtureLoadFailed))
$Status = if ($Passed) { 'PASS' } elseif (-not $Finished) { 'TIMEOUT' } else { 'FAIL' }
$Report = [ordered]@{
    test = "Cooked Nai_Long Development startup: $MapName"
    status = $Status
    startedAt = $StartedAt.ToString('yyyy-MM-dd HH:mm:ssK')
    elapsedTimeoutSeconds = $TimeoutSeconds
    processId = $Process.Id
    stageDirectory = $WindowsRoot
    executable = $Executable
    contentPak = $ContentPak
    map = $MapName
    finishedWithinTimeout = $Finished
    processExitCode = $ExitCode
    targetMapReportedLoaded = $LoadedWorld
    p7FixtureLoaded = $FixtureLoaded
    p7FixtureLoadFailed = $FixtureLoadFailed
    cleanEngineExitReported = $CleanExit
    engineLogCreated = Test-Path -LiteralPath $EngineLog
    engineLog = $EngineLog
    stdoutLog = $StdoutLog
    stderrLog = $StderrLog
}
$Report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ReportPath -Encoding utf8

[pscustomobject]@{
    Status = $Status
    ProcessExitCode = $ExitCode
    TargetMapReportedLoaded = $LoadedWorld
    P7FixtureLoaded = $FixtureLoaded
    CleanEngineExitReported = $CleanExit
    Report = $ReportPath
    EngineLog = $EngineLog
} | Format-List
if ($EngineOutput) {
    $EngineOutput -split "`r?`n" | Select-String -Pattern ('Bringing World .*' + [regex]::Escape($MapName) + '|P7 graybox loaded|P7 graybox could not|Engine exit requested|LogExit: Exiting\.') | Select-Object -Last 8
}

if (-not $Passed) {
    throw "Cooked P7 main-menu startup did not pass. See $ReportPath and $EngineLog"
}
