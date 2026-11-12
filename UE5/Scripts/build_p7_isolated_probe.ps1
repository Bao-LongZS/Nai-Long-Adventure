[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateSet('Editor', 'Game')]
    [string]$Target = 'Editor',
    [switch]$RunAutomationTests,
    [switch]$KeepProbe
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProbeParentCandidate = Join-Path $RepositoryRoot 'tmp'
New-Item -ItemType Directory -Force -Path $ProbeParentCandidate | Out-Null
$ProbeParent = (Resolve-Path $ProbeParentCandidate).Path
$ProbeRoot = Join-Path $ProbeParent 'P7BuildProbe'
$ProbeTempCandidate = Join-Path $ProbeParent 'P7ProbeTemp'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$BuildBatch = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$EditorCommandlet = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$ProbeProject = Join-Path $ProbeRoot 'P7BuildProbe.uproject'
$BuildTimestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$TargetName = if ($Target -eq 'Editor') { 'P7BuildProbeEditor' } else { 'P7BuildProbe' }
$BuildLog = Join-Path $EvidenceRoot "P7_Isolated${Target}Build_$BuildTimestamp.log"
$PreviousUbaRoot = $env:UBA_ROOT
$PreviousTemp = $env:TEMP
$PreviousTmp = $env:TMP

if (-not (Test-Path -LiteralPath $BuildBatch)) {
    throw "Unreal Build.bat was not found: $BuildBatch"
}
if ($RunAutomationTests -and $Target -ne 'Editor') {
    throw 'P7 automation tests require -Target Editor.'
}
if (Test-Path -LiteralPath $ProbeRoot) {
    throw "Probe path already exists; refusing to overwrite it: $ProbeRoot"
}

try {
    New-Item -ItemType Directory -Force -Path $ProbeTempCandidate | Out-Null
    $ProbeTemp = (Resolve-Path -LiteralPath $ProbeTempCandidate).Path
    if (-not $ProbeTemp.StartsWith($ProbeParent + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) -or
        $ProbeTemp.Contains('!')) {
        throw "Unsafe temporary root for Unreal Build.bat: $ProbeTemp"
    }
    $env:TEMP = $ProbeTemp
    $env:TMP = $ProbeTemp

    New-Item -ItemType Directory -Force -Path (Join-Path $ProbeRoot 'Source\P7BuildProbe\Private') | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $ProbeRoot 'Source\NaiwaP7Runtime') | Out-Null
    New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

    $SourceModule = Join-Path $RepositoryRoot 'UE5\NaiwaP0\Source\NaiwaP7Runtime'
    Copy-Item -Path (Join-Path $SourceModule '*') -Destination (Join-Path $ProbeRoot 'Source\NaiwaP7Runtime') -Recurse -Force

    @'
{
  "FileVersion": 3,
  "EngineAssociation": "5.8",
  "Plugins": [
    { "Name": "EnhancedInput", "Enabled": true }
  ],
  "Modules": [
    { "Name": "P7BuildProbe", "Type": "Runtime", "LoadingPhase": "Default" },
    { "Name": "NaiwaP7Runtime", "Type": "Runtime", "LoadingPhase": "Default" }
  ]
}
'@ | Set-Content -Encoding utf8 $ProbeProject

    @'
using UnrealBuildTool;
public class P7BuildProbeTarget : TargetRules
{
    public P7BuildProbeTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("P7BuildProbe");
        ExtraModuleNames.Add("NaiwaP7Runtime");
    }
}
'@ | Set-Content -Encoding utf8 (Join-Path $ProbeRoot 'Source\P7BuildProbe.Target.cs')

    @'
using UnrealBuildTool;
public class P7BuildProbeEditorTarget : TargetRules
{
    public P7BuildProbeEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("P7BuildProbe");
        ExtraModuleNames.Add("NaiwaP7Runtime");
    }
}
'@ | Set-Content -Encoding utf8 (Join-Path $ProbeRoot 'Source\P7BuildProbeEditor.Target.cs')

    @'
