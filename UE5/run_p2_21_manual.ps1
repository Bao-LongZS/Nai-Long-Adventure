[CmdletBinding()]
param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8',
    [switch]$Build,
    [ValidateRange(15, 180)]
    [int]$StartupTimeoutSeconds = 90
)

# Launch the canonical 2-1 map from the source project. The map name selects
# the full Chaos path; no automation-only physics override is required here.
# Deliberately excludes flags such as -Multiprocess and -NullRHI.
$ErrorActionPreference = 'Stop'
$projectDirectory = Join-Path $PSScriptRoot 'NaiwaP0'
$projectFile = Join-Path $projectDirectory 'Nai_Long.uproject'
$editorExecutable = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor.exe'
$buildScript = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
$sourceMap = Join-Path $projectDirectory 'Content/Naiwa/P2/Maps/L_NaiwaP2_21.umap'
$map = '/Game/Naiwa/P2/Maps/L_NaiwaP2_21'
$runDirectory = Join-Path $projectDirectory 'Saved/Logs'
$logFile = Join-Path $runDirectory ("p2_21_manual_{0}.log" -f (Get-Date -Format 'yyyyMMdd_HHmmss'))

foreach ($requiredPath in @($projectFile, $editorExecutable, $sourceMap)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required P2 2-1 launch input not found: $requiredPath"
    }
}

$existingEditor = @(Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue)
if ($existingEditor.Count -gt 0) {
    $ids = ($existingEditor | ForEach-Object { $_.Id }) -join ', '
    throw "UnrealEditor is already running (PID $ids). Close that editor/game window, then run this script again."
}

New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null

if ($Build) {
    if (-not (Test-Path -LiteralPath $buildScript)) {
        throw "Unreal Build.bat not found: $buildScript"
    }
    Write-Output 'Building Nai_LongEditor Win64 Development (no package)...'
    & $buildScript Nai_LongEditor Win64 Development "-Project=$projectFile" `
        -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=4
    if ($LASTEXITCODE -ne 0) {
        throw "Nai_LongEditor build failed (exit $LASTEXITCODE)"
    }
}

$editorArguments = @(
    $projectFile,
    $map,
    '-game',
    '-windowed',
    '-ResX=1280',
    '-ResY=720',
    '-log',
    "-abslog=$logFile"
)
$process = Start-Process -FilePath $editorExecutable -ArgumentList ($editorArguments -join ' ') `
    -WorkingDirectory $projectDirectory -PassThru
Write-Output "Started interactive P2 2-1 (PID $($process.Id))."
Write-Output "map=$map"
Write-Output "log=$logFile"

$deadline = (Get-Date).AddSeconds($StartupTimeoutSeconds)
while ((Get-Date) -lt $deadline) {
    if (Test-Path -LiteralPath $logFile) {
        $physicsReady = Select-String -LiteralPath $logFile `
            -Pattern 'NAIWA_P2_PHYS_SPAWN level=2-1 blocks=13 goals=4 physics=1' -Quiet
        $ready = Select-String -LiteralPath $logFile -Pattern 'NAIWA_P2_PLAY_READY level=2-1' -Quiet
        if ($physicsReady -and $ready) {
            Write-Output 'Canonical 2-1 map started with full Chaos physics; the game window is ready.'
            exit 0
        }
    }
    $process.Refresh()
    if ($process.HasExited) {
        $tail = if (Test-Path -LiteralPath $logFile) { Get-Content -LiteralPath $logFile -Tail 80 } else { @('(No run log was produced.)') }
        $tail | ForEach-Object { Write-Output $_ }
        throw "UnrealEditor exited before the 2-1 ready marker (exit $($process.ExitCode)); inspect $logFile"
    }
    Start-Sleep -Seconds 1
}

$tail = if (Test-Path -LiteralPath $logFile) { Get-Content -LiteralPath $logFile -Tail 80 } else { @('(No run log was produced.)') }
$tail | ForEach-Object { Write-Output $_ }
throw "Timed out waiting for NAIWA_P2_PLAY_READY after $StartupTimeoutSeconds seconds; inspect $logFile"
