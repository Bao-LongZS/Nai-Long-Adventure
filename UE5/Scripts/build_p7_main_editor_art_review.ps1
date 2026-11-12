[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectRoot = Join-Path $RepositoryRoot 'UE5\NaiwaP0'
$Project = Join-Path $ProjectRoot 'Nai_Long.uproject'
$ProbeProject = Join-Path $ProjectRoot 'Nai_LongP7ArtReview_TEMP.uproject'
$BuildBatch = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$BuildLog = Join-Path $EvidenceRoot "P7_MainNaiLongEditor_ApprovedArt_$Timestamp.log"
$CreatedProbe = $false

foreach ($path in @($Project, $BuildBatch)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required P7 Editor build input was not found: $path"
    }
}
if (Test-Path -LiteralPath $ProbeProject) {
    throw "Temporary project descriptor already exists; refusing to overwrite it: $ProbeProject"
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

try {
    Copy-Item -LiteralPath $Project -Destination $ProbeProject
    $CreatedProbe = $true
    "P7 main Nai_LongEditor build for approved P7 HUD art - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" |
        Set-Content -LiteralPath $BuildLog -Encoding Unicode
    $BuildArguments = @(
        'Nai_LongEditor', 'Win64', 'Development', $ProbeProject,
        '-NoUBA', '-NoHotReloadFromIDE', '-NoLiveCoding', '-WaitMutex', '-MaxParallelActions=4'
    )
    $BuildOutput = & $BuildBatch @BuildArguments 2>&1
    $BuildExitCode = $LASTEXITCODE
    $BuildOutput | ForEach-Object { $_.ToString() } | Add-Content -LiteralPath $BuildLog -Encoding Unicode
    $BuildOutput | Select-Object -Last 35
    if ($BuildExitCode -ne 0) {
        throw "P7 main Nai_LongEditor build failed with exit code $BuildExitCode. See $BuildLog"
    }
    $BuildText = Get-Content -LiteralPath $BuildLog -Raw -Encoding Unicode
    if ($BuildText -notmatch 'Result: Succeeded' -or
        ($BuildText -notmatch 'WriteMetadata Nai_LongEditor\.target' -and
         $BuildText -notmatch 'Target is up to date')) {
        throw "P7 main Nai_LongEditor build did not produce the expected success record. See $BuildLog"
    }
    Write-Output "P7 main Nai_LongEditor build succeeded. Log: $BuildLog"
} finally {
    if ($CreatedProbe -and (Test-Path -LiteralPath $ProbeProject)) {
        $ResolvedProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
        $ResolvedProbeProject = (Resolve-Path -LiteralPath $ProbeProject).Path
        $ExpectedProbeProject = Join-Path $ResolvedProjectRoot 'Nai_LongP7ArtReview_TEMP.uproject'
        if (-not $ResolvedProbeProject.Equals($ExpectedProbeProject, [System.StringComparison]::OrdinalIgnoreCase) -or
            -not $ResolvedProbeProject.StartsWith($ResolvedProjectRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove unexpected temporary project descriptor: $ResolvedProbeProject"
        }
        Remove-Item -LiteralPath $ResolvedProbeProject -Force
    }
}
