[CmdletBinding()]
param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8',
    [switch]$SkipBuild,
    [double]$Angle = 19.37,
    [ValidateRange(0.05, 1.0)]
    [double]$Power = 1.0,
    [string]$TargetBlock = 'T.c1',
    [string]$Tag = (Get-Date -Format 'yyyyMMdd_HHmmss')
)

# Hidden one-shot F1 Chaos impact probe. No UAT, Cook, Stage, or Package.
$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$buildScript = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
$editorExecutable = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor.exe'
$map = '/Game/Naiwa/P2/Maps/L_NaiwaP2_21_Prototype'
$sourceMap = Join-Path $PSScriptRoot 'NaiwaP0/Content/Naiwa/P2/Maps/L_NaiwaP2_21_Prototype.umap'
$testDirectory = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../.workbuddy/work/20261020_全物理改造/stability'))
$ubaDirectory = Join-Path $testDirectory 'uba'
$logFile = [System.IO.Path]::GetFullPath((Join-Path $testDirectory "physics_impact_$Tag.log"))

foreach ($requiredPath in @($projectFile, $buildScript, $editorExecutable, $sourceMap)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required physics impact probe input not found: $requiredPath"
    }
}
New-Item -ItemType Directory -Path $testDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ubaDirectory -Force | Out-Null

if (-not $SkipBuild) {
    Write-Output 'Building Nai_LongEditor Win64 Development (no package)...'
    & $buildScript Nai_LongEditor Win64 Development "-Project=$projectFile" `
        -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=4 "-UBARootDir=$ubaDirectory"
    if ($LASTEXITCODE -ne 0) {
        throw "Nai_LongEditor build failed (exit $LASTEXITCODE)"
    }
}

Remove-Item -LiteralPath $logFile -Force -ErrorAction SilentlyContinue
$editorArguments = @(
    $projectFile, $sourceMap, '-game', '-PIEVIACONSOLE', '-Multiprocess',
    '-unattended', '-nop4', '-nosplash', '-NullRHI', '-NoSound', '-stdout', '-log',
    '-NaiwaP2PhysicsImpactProbe', '-NaiwaP2Level=2-1',
    "-NaiwaP2ImpactAngle=$($Angle.ToString([Globalization.CultureInfo]::InvariantCulture))",
    "-NaiwaP2ImpactPower=$($Power.ToString([Globalization.CultureInfo]::InvariantCulture))",
    "-NaiwaP2ImpactTarget=$TargetBlock", "-abslog=$logFile"
)
$process = Start-Process -FilePath $editorExecutable -ArgumentList $editorArguments `
    -WorkingDirectory (Split-Path -Parent $editorExecutable) -WindowStyle Hidden -PassThru -Wait

$output = if (Test-Path -LiteralPath $logFile) { Get-Content -LiteralPath $logFile -Raw } else { '' }
$summary = [regex]::Matches($output, 'NAIWA_P2_PHYS_IMPACT_PROBE pass=\d[^\r\n]*') | Select-Object -Last 1
$hits = [regex]::Matches($output, 'NAIWA_P2_PHYS_EGG_HIT [^\r\n]*') | ForEach-Object { $_.Value }
$blocks = [regex]::Matches($output, 'NAIWA_P2_PHYS_SANDBOX_BLOCK_FINAL [^\r\n]*') | ForEach-Object { $_.Value }

Write-Output "level=2-1 map=$map exit=$($process.ExitCode) log=$logFile"
foreach ($hit in $hits) { Write-Output "  $hit" }
foreach ($block in $blocks) { Write-Output "  $block" }
if (-not $summary) {
    throw "NAIWA_P2_PHYS_IMPACT_PROBE result missing; the hidden runtime did not complete. Process exit=$($process.ExitCode), log=$logFile"
}
Write-Output "  $($summary[0].Value)"
if ($process.ExitCode -ne 0) { exit $process.ExitCode }
if ($summary[0].Value -notmatch 'pass=1(?:\s|$)') { exit 1 }
exit 0
