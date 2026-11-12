[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectRoot = Join-Path $RepositoryRoot 'UE5\NaiwaP0'
$Project = Join-Path $ProjectRoot 'Nai_Long.uproject'
$BuildBatch = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$UbaRoot = Join-Path $RepositoryRoot 'tmp\P7UbaCache'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$BuildLog = Join-Path $EvidenceRoot "P7_MainNaiLongGameBuild_$Timestamp.log"

foreach ($path in @($Project, $BuildBatch)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required P7 build input was not found: $path"
    }
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
New-Item -ItemType Directory -Force -Path $UbaRoot | Out-Null

$BuildArguments = @(
    'Nai_Long', 'Win64', 'Development', $Project,
    '-NoUBA', '-NoHotReloadFromIDE', '-WaitMutex', '-MaxParallelActions=4', "-UBARootDir=$UbaRoot"
)
if ($BuildArguments -contains '-NoLiveCoding') {
    throw 'Do not pass -NoLiveCoding to the main Game target; it causes a UHT registry layout mismatch with the prebuilt Engine.'
}
if ($Clean) {
    $BuildArguments += '-clean'
}
$BuildLogHeader = "P7 main Nai_Long Game Development build using the project descriptor - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$BuildLogHeader | Set-Content -LiteralPath $BuildLog -Encoding utf8
$BuildOutput = & $BuildBatch @BuildArguments 2>&1 | Tee-Object -FilePath $BuildLog -Append
$BuildExitCode = $LASTEXITCODE
$BuildOutput | Select-Object -Last 30

if ($BuildExitCode -ne 0) {
    throw "P7 main Nai_Long Game build failed with exit code $BuildExitCode. See $BuildLog"
}

$BuildText = Get-Content -LiteralPath $BuildLog -Raw
if ($BuildText -notmatch 'Result: Succeeded' -or
    ($BuildText -notmatch 'WriteMetadata Nai_Long\.target' -and
     $BuildText -notmatch 'Target is up to date')) {
    throw "P7 main Nai_Long Game build did not produce the expected success record. See $BuildLog"
}

Write-Output "P7 main Nai_Long Game build succeeded. Log: $BuildLog"
