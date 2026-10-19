[CmdletBinding()]
param(
    [ValidateSet('Development', 'Shipping')]
    [string]$Config = 'Development',
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$ArchiveRoot,
    [switch]$SkipBuild,
    [switch]$UseUBA
)
$ErrorActionPreference = 'Stop'

# Packaging pipeline: build Game target -> UAT BuildCookRun (cook/stage/pak/archive) -> verify output.
#
# Reproduces the two commands actually used to produce the 2026-10-18 demo builds
# (828 MB Development / 630 MB Shipping), so the packages satisfy the rule
# "if deleting it cannot be regenerated with one command, it does not belong in the repo".
#
# Three known pitfalls, all handled here:
#   1. UBA cas-directory permissions break PCH writes -> -NoUBA by default; pass -UseUBA to opt out.
#   2. A failed RunUAT deletes UnrealEditor-NaiwaRuntime.dll, and the following cook then
#      cannot find the module -> verify the DLL before cooking and rebuild the editor if missing.
#   3. A Zen oplog 404 breaks the pak stage -> relies on bUseIoStore=False (classic pak) in
#      DefaultEngine.ini; checked at startup and reported as a warning only.
#
# Usage:
#   .\package_demo.ps1                       # Development -> Saved/DemoBuild
#   .\package_demo.ps1 -Config Shipping     # Shipping    -> Saved/DemoBuild_Shipping
#   .\package_demo.ps1 -SkipBuild           # rebuild cook/stage/pak only

$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$recordDirectory = Join-Path $PSScriptRoot 'Records'
$projectName = 'Nai_Long'

if (-not (Test-Path -LiteralPath $projectFile)) { throw "Project file not found: $projectFile" }
$runUat = Join-Path $Engine 'Engine/Build/BatchFiles/RunUAT.bat'
$buildBat = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
foreach ($tool in @($runUat, $buildBat)) {
    if (-not (Test-Path -LiteralPath $tool)) { throw "Engine tool not found: $tool (use -Engine to point at the UE install)" }
}

if (-not $ArchiveRoot) {
    $folder = if ($Config -eq 'Shipping') { 'DemoBuild_Shipping' } else { 'DemoBuild' }
    $ArchiveRoot = Join-Path $PSScriptRoot "NaiwaP0/Saved/$folder"
}
New-Item -ItemType Directory -Path $recordDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ArchiveRoot -Force | Out-Null

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$logPath = Join-Path $recordDirectory "package_$($Config.ToLowerInvariant())_$stamp.log"
$ubaArgs = if ($UseUBA) { @() } else { @('-NoUBA') }

function Write-Stage([string]$Text) {
    $line = "=== $Text ==="
    Write-Host $line
    Add-Content -LiteralPath $logPath -Value $line -Encoding UTF8
}

# -- 0. Config check: classic pak requires bUseIoStore=False ---------
$engineIni = Join-Path $PSScriptRoot 'NaiwaP0/Config/DefaultEngine.ini'
if (Test-Path -LiteralPath $engineIni) {
    $ioStoreOff = Select-String -LiteralPath $engineIni -Pattern 'bUseIoStore\s*=\s*False' -Quiet
    if (-not $ioStoreOff) {
        Write-Warning 'DefaultEngine.ini does not set bUseIoStore=False; if the pak stage reports a Zen oplog 404, set it back to False first.'
    }
}

Write-Stage "package $Config -> $ArchiveRoot"
Write-Stage "log $logPath"

# -- 1. Pre-cook: UnrealEditor-NaiwaRuntime.dll must exist ----------
$runtimeDll = Join-Path $PSScriptRoot 'NaiwaP0/Binaries/Win64/UnrealEditor-NaiwaRuntime.dll'
if (-not (Test-Path -LiteralPath $runtimeDll)) {
    Write-Warning 'UnrealEditor-NaiwaRuntime.dll is missing (a failed RunUAT deletes it); rebuilding the editor target first.'
    Write-Stage 'build Nai_LongEditor Win64 Development (restore editor module)'
    & $buildBat "$($projectName)Editor" Win64 Development "-Project=$projectFile" -WaitMutex @ubaArgs 2>&1 |
        Tee-Object -FilePath $logPath -Append
    if ($LASTEXITCODE -ne 0) { throw "Editor target build failed (exit $LASTEXITCODE)" }
    if (-not (Test-Path -LiteralPath $runtimeDll)) { throw "Editor rebuild finished but $runtimeDll is still missing" }
}

# -- 2. Build Game target -------------------------------------------
if (-not $SkipBuild) {
    Write-Stage "build $projectName Win64 $Config"
    & $buildBat $projectName Win64 $Config "-Project=$projectFile" -WaitMutex @ubaArgs 2>&1 |
        Tee-Object -FilePath $logPath -Append
    if ($LASTEXITCODE -ne 0) { throw "Game target build failed (exit $LASTEXITCODE)" }
} else {
    Write-Stage 'build skipped (-SkipBuild)'
}

# -- 3. cook / stage / pak / archive --------------------------------
# Arguments match the 2026-10-18 run. -nobuild because step 2 already built the target.
$uatArgs = @(
    'BuildCookRun',
    "-project=$projectFile",
    '-noP4',
    '-platform=Win64',
    "-clientconfig=$Config",
    '-nobuild',
    '-cook',
    '-stage',
    '-pak',
    '-archive',
    "-archivedirectory=$ArchiveRoot",
    '-utf8output',
    '-nodebuginfo',
    '-prereqs',
    '-unattended'
)
if (-not $UseUBA) { $uatArgs += '-NoUBA' }

Write-Stage "UAT $($uatArgs -join ' ')"
& $runUat @uatArgs 2>&1 | Tee-Object -FilePath $logPath -Append
if ($LASTEXITCODE -ne 0) { throw "BuildCookRun failed (exit $LASTEXITCODE); see $logPath" }

# -- 4. Verify output -----------------------------------------------
$windowsDir = Join-Path $ArchiveRoot 'Windows'
$exe = Join-Path $windowsDir "$projectName.exe"
if (-not (Test-Path -LiteralPath $exe)) { throw "Packaged executable not found: $exe" }

$pakFiles = Get-ChildItem -LiteralPath $windowsDir -Recurse -Filter '*.pak' -File -ErrorAction SilentlyContinue
$pakBytes = ($pakFiles | Measure-Object -Property Length -Sum).Sum
$totalBytes = (Get-ChildItem -LiteralPath $ArchiveRoot -Recurse -File | Measure-Object -Property Length -Sum).Sum

Write-Stage 'result'
$summary = @(
    "config      : $Config",
    "executable  : $exe",
    "pak files   : $($pakFiles.Count)",
    "pak size    : $([math]::Round($pakBytes / 1MB, 2)) MB",
    "total size  : $([math]::Round($totalBytes / 1MB, 2)) MB",
    "log         : $logPath"
)
$summary | ForEach-Object { Write-Host $_; Add-Content -LiteralPath $logPath -Value $_ -Encoding UTF8 }

if ($pakFiles.Count -eq 0) { throw 'No .pak in the archive; classic pak packaging did not take effect' }
Write-Host 'PACKAGE_OK'
