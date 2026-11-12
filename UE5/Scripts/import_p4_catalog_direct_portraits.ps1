[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [int]$TimeoutSeconds = 240
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectRoot = Join-Path $RepositoryRoot 'UE5\NaiwaP0'
$Project = Join-Path $ProjectRoot 'NaiwaP4CatalogImport_TEMP.uproject'
$EditorCommandlet = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$PythonScript = Join-Path $PSScriptRoot 'import_p4_catalog_direct_portraits.py'
$SourceRoot = Join-Path $RepositoryRoot 'UE5\SourceAssets\P4\UI\Alpha\CatalogDirect'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p4_alpha\evidence\CatalogDirect'
$SafeTempRoot = Join-Path $RepositoryRoot 'tmp\P4CatalogPortraitImportTemp'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$EngineLog = Join-Path $EvidenceRoot "P4_A1_CatalogPortraits_Import_$Timestamp.log"
$StdoutLog = Join-Path $EvidenceRoot "P4_A1_CatalogPortraits_Import_$Timestamp.stdout.log"
$StderrLog = Join-Path $EvidenceRoot "P4_A1_CatalogPortraits_Import_$Timestamp.stderr.log"

foreach ($Path in @($Project, $EditorCommandlet, $PythonScript, (Join-Path $SourceRoot 'P4_A1_CatalogPortraits_manifest_v01.csv'))) {
    if ($Path -eq $Project) { continue }
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required catalog portrait import input was not found: $Path"
    }
}
if (Test-Path -LiteralPath $Project) {
    throw "Temporary import project already exists; refusing to overwrite it: $Project"
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot,$SafeTempRoot | Out-Null
$PreviousReportUtc = Get-ChildItem -LiteralPath $EvidenceRoot -Filter 'P4_A1_CatalogPortraits_Import_*.json' -ErrorAction SilentlyContinue |
    Measure-Object -Property LastWriteTimeUtc -Maximum | Select-Object -ExpandProperty Maximum
if (-not $PreviousReportUtc) { $PreviousReportUtc = [DateTime]::MinValue }
$ImportProjectConfig = [ordered]@{
    FileVersion = 3
    EngineAssociation = '5.8'
    Category = ''
    Description = 'Temporary content-only project descriptor for P4 portrait texture import.'
    Plugins = @(
        @{ Name = 'PythonScriptPlugin'; Enabled = $true }
        @{ Name = 'EditorScriptingUtilities'; Enabled = $true }
    )
}
$ImportProjectConfig | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $Project -Encoding utf8
$EngineLog | Set-Content -LiteralPath $EngineLog -Encoding utf8
$PreviousTemp = $env:TEMP
$PreviousTmp = $env:TMP
try {
    $env:TEMP = $SafeTempRoot
    $env:TMP = $SafeTempRoot
    $ArgumentLine = '"' + $Project + '" -unattended -nop4 -nosplash -nullrhi -NoMutex -stdout -FullStdOutLogOutput -log -ExecutePythonScript=' + $PythonScript + ' -AbsLog=' + $EngineLog
    $Process = Start-Process -FilePath $EditorCommandlet -ArgumentList $ArgumentLine `
        -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $StdoutLog -RedirectStandardError $StderrLog
    $Finished = $Process.WaitForExit($TimeoutSeconds * 1000)
    if (-not $Finished -and -not $Process.HasExited) {
        Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue
        $Process.WaitForExit()
    }
    $Process.Refresh()
    $ExitCode = if ($Finished) { $Process.ExitCode } else { $null }
} finally {
    $env:TEMP = $PreviousTemp
    $env:TMP = $PreviousTmp
    Remove-Item -LiteralPath $Project -Force -ErrorAction SilentlyContinue
}

$ReportFile = Get-ChildItem -LiteralPath $EvidenceRoot -Filter 'P4_A1_CatalogPortraits_Import_*.json' |
    Where-Object { $_.LastWriteTimeUtc -gt $PreviousReportUtc } |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $ReportFile) {
    Write-Output "UnrealEditor-Cmd exit code: $ExitCode"
    Get-Content -LiteralPath $EngineLog -Tail 80
    if (Test-Path -LiteralPath $StdoutLog) { Get-Content -LiteralPath $StdoutLog -Tail 50 }
    if (Test-Path -LiteralPath $StderrLog) { Get-Content -LiteralPath $StderrLog -Tail 30 }
    throw "P4 portrait import produced no fresh report. See $EngineLog"
}
$Report = Get-Content -LiteralPath $ReportFile.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
if ($Report.status -ne 'textures_imported_pending_hud_layout_and_runtime_review' -or @($Report.assets).Count -ne 4) {
    Get-Content -LiteralPath $ReportFile.FullName -Raw
    throw "P4 portrait import did not verify all four textures. See $($ReportFile.FullName) (Editor-Cmd exit code: $ExitCode)"
}

$MissingAssets = @($Report.assets | ForEach-Object {
    $RelativeAsset = $_.asset_path -replace '^/Game/', ''
    $AssetFile = Join-Path $ProjectRoot ('Content\' + ($RelativeAsset -replace '/', '\') + '.uasset')
    if (-not (Test-Path -LiteralPath $AssetFile -PathType Leaf)) { $AssetFile }
})
if ($MissingAssets.Count -gt 0) {
    throw "Import report references missing UE Texture2D files: $($MissingAssets -join ', ')"
}
$Report.assets | Select-Object asset_id,asset_path,dimensions_px,action,texture_settings_readback | Format-Table -AutoSize
Write-Output "Report: $($ReportFile.FullName)"
Write-Output "Engine log: $EngineLog"
