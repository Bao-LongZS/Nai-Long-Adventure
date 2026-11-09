[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectRoot = Join-Path $RepositoryRoot 'UE5\NaiwaP0'
$Project = Join-Path $ProjectRoot 'Nai_Long.uproject'
$TestAssetFolder = Join-Path $ProjectRoot 'Content\Naiwa\P7\Tests'
$EditorCommandlet = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$PythonScript = Join-Path $PSScriptRoot 'p7_datatable_csv_import_probe.py'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$CrashRoot = Join-Path $ProjectRoot 'Saved\Crashes'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$EngineLog = Join-Path $EvidenceRoot "P7_DataTableCsvEditorImport_$Timestamp.log"
$StdoutLog = Join-Path $EvidenceRoot "P7_DataTableCsvEditorImport_$Timestamp.stdout.log"
$StderrLog = Join-Path $EvidenceRoot "P7_DataTableCsvEditorImport_$Timestamp.stderr.log"
$ProbeReport = Join-Path $EvidenceRoot "P7_DataTableCsvProbe_$Timestamp.json"
$RunnerReport = Join-Path $EvidenceRoot "P7_DataTableCsvRunner_$Timestamp.json"

foreach ($Path in @($Project, $EditorCommandlet, $PythonScript)) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required P7 CSV import probe input was not found: $Path"
    }
}
if (Test-Path -LiteralPath $TestAssetFolder) {
    $ExistingTestFolderItems = @(Get-ChildItem -LiteralPath $TestAssetFolder -Force -ErrorAction Stop)
    if ($ExistingTestFolderItems.Count -gt 0) {
        throw "Refusing to run while the P7 test asset folder contains user data: $TestAssetFolder"
    }
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

$ArgumentLine = '"' + $Project + '" -unattended -nop4 -nosplash -nullrhi -NoMutex -stdout -FullStdOutLogOutput -log -ExecutePythonScript="' + $PythonScript + '" -AbsLog="' + $EngineLog + '"'
$PreviousReportOverride = $env:NAIWA_P7_DATATABLE_REPORT
$env:NAIWA_P7_DATATABLE_REPORT = $ProbeReport
$StartedAt = Get-Date
$Process = $null
$Finished = $false
$ExitCode = $null
$RecentCrashContext = $null
$ProbeResult = $null

try {
    $Process = Start-Process -FilePath $EditorCommandlet -ArgumentList $ArgumentLine -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $StdoutLog -RedirectStandardError $StderrLog
    $Finished = $Process.WaitForExit($TimeoutSeconds * 1000)
    if (-not $Finished -and -not $Process.HasExited) {
        Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue
        $Process.WaitForExit()
    }
    $Process.Refresh()
    $ExitCode = if ($Finished) { $Process.ExitCode } else { $null }
} finally {
    if ($null -eq $PreviousReportOverride) {
        Remove-Item Env:\NAIWA_P7_DATATABLE_REPORT -ErrorAction SilentlyContinue
    } else {
        $env:NAIWA_P7_DATATABLE_REPORT = $PreviousReportOverride
    }
}

$DiskCleanupPassed = $true
$DiskCleanupRemovedFiles = @()
if (Test-Path -LiteralPath $TestAssetFolder) {
    $ResolvedTestFolder = (Resolve-Path -LiteralPath $TestAssetFolder).Path
    $ExpectedTestFolder = Join-Path $ProjectRoot 'Content\Naiwa\P7\Tests'
    $ContentRoot = (Resolve-Path -LiteralPath (Join-Path $ProjectRoot 'Content')).Path
    if (-not $ResolvedTestFolder.Equals($ExpectedTestFolder, [System.StringComparison]::OrdinalIgnoreCase) -or
        -not $ResolvedTestFolder.StartsWith($ContentRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean an unexpected P7 test folder: $ResolvedTestFolder"
    }

    $AllowedPackageNames = @('DT_P7_Level_CsvImportProbe', 'DT_P7_Wave_CsvImportProbe')
    $AllowedPackageExtensions = @('.uasset', '.uexp', '.ubulk', '.uptnl')
    foreach ($PackageFile in Get-ChildItem -LiteralPath $ResolvedTestFolder -File -Force -ErrorAction SilentlyContinue) {
        if ($PackageFile.BaseName -in $AllowedPackageNames -and $PackageFile.Extension -in $AllowedPackageExtensions) {
            Remove-Item -LiteralPath $PackageFile.FullName -Force
            $DiskCleanupRemovedFiles += $PackageFile.Name
        } else {
            $DiskCleanupPassed = $false
        }
    }
    $RemainingTestFolderItems = @(Get-ChildItem -LiteralPath $ResolvedTestFolder -Force -ErrorAction SilentlyContinue)
    if ($RemainingTestFolderItems.Count -eq 0) {
        Remove-Item -LiteralPath $ResolvedTestFolder -Force
    } else {
        $DiskCleanupPassed = $false
    }
}

if (Test-Path -LiteralPath $ProbeReport -PathType Leaf) {
    $ProbeResult = Get-Content -LiteralPath $ProbeReport -Raw | ConvertFrom-Json
}
if (Test-Path -LiteralPath $CrashRoot) {
    $RecentCrashContext = Get-ChildItem -LiteralPath $CrashRoot -Recurse -Filter 'CrashContext.runtime-xml' -File -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -ge $StartedAt.AddSeconds(-2) } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
}

$EngineLogCreated = (Test-Path -LiteralPath $EngineLog -PathType Leaf) -and (Get-Item -LiteralPath $EngineLog).Length -gt 0
$EngineLogText = if ($EngineLogCreated) { Get-Content -LiteralPath $EngineLog -Raw } else { '' }
$UnclassifiedAutomationErrorCount = [regex]::Matches($EngineLogText, '(?m)^.*LogAutomationTest: Error:').Count
$AssetRegistryDeletionWarningCount = [regex]::Matches(
    $EngineLogText,
    'package was marked as deleted in editor, but has been modified on disk'
).Count
$ProbeReportCreated = (Test-Path -LiteralPath $ProbeReport -PathType Leaf) -and (Get-Item -LiteralPath $ProbeReport).LastWriteTime -ge $StartedAt.AddSeconds(-2)
$CleanupProperties = if ($ProbeResult -and $ProbeResult.temporary_asset_cleanup) { @($ProbeResult.temporary_asset_cleanup.PSObject.Properties) } else { @() }
$CleanupPassed = $CleanupProperties.Count -gt 0 -and @($CleanupProperties | Where-Object { -not $_.Value }).Count -eq 0
$ProbePassed = $ProbeReportCreated -and $ProbeResult.status -eq 'passed' -and $CleanupPassed -and $DiskCleanupPassed
$Status = if (-not $Finished) {
    'TIMEOUT'
} elseif ($ExitCode -eq 777006) {
    'CRASH_DURING_STATIC_INIT'
} elseif ($ExitCode -eq 0 -and $EngineLogCreated -and $ProbePassed) {
    'PASS'
} elseif ($ExitCode -eq 0 -and -not $EngineLogCreated -and -not $ProbeReportCreated) {
    'INCONCLUSIVE_NO_ENGINE_LOG'
} else {
    'FAIL'
}

$Report = [ordered]@{
    test = 'P7 Level/Wave CSVImportFactory disk import and DataTable readback'
    status = $Status
    startedAt = $StartedAt.ToString('yyyy-MM-dd HH:mm:ssK')
    finishedWithinTimeout = $Finished
    processExitCode = $ExitCode
    executable = $EditorCommandlet
    project = $Project
    pythonScript = $PythonScript
    engineLogCreated = $EngineLogCreated
    engineLog = $EngineLog
    engineLogObservations = [ordered]@{
        unclassifiedAutomationErrorLines = $UnclassifiedAutomationErrorCount
        temporaryAssetRegistryDeletionWarnings = $AssetRegistryDeletionWarningCount
    }
    stdoutLog = $StdoutLog
    stderrLog = $StderrLog
    probeReportCreated = $ProbeReportCreated
    probeReport = if (Test-Path -LiteralPath $ProbeReport -PathType Leaf) { $ProbeReport } else { $null }
    probeStatus = if ($ProbeResult) { $ProbeResult.status } else { $null }
    temporaryAssetCleanup = if ($ProbeResult) { $ProbeResult.temporary_asset_cleanup } else { $null }
    diskCleanupPassed = $DiskCleanupPassed
    diskCleanupRemovedFiles = $DiskCleanupRemovedFiles
    crashContext = if ($RecentCrashContext) { $RecentCrashContext.FullName } else { $null }
    note = if ($Status -eq 'INCONCLUSIVE_NO_ENGINE_LOG') {
        'UnrealEditor-Cmd exited 0 without a non-empty Engine log or a fresh Python probe report. The import was not counted as run.'
    } elseif ($UnclassifiedAutomationErrorCount -gt 0 -or $AssetRegistryDeletionWarningCount -gt 0) {
        'The CSV import/readback probe passed. The Engine log also contains unclassified automation error lines and/or temporary asset deletion warnings; these observations are reported separately and are not attributed to P7 by this probe.'
    } else {
        $null
    }
}
$Report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $RunnerReport -Encoding utf8

[pscustomobject]@{
    Status = $Status
    ProcessExitCode = $ExitCode
    EngineLogCreated = $EngineLogCreated
    ProbeReportCreated = $ProbeReportCreated
    ProbeStatus = if ($ProbeResult) { $ProbeResult.status } else { $null }
    UnclassifiedAutomationErrorCount = $UnclassifiedAutomationErrorCount
    AssetRegistryDeletionWarningCount = $AssetRegistryDeletionWarningCount
    DiskCleanupPassed = $DiskCleanupPassed
    DiskCleanupRemovedFiles = $DiskCleanupRemovedFiles
    RunnerReport = $RunnerReport
    ProbeReport = if (Test-Path -LiteralPath $ProbeReport -PathType Leaf) { $ProbeReport } else { $null }
} | Format-List

if ($EngineLogCreated) {
    Get-Content -LiteralPath $EngineLog -Tail 30
}
if ($Status -ne 'PASS') {
    exit 1
}