using UnrealBuildTool;
public class P7BuildProbe : ModuleRules
{
    public P7BuildProbe(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
    }
}
'@ | Set-Content -Encoding utf8 (Join-Path $ProbeRoot 'Source\P7BuildProbe\P7BuildProbe.Build.cs')

    @'
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, P7BuildProbe, "P7BuildProbe");
'@ | Set-Content -Encoding utf8 (Join-Path $ProbeRoot 'Source\P7BuildProbe\Private\P7BuildProbe.cpp')

    $env:UBA_ROOT = Join-Path $ProbeRoot 'Uba'
    "P7 isolated $Target development build - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')`r`nTMP/TEMP: $ProbeTemp" |
        Set-Content -LiteralPath $BuildLog -Encoding utf8
    $BuildOutput = & $BuildBatch $TargetName Win64 Development $ProbeProject `
        -NoUBA -NoHotReloadFromIDE -NoLiveCoding -WaitMutex -MaxParallelActions=8 2>&1
    $BuildExitCode = $LASTEXITCODE
    $BuildOutput | Out-File -LiteralPath $BuildLog -Append -Encoding utf8
    $BuildOutput | Select-Object -Last 18
    if ($BuildExitCode -ne 0) {
        throw "Isolated P7 $Target build failed with exit code $BuildExitCode. See $BuildLog"
    }
    Write-Output "Isolated P7 $Target build succeeded. Log: $BuildLog"

    if ($RunAutomationTests) {
        if (-not (Test-Path -LiteralPath $EditorCommandlet)) {
            throw "UnrealEditor-Cmd was not found: $EditorCommandlet"
        }

        $AutomationLog = Join-Path $EvidenceRoot "P7_Automation_$BuildTimestamp.log"
        $AutomationConsoleLog = Join-Path $EvidenceRoot "P7_AutomationConsole_$BuildTimestamp.log"
        $AutomationStatusLog = Join-Path $EvidenceRoot "P7_AutomationStatus_$BuildTimestamp.txt"
        $AutomationOutput = & $EditorCommandlet $ProbeProject `
            -unattended -nop4 -nullrhi -nosplash -stdout -FullStdOutLogOutput `
            -log `
            '-ExecCmds=Automation RunTests Naiwa.P7' `
            '-TestExit=Automation Test Queue Empty' `
            ("-AbsLog=$AutomationLog") 2>&1
        $AutomationExitCode = $LASTEXITCODE
        $AutomationOutput | Out-File -LiteralPath $AutomationConsoleLog -Encoding utf8
        if ($AutomationExitCode -ne 0) {
            throw "P7 automation runner exited with code $AutomationExitCode. See $AutomationConsoleLog"
        }

        if (-not (Test-Path -LiteralPath $AutomationLog) -or (Get-Item -LiteralPath $AutomationLog).Length -eq 0) {
            @(
                "Target: $TargetName"
                "Command: UnrealEditor-Cmd.exe $ProbeProject -ExecCmds=Automation RunTests Naiwa.P7 -TestExit=Automation Test Queue Empty"
                "Process exit code: $AutomationExitCode"
                "Engine log exists: $(Test-Path -LiteralPath $AutomationLog)"
                "Console capture bytes: $((Get-Item -LiteralPath $AutomationConsoleLog).Length)"
                'Outcome: no verifiable engine log; automation tests were not counted as run.'
            ) | Set-Content -LiteralPath $AutomationStatusLog -Encoding utf8
            throw "P7 automation produced no engine log. See $AutomationStatusLog"
        }
        $AutomationText = Get-Content -LiteralPath $AutomationLog -Raw
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
        $CompletedP7Tests = [regex]::Matches(
            $AutomationText,
            'Test Completed\. Result=\{Success\} Name=\{[^}]+\} Path=\{(?<Path>Naiwa\.P7\.[^}]+)\}'
        ) | ForEach-Object { $_.Groups['Path'].Value }
        $FailedP7Tests = [regex]::Matches(
            $AutomationText,
            'Test Completed\. Result=\{Fail\} Name=\{[^}]+\} Path=\{(?<Path>Naiwa\.P7\.[^}]+)\}'
        ) | ForEach-Object { $_.Groups['Path'].Value }
        $MissingP7Tests = @($ExpectedP7Tests | Where-Object { $_ -notin $CompletedP7Tests })
        $UnexpectedP7Tests = @($CompletedP7Tests | Where-Object { $_ -notin $ExpectedP7Tests })
        if ($FailedP7Tests.Count -gt 0 -or $MissingP7Tests.Count -gt 0 -or $UnexpectedP7Tests.Count -gt 0) {
            $FailureSummary = @(
                "Failed: $($FailedP7Tests -join ', ')"
                "Missing: $($MissingP7Tests -join ', ')"
                "Unexpected: $($UnexpectedP7Tests -join ', ')"
            ) -join '; '
            throw "P7 automation suite did not match the expected passing set ($FailureSummary). See $AutomationLog and $AutomationConsoleLog"
        }

        Write-Output "P7 automation tests succeeded ($($CompletedP7Tests.Count)/$($ExpectedP7Tests.Count)). Log: $AutomationLog"
        Get-Content -LiteralPath $AutomationLog -Tail 28
    }
}
finally {
    $env:UBA_ROOT = $PreviousUbaRoot
    $env:TEMP = $PreviousTemp
    $env:TMP = $PreviousTmp
    if (Test-Path -LiteralPath $ProbeRoot) {
        if ($KeepProbe) {
            Write-Output "Keeping isolated P7 project for inspection: $ProbeRoot"
        } else {
            $ResolvedProbe = (Resolve-Path -LiteralPath $ProbeRoot).Path
            $ExpectedProbe = Join-Path $ProbeParent 'P7BuildProbe'
            if (-not $ResolvedProbe.Equals($ExpectedProbe, [System.StringComparison]::OrdinalIgnoreCase) -or
                -not $ResolvedProbe.StartsWith($ProbeParent + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Refusing to remove unexpected probe path: $ResolvedProbe"
            }
            Remove-Item -LiteralPath $ResolvedProbe -Recurse -Force
        }
    }
}
