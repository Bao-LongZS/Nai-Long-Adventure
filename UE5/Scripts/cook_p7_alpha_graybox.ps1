[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$MapName = '/Game/Naiwa/P7/Maps/L_P7_Alpha01_Graybox'
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectRoot = Join-Path $RepositoryRoot 'UE5\NaiwaP0'
$Project = Join-Path $ProjectRoot 'Nai_Long.uproject'
$RunUAT = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$MapPackage = Join-Path $ProjectRoot 'Content\Naiwa\P7\Maps\L_P7_Alpha01_Graybox.umap'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$StageDirectory = Join-Path $RepositoryRoot "tmp\P7CookedAlphaGraybox_$Timestamp"
$BuildLog = Join-Path $EvidenceRoot "P7_AlphaGrayboxCookStage_$Timestamp.log"
$ReportPath = Join-Path $EvidenceRoot "P7_AlphaGrayboxCookStage_$Timestamp.json"

foreach ($path in @($Project, $RunUAT, $MapPackage)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required P7 cook input was not found: $path"
    }
}
if (Test-Path -LiteralPath $StageDirectory) {
    throw "P7 staging directory already exists; refusing to overwrite it: $StageDirectory"
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

$Arguments = @(
    'BuildCookRun',
    "-project=$Project",
    '-noP4',
    '-platform=Win64',
    '-clientconfig=Development',
    '-skipbuild',
    '-cook',
    '-stage',
    '-pak',
    "-map=$MapName",
    "-stagingdirectory=$StageDirectory",
    '-unattended',
    '-utf8output'
)

"P7 Alpha graybox cook/stage - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" |
    Set-Content -LiteralPath $BuildLog -Encoding utf8
$BuildOutput = & $RunUAT @Arguments 2>&1
$ExitCode = $LASTEXITCODE
$BuildOutput | Out-File -LiteralPath $BuildLog -Append -Encoding utf8
$BuildOutput | Select-Object -Last 30
$LogText = Get-Content -LiteralPath $BuildLog -Raw
$WindowsStage = Join-Path $StageDirectory 'Windows'
$Executable = Join-Path $WindowsStage 'Nai_Long.exe'
$ContentPak = Join-Path $WindowsStage 'Nai_Long\Content\Paks\Nai_Long-Windows.pak'
$Passed = $ExitCode -eq 0 -and $LogText -match 'BUILD SUCCESSFUL' -and
    (Test-Path -LiteralPath $Executable -PathType Leaf) -and
    (Test-Path -LiteralPath $ContentPak -PathType Leaf)

$Report = [ordered]@{
    test = "Development cook/stage: $MapName"
    status = if ($Passed) { 'PASS' } else { 'FAIL' }
    startedAt = $Timestamp
    processExitCode = $ExitCode
    map = $MapName
    project = $Project
    stageDirectory = $WindowsStage
    executable = $Executable
    contentPak = $ContentPak
    buildLog = $BuildLog
}
$Report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ReportPath -Encoding utf8

[pscustomobject]@{
    Status = $Report.status
    ProcessExitCode = $ExitCode
    StageDirectory = $WindowsStage
    Map = $MapName
    Report = $ReportPath
    BuildLog = $BuildLog
} | Format-List

if (-not $Passed) {
    throw "P7 Alpha graybox cook/stage failed. See $BuildLog and $ReportPath"
}
