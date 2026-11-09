[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [int]$TimeoutSeconds = 60,
    [switch]$DisableAsyncLoadingThread,
    [switch]$SkipEngineMap,
    [switch]$TracePackageLoading
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ScratchParent = (Resolve-Path (Join-Path $RepositoryRoot 'tmp')).Path
$Stamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$ScratchRoot = Join-Path $ScratchParent "P7GameInitControl_$Stamp"
$ProjectName = 'P7GameInitControl'
$Project = Join-Path $ScratchRoot "$ProjectName.uproject"
$BuildScript = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$Executable = Join-Path $ScratchRoot "Binaries\Win64\$ProjectName.exe"
$UbaRoot = Join-Path $ScratchRoot 'UbaRoot'
$EngineMap = Join-Path $EngineRoot 'Engine\Content\Maps\Entry.umap'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$BuildLog = Join-Path $EvidenceRoot "P7_GameInitControlBuild_$Stamp.log"
$EngineLog = Join-Path $EvidenceRoot "P7_GameInitControlStartup_$Stamp.log"
$StdoutLog = Join-Path $EvidenceRoot "P7_GameInitControlStartup_$Stamp.stdout.log"
$StderrLog = Join-Path $EvidenceRoot "P7_GameInitControlStartup_$Stamp.stderr.log"
$ReportPath = Join-Path $EvidenceRoot "P7_GameInitControl_$Stamp.json"
$CrashContextEvidence = Join-Path $EvidenceRoot "P7_GameInitControl_CrashContext_$Stamp.xml"
$CrashDumpEvidence = Join-Path $EvidenceRoot "P7_GameInitControl_CrashDump_$Stamp.dmp"
$CrashLogEvidence = Join-Path $EvidenceRoot "P7_GameInitControl_CrashLog_$Stamp.log"
$TempEngineLog = Join-Path $env:TEMP "P7GameInitControl_$($Stamp -replace '[^0-9]', '').log"

foreach ($path in @($BuildScript, $EngineMap)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required P7 static-init control input was not found: $path"
    }
}
if (Test-Path -LiteralPath $ScratchRoot) {
    throw "Control project already exists; refusing to overwrite it: $ScratchRoot"
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
New-Item -ItemType Directory -Path (Join-Path $ScratchRoot "Source\$ProjectName\Private") -Force | Out-Null

try {
    @'
{
  "FileVersion": 3,
  "EngineAssociation": "5.8",
  "Modules": [
    { "Name": "P7GameInitControl", "Type": "Runtime", "LoadingPhase": "Default" }
  ]
}
'@ | Set-Content -LiteralPath $Project -Encoding utf8

    @'
using UnrealBuildTool;
public class P7GameInitControl : ModuleRules
{
    public P7GameInitControl(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
    }
}
'@ | Set-Content -LiteralPath (Join-Path $ScratchRoot "Source\$ProjectName\$ProjectName.Build.cs") -Encoding utf8

    @'
using UnrealBuildTool;
public class P7GameInitControlTarget : TargetRules
{
    public P7GameInitControlTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("P7GameInitControl");
    }
}
'@ | Set-Content -LiteralPath (Join-Path $ScratchRoot "Source\$ProjectName.Target.cs") -Encoding utf8

    @'
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, P7GameInitControl, "P7GameInitControl");
'@ | Set-Content -LiteralPath (Join-Path $ScratchRoot "Source\$ProjectName\Private\$ProjectName.cpp") -Encoding utf8

    "P7 no-runtime-module Game target build control - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" |
        Set-Content -LiteralPath $BuildLog -Encoding utf8
    & $BuildScript $ProjectName Win64 Development $Project `
        -NoUBA -NoHotReloadFromIDE -NoLiveCoding -WaitMutex -MaxParallelActions=4 `
        "-UBARootDir=$UbaRoot" 2>&1 |
        Tee-Object -FilePath $BuildLog -Append
    $BuildExitCode = $LASTEXITCODE
    if ($BuildExitCode -ne 0 -or (Get-Content -LiteralPath $BuildLog -Raw) -notmatch 'Result: Succeeded') {
        throw "P7 no-runtime-module control build failed with exit code $BuildExitCode. See $BuildLog"
    }

    $AsyncLoadingArgument = if ($DisableAsyncLoadingThread) { ' -NoAsyncLoadingThread' } else { '' }
    $MapArgument = if ($SkipEngineMap) { '' } else { ' -Map=/Engine/Maps/Entry' }
    $VerboseLoadingArgument = if ($TracePackageLoading) { ' -LogCmds="LogStreaming VeryVerbose,LogAsyncLoading VeryVerbose,LogIoDispatcher VeryVerbose"' } else { '' }
    $ArgumentLine = '-project="' + $Project + '" -unattended -nop4 -nosplash -NullRHI' + $MapArgument + ' -ExecCmds=Quit -stdout -FullStdOutLogOutput -AbsLog="' + $TempEngineLog + '"' + $AsyncLoadingArgument + $VerboseLoadingArgument
    $StartedAt = Get-Date
    $Process = Start-Process -FilePath $Executable -ArgumentList $ArgumentLine `
        -WorkingDirectory $ScratchRoot -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $StdoutLog -RedirectStandardError $StderrLog
    $Finished = $Process.WaitForExit($TimeoutSeconds * 1000)
    if (-not $Finished -and -not $Process.HasExited) {
        Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue
        $Process.WaitForExit()
    }
    $Process.Refresh()
    $ExitCode = if ($Finished) { $Process.ExitCode } else { $null }

    $StartupLogCreated = Test-Path -LiteralPath $TempEngineLog
    if ($StartupLogCreated) {
        Copy-Item -LiteralPath $TempEngineLog -Destination $EngineLog -Force
    }
    $EngineOutput = if ($StartupLogCreated) { Get-Content -LiteralPath $TempEngineLog -Raw } else { '' }
    $LoadedEngineMap = $EngineOutput -match '(?m)LogWorld: Bringing World .*Entry'
    $CleanExit = $EngineOutput -match '(?m)LogExit: Exiting\.'
    $CrashContextSource = if (Test-Path -LiteralPath (Join-Path $ScratchRoot 'Saved\Crashes')) {
        Get-ChildItem -LiteralPath (Join-Path $ScratchRoot 'Saved\Crashes') -Recurse -Filter 'CrashContext.runtime-xml' -File -ErrorAction SilentlyContinue |
            Where-Object { $_.LastWriteTime -ge $StartedAt.AddSeconds(-2) } |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
    } else {
        $null
    }
    if ($CrashContextSource) {
        Copy-Item -LiteralPath $CrashContextSource.FullName -Destination $CrashContextEvidence -Force
        $CrashDumpSource = Join-Path $CrashContextSource.DirectoryName 'UEMinidump.dmp'
        if (Test-Path -LiteralPath $CrashDumpSource -PathType Leaf) {
            Copy-Item -LiteralPath $CrashDumpSource -Destination $CrashDumpEvidence -Force
        }
        $CrashLogSource = Get-ChildItem -LiteralPath $CrashContextSource.DirectoryName -Filter '*.log' -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($CrashLogSource) {
            Copy-Item -LiteralPath $CrashLogSource.FullName -Destination $CrashLogEvidence -Force
        }
    }
    $Passed = $Finished -and $ExitCode -eq 0 -and ($SkipEngineMap -or $LoadedEngineMap) -and $CleanExit
    $Status = if ($Passed) {
        'PASS'
    } elseif (-not $Finished) {
        'TIMEOUT'
    } elseif ($ExitCode -eq 777006) {
        'CRASH_DURING_STATIC_INIT'
    } elseif ($ExitCode -eq 0 -and -not $StartupLogCreated) {
        'INCONCLUSIVE_NO_STARTUP_LOG'
    } else {
        'FAIL'
    }

    $Report = [ordered]@{
        test = 'Minimal Game target with no P7 or main-project runtime modules'
        status = $Status
        startedAt = $StartedAt.ToString('yyyy-MM-dd HH:mm:ssK')
        processId = $Process.Id
        timeoutSeconds = $TimeoutSeconds
        executable = $Executable
        project = $Project
        map = if ($SkipEngineMap) { $null } else { '/Engine/Maps/Entry' }
        asyncLoadingThreadDisabled = [bool]$DisableAsyncLoadingThread
        engineMapSkipped = [bool]$SkipEngineMap
        packageLoadingTraceEnabled = [bool]$TracePackageLoading
        buildExitCode = $BuildExitCode
        buildLog = $BuildLog
        processFinishedWithinTimeout = $Finished
        processExitCode = $ExitCode
        processExitCodeHex = if ($null -ne $ExitCode) { '0x{0:X8}' -f $ExitCode } else { $null }
        startupLogCreated = $StartupLogCreated
        engineMapReportedLoaded = $LoadedEngineMap
        cleanEngineExitReported = $CleanExit
        engineLog = $EngineLog
        stdoutLog = $StdoutLog
        stderrLog = $StderrLog
        crashContext = if ($CrashContextSource) { $CrashContextEvidence } else { $null }
        crashDump = if ($CrashContextSource -and (Test-Path -LiteralPath $CrashDumpEvidence)) { $CrashDumpEvidence } else { $null }
        crashLog = if ($CrashContextSource -and (Test-Path -LiteralPath $CrashLogEvidence)) { $CrashLogEvidence } else { $null }
    }
    $Report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ReportPath -Encoding utf8
    [pscustomobject]@{
        Status = $Status
        BuildExitCode = $BuildExitCode
        ProcessExitCode = $ExitCode
        EngineMapReportedLoaded = $LoadedEngineMap
        CleanEngineExitReported = $CleanExit
        Report = $ReportPath
        BuildLog = $BuildLog
        EngineLog = $EngineLog
        CrashContext = if ($CrashContextSource) { $CrashContextEvidence } else { $null }
    } | Format-List
    if ($StartupLogCreated) {
        Get-Content -LiteralPath $EngineLog -Tail 35
    }
}
finally {
    if (Test-Path -LiteralPath $ScratchRoot) {
        $ResolvedScratch = (Resolve-Path -LiteralPath $ScratchRoot).Path
        $ExpectedScratch = Join-Path $ScratchParent "P7GameInitControl_$Stamp"
        if (-not $ResolvedScratch.Equals($ExpectedScratch, [System.StringComparison]::OrdinalIgnoreCase) -or
            -not $ResolvedScratch.StartsWith($ScratchParent + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove unexpected control project: $ResolvedScratch"
        }
        $CleanupError = $null
        for ($Attempt = 0; $Attempt -lt 10 -and (Test-Path -LiteralPath $ResolvedScratch); $Attempt++) {
            try {
                Remove-Item -LiteralPath $ResolvedScratch -Recurse -Force -ErrorAction Stop
            } catch {
                $CleanupError = $_
                Start-Sleep -Milliseconds 500
            }
        }
        if (Test-Path -LiteralPath $ResolvedScratch) {
            Write-Warning "Control project could not be cleaned automatically; inspect and remove only this scratch directory later: $ResolvedScratch. Last error: $CleanupError"
        }
    }
    if (Test-Path -LiteralPath $TempEngineLog) {
        Remove-Item -LiteralPath $TempEngineLog -Force
    }
}
