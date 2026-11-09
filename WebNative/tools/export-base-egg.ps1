$ErrorActionPreference = 'Stop'
$webRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $webRoot '..')).Path
$ueRoot = 'C:\Program Files\Epic Games\UE_5.8'
$projectPath = Join-Path $repoRoot 'UE5\NaiwaP0\Nai_Long.uproject'
$editorCmd = Join-Path $ueRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$scriptPath = Join-Path $PSScriptRoot 'export-base-egg.py'
$outputPath = Join-Path $webRoot 'public\assets\naiwa-egg-base.glb'
$reportPath = Join-Path $webRoot 'docs\base-egg-export.json'
$runId = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath = Join-Path $webRoot ("docs\base-egg-export-unreal-$runId.log")

foreach ($required in @($projectPath, $editorCmd, $scriptPath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required input not found: $required" }
}
$resolvedOutput = [System.IO.Path]::GetFullPath($outputPath)
if (-not $resolvedOutput.StartsWith($webRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Output must remain inside WebNative: $resolvedOutput"
}

$argumentLine = '"{0}" -run=PythonScript -script="{1}" -NaiwaWebRepoRoot="{2}" -abslog="{3}" -log -unattended -nop4 -nosplash -nullrhi -nosound' -f $projectPath, $scriptPath, $repoRoot, $logPath
$process = Start-Process -FilePath $editorCmd -ArgumentList $argumentLine -WorkingDirectory $repoRoot `
    -WindowStyle Hidden -Wait -PassThru
if ($process.ExitCode -ne 0) { throw "Unreal base egg exporter failed with exit code $($process.ExitCode); see $logPath" }

if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) { throw "Missing export report: $reportPath" }
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
if ($report.status -ne 'PASS_UE_GLTF_EXPORT') { throw "Base egg export failed; see $reportPath" }
if (-not (Test-Path -LiteralPath $outputPath -PathType Leaf)) { throw "Missing GLB output: $outputPath" }
& node (Join-Path $PSScriptRoot 'build-asset-manifest.mjs')
if ($LASTEXITCODE -ne 0) { throw "Asset manifest update failed with exit code $LASTEXITCODE" }
Write-Host "PASS: exported approved base egg SkeletalMesh to $outputPath"
