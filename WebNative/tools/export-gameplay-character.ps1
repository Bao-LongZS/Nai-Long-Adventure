$ErrorActionPreference = 'Stop'
$webRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $webRoot '..')).Path
$ueRoot = 'C:\Program Files\Epic Games\UE_5.8'
$projectPath = Join-Path $repoRoot 'UE5\NaiwaP0\Nai_Long.uproject'
$editorCmd = Join-Path $ueRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$scriptPath = Join-Path $PSScriptRoot 'export-gameplay-character.py'
$outputPath = Join-Path $webRoot 'public\assets\naiwa-character-e2.glb'
$logPath = Join-Path $webRoot ("docs\character-export-unreal-{0}.log" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))

foreach ($required in @($projectPath, $editorCmd, $scriptPath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required input not found: $required" }
}
$resolvedOutput = [System.IO.Path]::GetFullPath($outputPath)
if (-not $resolvedOutput.StartsWith($webRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Output must remain inside WebNative: $resolvedOutput"
}

$editorArgs = @(
    $projectPath,
    '-run=PythonScript',
    "-script=$scriptPath",
    "-NaiwaWebRepoRoot=$repoRoot",
    "-abslog=$logPath",
    '-log', '-unattended', '-nop4', '-nosplash', '-nullrhi', '-stdout', '-FullStdOutLogOutput', '-nosound'
)
Push-Location $repoRoot
try {
    & $editorCmd @editorArgs
    if ($LASTEXITCODE -ne 0) { throw "Unreal character exporter failed with exit code $LASTEXITCODE; see $logPath" }
}
finally { Pop-Location }

$reportPath = Join-Path $webRoot 'docs\gameplay-character-export.json'
if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) { throw "Missing export report: $reportPath" }
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
if ($report.status -ne 'PASS_UE_GLTF_EXPORT') { throw "Character export failed; see $reportPath" }
$compressScript = Join-Path $PSScriptRoot 'compress-gameplay-assets.mjs'
& node $compressScript
if ($LASTEXITCODE -ne 0) { throw "Asset compression failed with exit code $LASTEXITCODE" }
& node (Join-Path $PSScriptRoot 'build-asset-manifest.mjs')
if ($LASTEXITCODE -ne 0) { throw "Asset manifest generation failed with exit code $LASTEXITCODE" }
Write-Host "PASS: exported gameplay mesh and run animation to $webRoot\public\assets"
