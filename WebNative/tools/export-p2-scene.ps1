param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$webRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $webRoot '..')).Path
$ueRoot = 'C:\Program Files\Epic Games\UE_5.8'
$projectPath = Join-Path $repoRoot 'UE5\NaiwaP0\Nai_Long.uproject'
$buildScript = Join-Path $ueRoot 'Engine\Build\BatchFiles\Build.bat'
$editorCmd = Join-Path $ueRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$bootstrapScript = Join-Path $PSScriptRoot 'ue-export-bootstrap.py'
$ubaRoot = Join-Path $webRoot '.uba'
$outputPath = Join-Path $webRoot 'public\data\p2_21_scene.json'
$logPath = Join-Path $webRoot ("docs\p2-export-unreal-{0}.log" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
$runtimeSource = Join-Path $repoRoot 'UE5\NaiwaP0\Source\NaiwaRuntime'

foreach ($required in @($projectPath, $editorCmd, $bootstrapScript, (Join-Path $runtimeSource 'NaiwaP2SceneLibrary.cpp'), (Join-Path $runtimeSource 'NaiwaP2PhysicsProfile.h'), (Join-Path $runtimeSource 'NaiwaP2ProjectileDriver.h'), (Join-Path $runtimeSource 'NaiwaP2GameMode.cpp'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required input not found: $required" }
}

$resolvedOutput = [System.IO.Path]::GetFullPath($outputPath)
if (-not $resolvedOutput.StartsWith($webRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Output must remain inside WebNative: $resolvedOutput"
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $resolvedOutput) | Out-Null

$sourceSceneHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $runtimeSource 'NaiwaP2SceneLibrary.cpp')).Hash.ToLowerInvariant()
$sourceProfileHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $runtimeSource 'NaiwaP2PhysicsProfile.h')).Hash.ToLowerInvariant()
$sourceProjectileHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $runtimeSource 'NaiwaP2ProjectileDriver.h')).Hash.ToLowerInvariant()
$sourceGameModeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $runtimeSource 'NaiwaP2GameMode.cpp')).Hash.ToLowerInvariant()
$sourceExporterHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $runtimeSource 'NaiwaWebNativeDataExport.cpp')).Hash.ToLowerInvariant()
$sourceRuntimeEntryHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $runtimeSource 'NaiwaRuntime.cpp')).Hash.ToLowerInvariant()

if (-not $SkipBuild) {
    Write-Host 'Building Nai_LongEditor so the read-only exporter is available...'
    Push-Location $repoRoot
    try {
        & $buildScript 'Nai_LongEditor' 'Win64' 'Development' "-Project=$projectPath" '-WaitMutex' '-NoHotReloadFromIDE' '-NoUBA' "-UBARootDir=$ubaRoot" '-MaxParallelActions=4'
        if ($LASTEXITCODE -ne 0) { throw "Unreal build failed with exit code $LASTEXITCODE" }
    }
    finally { Pop-Location }
}

Write-Host "Exporting current FSceneLibrary::Make21 to $resolvedOutput"
$editorArgs = @(
    $projectPath,
    '-run=PythonScript',
    "-script=$bootstrapScript",
    "-NaiwaWebExportP2Scene=$resolvedOutput",
    "-NaiwaWebP2SceneSourceSha256=$sourceSceneHash",
    "-NaiwaWebP2ProfileSourceSha256=$sourceProfileHash",
    "-NaiwaWebP2ProjectileSourceSha256=$sourceProjectileHash",
    "-NaiwaWebP2GameModeSourceSha256=$sourceGameModeHash",
    "-NaiwaWebExporterSha256=$sourceExporterHash",
    "-NaiwaWebRuntimeEntrySha256=$sourceRuntimeEntryHash",
    "-abslog=$logPath",
    '-log', '-unattended', '-nop4', '-nosplash', '-nullrhi', '-stdout', '-FullStdOutLogOutput', '-nosound'
)
Push-Location $repoRoot
try {
    & $editorCmd @editorArgs
    if ($LASTEXITCODE -ne 0) { throw "Unreal scene exporter failed with exit code $LASTEXITCODE; see $logPath" }
}
finally { Pop-Location }

if (-not (Test-Path -LiteralPath $resolvedOutput -PathType Leaf)) {
    throw "Unreal exited successfully but did not create the expected file: $resolvedOutput"
}
Write-Host "PASS: $resolvedOutput"
