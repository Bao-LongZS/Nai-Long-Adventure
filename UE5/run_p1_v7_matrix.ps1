[CmdletBinding()]
param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')

$ErrorActionPreference = 'Stop'
$projectRoot = Join-Path $PSScriptRoot 'NaiwaP0'
$projectFile = Join-Path $projectRoot 'Nai_Long.uproject'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$mapPackage = '/Game/Naiwa/P1/Maps/L_NaiwaP1_v7'
$mapFile = Join-Path $projectRoot 'Content/Naiwa/P1/Maps/L_NaiwaP1_v7.umap'
$savedDirectory = Join-Path $projectRoot 'Saved/NaiwaP1'
foreach ($file in @($projectFile, $editor, $mapFile)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Required V7 matrix file is missing: $file" }
}

$existingReports = @(Get-ChildItem -LiteralPath $savedDirectory -Filter 'matrix_*.json' -File -ErrorAction SilentlyContinue |
    ForEach-Object FullName)
$started = [DateTime]::UtcNow
$stamp = $started.ToString('yyyyMMddTHHmmssZ') + '_' + [Guid]::NewGuid().ToString('N')
$logFile = Join-Path $savedDirectory "matrix_v7_$stamp.log"
$arguments = @($projectFile, $mapPackage, '-game', '-RenderOffScreen', '-windowed', '-ResX=1600', '-ResY=900',
    '-ForceRes', '-unattended', '-nop4', '-nosplash', '-NaiwaP1Standalone', '-NaiwaP1Matrix', '-ExecCmds=t.MaxFPS 60', "-abslog=$logFile")
& $editor @arguments
if ($LASTEXITCODE -ne 0) { throw "V7 state matrix exited $LASTEXITCODE; inspect $logFile" }

$freshReports = @(Get-ChildItem -LiteralPath $savedDirectory -Filter 'matrix_*.json' -File |
    Where-Object { $_.FullName -notin $existingReports -and $_.LastWriteTimeUtc -ge $started })
if ($freshReports.Count -ne 1) { throw "Expected one fresh V7 matrix report, found $($freshReports.Count); inspect $logFile" }
$report = Get-Content -LiteralPath $freshReports[0].FullName -Raw -Encoding UTF8 | ConvertFrom-Json
if ($report.schema -ne 'NaiwaP1StateMatrixV1' -or $report.status -ne 'PASS' -or $report.map -ne 'L_NaiwaP1_v7') {
    throw "V7 state matrix failed identity/status: $($freshReports[0].FullName)"
}
$logText = Get-Content -LiteralPath $logFile -Raw -Encoding UTF8
if ($logText -notmatch 'restart_full .*death_count_before=3 death_count_after=0') {
    throw "V7 matrix did not record the third-death counter resetting to zero on R; inspect $logFile"
}
[pscustomobject]@{ status = $report.status; report = $freshReports[0].FullName; log = $logFile;
    death_count_reset = '3 -> 0'; events = $report.events.Count } | Format-List
