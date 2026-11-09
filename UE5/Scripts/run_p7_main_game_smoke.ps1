[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [int]$TimeoutSeconds = 60,
    [string]$MapName = '/Game/Naiwa/UI/L_NaiwaMainMenu',
    [switch]$RequireP7Fixture
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectRoot = Join-Path $RepositoryRoot 'UE5\NaiwaP0'
$Project = Join-Path $ProjectRoot 'Nai_Long.uproject'
$Executable = Join-Path $ProjectRoot 'Binaries\Win64\Nai_Long.exe'
$MapAssetRelativePath = if ($MapName.StartsWith('/Game/', [System.StringComparison]::OrdinalIgnoreCase)) {
    ($MapName.Substring('/Game/'.Length) -replace '/', '\') + '.umap'
} else {
    throw "P7 smoke expects a project content map path beginning with /Game/: $MapName"
}
$MapAsset = Join-Path $ProjectRoot (Join-Path 'Content' $MapAssetRelativePath)
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$CrashContextHeader = Join-Path $EngineRoot 'Engine\Source\Runtime\Core\Public\GenericPlatform\GenericPlatformCrashContext.h'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$EngineLog = Join-Path $EvidenceRoot "P7_MainGameStartup_engine_$Timestamp.log"
$StdoutLog = Join-Path $EvidenceRoot "P7_MainGameStartup_stdout_$Timestamp.log"
$StderrLog = Join-Path $EvidenceRoot "P7_MainGameStartup_stderr_$Timestamp.log"
$ReportPath = Join-Path $EvidenceRoot "P7_MainGameStartup_$Timestamp.json"
$CrashRoot = Join-Path $ProjectRoot 'Saved\Crashes'

foreach ($path in @($Project, $Executable, $MapAsset)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required P7 game startup input was not found: $path"
    }
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

$ArgumentLine = "-project=`"$Project`" -unattended -nop4 -nosplash -NullRHI -Map=$MapName -ExecCmds=Quit -stdout -FullStdOutLogOutput -AbsLog=`"$EngineLog`""
$StartedAt = Get-Date
$Process = Start-Process -FilePath $Executable -ArgumentList $ArgumentLine `
    -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput $StdoutLog -RedirectStandardError $StderrLog
$Finished = $Process.WaitForExit($TimeoutSeconds * 1000)
if (-not $Finished -and -not $Process.HasExited) {
    Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue
    $Process.WaitForExit()
}
$Process.Refresh()
$ExitCode = if ($Finished) { $Process.ExitCode } else { $null }

$CrashContextSource = $null
$CrashContextCopyPath = $null
$CrashDumpCopyPath = $null
$CrashLogCopyPath = $null
$CrashGameName = $null
$CrashErrorMessage = $null
$CrashSecondsSinceStart = $null
for ($Attempt = 0; $Attempt -lt 10 -and -not $CrashContextSource; $Attempt++) {
    if (Test-Path -LiteralPath $CrashRoot) {
        $CrashCandidates = Get-ChildItem -LiteralPath $CrashRoot -Recurse -Filter 'CrashContext.runtime-xml' -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending
        foreach ($Candidate in $CrashCandidates) {
            if ($Candidate.LastWriteTime -lt $StartedAt.AddSeconds(-2)) {
                continue
            }
            try {
                [xml]$CandidateXml = Get-Content -LiteralPath $Candidate.FullName -Raw
                $CandidateRuntime = $CandidateXml.FGenericCrashContext.RuntimeProperties
                if ($CandidateRuntime.GameName -match 'Nai_Long') {
                    $CrashContextSource = $Candidate
                    $CrashGameName = $CandidateRuntime.GameName
                    $CrashErrorMessage = $CandidateRuntime.ErrorMessage
                    $CrashSecondsSinceStart = $CandidateRuntime.SecondsSinceStart
                    break
                }
            } catch {
                continue
            }
        }
    }
    if (-not $CrashContextSource -and $Attempt -lt 9) {
        Start-Sleep -Milliseconds 500
    }
}
if ($CrashContextSource) {
    $CrashContextCopyPath = Join-Path $EvidenceRoot "P7_MainGameStartup_CrashContext_$Timestamp.xml"
    Copy-Item -LiteralPath $CrashContextSource.FullName -Destination $CrashContextCopyPath

    $CrashDumpSource = Join-Path $CrashContextSource.DirectoryName 'UEMinidump.dmp'
    if (Test-Path -LiteralPath $CrashDumpSource -PathType Leaf) {
        $CrashDumpCopyPath = Join-Path $EvidenceRoot "P7_MainGameStartup_CrashDump_$Timestamp.dmp"
        Copy-Item -LiteralPath $CrashDumpSource -Destination $CrashDumpCopyPath
    }

    $CrashLogSource = Get-ChildItem -LiteralPath $CrashContextSource.DirectoryName -Filter 'Nai_Long*.log' -File -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -ge $StartedAt.AddSeconds(-2) } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($CrashLogSource) {
        $CrashLogCopyPath = Join-Path $EvidenceRoot "P7_MainGameStartup_CrashLog_$Timestamp.log"
        Copy-Item -LiteralPath $CrashLogSource.FullName -Destination $CrashLogCopyPath
    }
}

$ExitCodeMeaning = $null
if ($null -ne $ExitCode -and (Test-Path -LiteralPath $CrashContextHeader -PathType Leaf)) {
    $CrashCodeDeclaration = Select-String -LiteralPath $CrashContextHeader `
        -Pattern 'CrashDuringStaticInit\s*=\s*777006' -Quiet
    if ($ExitCode -eq 777006 -and $CrashCodeDeclaration) {
        $ExitCodeMeaning = 'CrashDuringStaticInit (UE ECrashExitCodes)'
    }
}

$EngineOutput = if (Test-Path -LiteralPath $EngineLog) { Get-Content -LiteralPath $EngineLog -Raw } else { '' }
$LoadedWorld = $EngineOutput -match ('(?m)LogWorld: Bringing World .*' + [regex]::Escape($MapName))
$FixtureLoaded = $EngineOutput -match 'P7 graybox loaded level P7_Alpha01 from five configured DataTables\.'
$FixtureLoadFailed = $EngineOutput -match 'P7 graybox could not load its configured level|P7 graybox could not find its simulation subsystem'
$CleanExit = $EngineOutput -match '(?m)LogExit: Exiting\.'
$Passed = $Finished -and $ExitCode -eq 0 -and $LoadedWorld -and $CleanExit -and
    (-not $RequireP7Fixture -or ($FixtureLoaded -and -not $FixtureLoadFailed))
$Status = if ($Passed) {
    'PASS'
} elseif (-not $Finished) {
    'TIMEOUT'
} elseif ($ExitCode -eq 777006 -and $null -ne $ExitCodeMeaning) {
    'CRASH_DURING_STATIC_INIT'
} elseif ($null -ne $CrashContextSource) {
    'CRASH_WITH_CONTEXT'
} else {
    'FAIL'
}

$Report = [ordered]@{
    test = "Nai_Long Development game target headless startup: $MapName"
    status = $Status
    startedAt = $StartedAt.ToString('yyyy-MM-dd HH:mm:ssK')
    elapsedTimeoutSeconds = $TimeoutSeconds
    processId = $Process.Id
    executable = $Executable
    project = $Project
    map = $MapName
    finishedWithinTimeout = $Finished
    processExitCode = $ExitCode
    processExitCodeHex = if ($null -ne $ExitCode) { '0x{0:X8}' -f $ExitCode } else { $null }
    processExitCodeMeaning = $ExitCodeMeaning
    crashContextCaptured = $null -ne $CrashContextSource
    crashContextGameName = $CrashGameName
    crashErrorMessage = $CrashErrorMessage
    crashSecondsSinceStart = $CrashSecondsSinceStart
    crashContextSource = if ($CrashContextSource) { $CrashContextSource.FullName } else { $null }
    crashContextEvidence = $CrashContextCopyPath
    crashDumpEvidence = $CrashDumpCopyPath
    crashLogEvidence = $CrashLogCopyPath
    startupLogCreated = Test-Path -LiteralPath $EngineLog
    targetMapReportedLoaded = $LoadedWorld
    p7FixtureLoaded = $FixtureLoaded
    p7FixtureLoadFailed = $FixtureLoadFailed
    cleanEngineExitReported = $CleanExit
    engineLog = $EngineLog
    stdoutLog = $StdoutLog
    stderrLog = $StderrLog
    note = if ($null -ne $CrashContextSource) {
        'Unreal wrote a CrashContext for this process. Treat the run as a crash even if the process exit code is 0.'
    } elseif ($Status -eq 'CRASH_DURING_STATIC_INIT') {
        'UE reported CrashDuringStaticInit before producing a project CrashContext or normal startup log.'
    } else {
        $null
    }
}
$Report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ReportPath -Encoding utf8

[pscustomobject]@{
    Status = $Status
    ProcessExitCode = $ExitCode
    ProcessExitCodeMeaning = $ExitCodeMeaning
    CrashContextCaptured = $null -ne $CrashContextSource
    CrashErrorMessage = $CrashErrorMessage
    TargetMapReportedLoaded = $LoadedWorld
    P7FixtureLoaded = $FixtureLoaded
    CleanEngineExitReported = $CleanExit
    Report = $ReportPath
    EngineLog = $EngineLog
    CrashContext = $CrashContextCopyPath
} | Format-List

if (Test-Path -LiteralPath $EngineLog) {
    Get-Content -LiteralPath $EngineLog -Tail 35
}
