[CmdletBinding()]
param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8',
    [switch]$SkipBuild,
    [string]$Tag = (Get-Date -Format 'yyyyMMdd_HHmmss')
)

# F1 Chaos sandbox. This builds editor code and launches the source map directly in
# the Unreal Editor runtime; it does not run UAT, Stage, Cook, or Package.
$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$buildScript = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
$editorExecutable = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor.exe'
$map = '/Game/Naiwa/P2/Maps/L_NaiwaP2_21_Prototype'
$runDirectory = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../.workbuddy/work/20261020_全物理改造/stability'))
$ubaDirectory = Join-Path $runDirectory 'uba'
$logFile = [System.IO.Path]::GetFullPath((Join-Path $runDirectory "physics_sandbox_$Tag.log"))

foreach ($requiredPath in @($projectFile, $buildScript, $editorExecutable)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required physics sandbox input not found: $requiredPath"
    }
}
New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ubaDirectory -Force | Out-Null

if (-not $SkipBuild) {
    Write-Output 'Building Nai_LongEditor Win64 Development (no package)...'
    & $buildScript Nai_LongEditor Win64 Development "-Project=$projectFile" `
        -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=4 "-UBARootDir=$ubaDirectory"
    if ($LASTEXITCODE -ne 0) {
        throw "Nai_LongEditor build failed (exit $LASTEXITCODE)"
    }
}

$arguments = @(
    $projectFile, $map, '-game', '-windowed', '-ResX=1280', '-ResY=720', '-log',
    '-NaiwaP2PhysicsSandbox', '-NaiwaP2Level=2-1', "-abslog=$logFile"
)
$process = Start-Process -FilePath $editorExecutable -ArgumentList $arguments -PassThru
Write-Output "Started Naiwa P2 F1 physics sandbox (PID $($process.Id))."
Write-Output "map=$map log=$logFile"
Write-Output 'Mouse: drag the pouch and release to launch. Press R to restore the 2-1 starting layout.'
