$ErrorActionPreference = 'Stop'
$webRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $webRoot '..')).Path
$ueRoot = 'C:\Program Files\Epic Games\UE_5.8'
$projectPath = Join-Path $repoRoot 'UE5\NaiwaP0\Nai_Long.uproject'
$editorCmd = Join-Path $ueRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$bootstrapScript = Join-Path $PSScriptRoot 'export-p1-map.py'
$configPath = Join-Path $repoRoot 'UE5\Scripts\p1_integrated_v7_config.json'
$mapFile = Join-Path $repoRoot 'UE5\NaiwaP0\Content\Naiwa\P1\Maps\L_NaiwaP1_v7.umap'
$outputPath = Join-Path $webRoot 'public\data\p1_v7_map.json'
$logPath = Join-Path $webRoot ("docs\p1-export-unreal-{0}.log" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))

foreach ($required in @($projectPath, $editorCmd, $bootstrapScript, $configPath, $mapFile)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required input not found: $required" }
}

$resolvedOutput = [System.IO.Path]::GetFullPath($outputPath)
if (-not $resolvedOutput.StartsWith($webRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Output must remain inside WebNative: $resolvedOutput"
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $resolvedOutput) | Out-Null

Write-Host "Read-only export of /Game/Naiwa/P1/Maps/L_NaiwaP1_v7 to $resolvedOutput"
$editorArgs = @(
    $projectPath,
    '-run=PythonScript',
    "-script=$bootstrapScript",
    "-NaiwaWebRepoRoot=$repoRoot",
    '-NaiwaWebP1Map=/Game/Naiwa/P1/Maps/L_NaiwaP1_v7',
    "-NaiwaWebP1Config=$configPath",
    "-NaiwaWebP1MapFile=$mapFile",
    "-NaiwaWebP1Output=$resolvedOutput",
    "-abslog=$logPath",
    '-log', '-unattended', '-nop4', '-nosplash', '-nullrhi', '-stdout', '-FullStdOutLogOutput', '-nosound'
)
Push-Location $repoRoot
try {
    & $editorCmd @editorArgs
    if ($LASTEXITCODE -ne 0) { throw "Unreal map exporter failed with exit code $LASTEXITCODE; see $logPath" }
}
finally { Pop-Location }

if (-not (Test-Path -LiteralPath $resolvedOutput -PathType Leaf)) {
    throw "Unreal exited successfully but did not create the expected file: $resolvedOutput"
}
Write-Host "PASS: $resolvedOutput"
