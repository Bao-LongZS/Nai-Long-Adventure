[CmdletBinding()]
param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateSet('2-1', '2-2', '2-3')]
    [string]$Level = '2-1',
    [switch]$SkipBuild,
    [double]$Angle = 19.37,
    [ValidateRange(0.05, 1.0)]
    [double]$Power = 1.0,
    [switch]$ResetAfterShot,
    [switch]$RefundZeroKillShot,
    [switch]$MultiShotTest,
    [switch]$FrameTimeProbe,
    [ValidateRange(1, 100)]
    [int]$FrameScreenPercentage = 100,
    [switch]$VisibleWindow,
    [switch]$FullscreenProbe,
    [string]$TraceFile = '',
    [ValidateSet('', 'CHICK', 'MOUSE', 'SOY', 'TEA')]
    [string]$SkillProbe = '',
    [string]$SequenceAngles = '',
    [ValidateRange(0.1, 2.0)]
    [double]$StructureMassMultiplier = 1.0,
    [string]$Tag = (Get-Date -Format 'yyyyMMdd_HHmmss')
)

# P2 full-level runtime physics probe. No UAT, Cook, Stage, or Package.
$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$buildScript = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
$editorExecutable = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor.exe'
$levelAssetId = $Level -replace '-', ''
$map = "/Game/Naiwa/P2/Maps/L_NaiwaP2_$levelAssetId"
$sourceMap = Join-Path $PSScriptRoot "NaiwaP0/Content/Naiwa/P2/Maps/L_NaiwaP2_$levelAssetId.umap"
if ($Level -eq '2-3' -and -not (Test-Path -LiteralPath $sourceMap)) {
    # 2-3 is generated directly by SceneLibrary at runtime; use the existing P2
    # boot map as the container while -NaiwaP2Level selects the actual scene.
    $map = '/Game/Naiwa/P2/Maps/L_NaiwaP2_22'
    $sourceMap = Join-Path $PSScriptRoot 'NaiwaP0/Content/Naiwa/P2/Maps/L_NaiwaP2_22.umap'
}
if (-not $SequenceAngles) {
    $SequenceAngles = switch ($Level) {
        '2-2' { '19.85+19.85+19.85+19.85+19.85' }
        '2-3' { '19.85+19.85+19.85+19.85' }
        default { '19.85+19.85+1' }
    }
}
$projectRoot = Split-Path -Parent $PSScriptRoot
$testDirectory = Join-Path $projectRoot '.workbuddy/work/20261020_全物理改造/stability'
$ubaDirectory = Join-Path $testDirectory 'uba'
$logFile = Join-Path $testDirectory "physics_level_$Tag.log"

foreach ($requiredPath in @($projectFile, $buildScript, $editorExecutable, $sourceMap)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required full-level physics probe input not found: $requiredPath"
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

$angleArg = $Angle.ToString([Globalization.CultureInfo]::InvariantCulture)
$powerArg = $Power.ToString([Globalization.CultureInfo]::InvariantCulture)
Remove-Item -LiteralPath $logFile -Force -ErrorAction SilentlyContinue
$editorArguments = @(
    $projectFile, $sourceMap, '-game', '-PIEVIACONSOLE', '-Multiprocess',
    '-unattended', '-nop4', '-nosplash', '-NoSound', '-stdout', '-log',
    '-NaiwaP2PhysicsLevelAutoTest',
    "-NaiwaP2Level=$Level",
    "-NaiwaP2ImpactAngle=$angleArg", "-NaiwaP2ImpactPower=$powerArg",
    "-NaiwaP2StructureMassMultiplier=$($StructureMassMultiplier.ToString([Globalization.CultureInfo]::InvariantCulture))",
    "-abslog=$logFile"
)
if ($FrameTimeProbe) {
    if ($SkillProbe -or $ResetAfterShot -or $RefundZeroKillShot -or $MultiShotTest) {
        throw 'Choose FrameTimeProbe by itself; it measures one regular full-physics shot.'
    }
    $frameExecCommands = "r.ScreenPercentage $FrameScreenPercentage"
    $windowMode = if ($FullscreenProbe) { '-Fullscreen' } else { '-Windowed' }
    $editorArguments += @(
        $windowMode, '-novsync', '-ResX=1920', '-ResY=1080', "-r.ScreenPercentage=$FrameScreenPercentage",
        "-ExecCmds=`"$frameExecCommands`"", '-NaiwaP2PhysicsFrameTimeProbe'
    )
} else {
    if ($FullscreenProbe) {
        throw '-FullscreenProbe requires -FrameTimeProbe.'
    }
    $editorArguments += '-NullRHI'
}
if ($TraceFile) {
    if (-not [IO.Path]::IsPathRooted($TraceFile)) {
        $TraceFile = Join-Path $projectRoot $TraceFile
    }
    $traceDirectory = Split-Path -Parent $TraceFile
    New-Item -ItemType Directory -Path $traceDirectory -Force | Out-Null
    $editorArguments += @('-trace=default', "-tracefile=$TraceFile")
}
if ($SkillProbe) {
    if ($ResetAfterShot -or $RefundZeroKillShot -or $MultiShotTest) {
        throw 'Choose either -SkillProbe or one of the reset/refund/multi-shot tests.'
    }
    $editorArguments += "-NaiwaP2PhysicsLevelSkillProbe=$SkillProbe"
}
if (@([bool]$ResetAfterShot, [bool]$RefundZeroKillShot, [bool]$MultiShotTest).Where({ $_ }).Count -gt 1) {
    throw 'Choose only one of -ResetAfterShot, -RefundZeroKillShot, or -MultiShotTest.'
}
if ($ResetAfterShot) {
    $editorArguments += '-NaiwaP2PhysicsLevelResetTest'
}
if ($RefundZeroKillShot) {
    if ($Level -eq '2-3') {
        throw '2-3 has no free retry token, so the zero-kill refund test does not apply.'
    }
    $editorArguments += '-NaiwaP2PhysicsLevelRefundTest'
}
if ($MultiShotTest) {
    $angleParts = @($SequenceAngles.Split('+'))
    $expectedShotCount = switch ($Level) {
        '2-2' { 5 }
        '2-3' { 4 }
        default { 3 }
    }
    if ($angleParts.Count -ne $expectedShotCount) {
        throw "-SequenceAngles requires exactly $expectedShotCount plus-separated values for level $Level."
    }
    foreach ($anglePart in $angleParts) {
        $parsedAngle = 0.0
        if (-not [double]::TryParse($anglePart, [Globalization.NumberStyles]::Float,
                [Globalization.CultureInfo]::InvariantCulture, [ref]$parsedAngle) -or
            $parsedAngle -lt 1.0 -or $parsedAngle -gt 89.0) {
            throw "Invalid sequence angle: $anglePart"
        }
    }
    $editorArguments += '-NaiwaP2PhysicsLevelMultiShotTest'
    $editorArguments += "-NaiwaP2PhysicsLevelAngles=$SequenceAngles"
}
$windowStyle = if ($VisibleWindow) { 'Normal' } else { 'Hidden' }
$process = Start-Process -FilePath $editorExecutable -ArgumentList $editorArguments `
    -WorkingDirectory (Split-Path -Parent $editorExecutable) -WindowStyle $windowStyle -PassThru -Wait

$output = if (Test-Path -LiteralPath $logFile) { Get-Content -LiteralPath $logFile -Raw } else { '' }
$summaryPattern = if ($ResetAfterShot) {
    'NAIWA_P2_PHYS_LEVEL_RESET_TEST [^\r\n]*\bpass=\d[^\r\n]*'
} elseif ($RefundZeroKillShot) {
    'NAIWA_P2_PHYS_LEVEL_REFUND_TEST [^\r\n]*\bpass=\d[^\r\n]*'
} elseif ($MultiShotTest) {
    'NAIWA_P2_PHYS_LEVEL_MULTI_SHOT_TEST [^\r\n]*\bpass=\d[^\r\n]*'
} elseif ($SkillProbe) {
    'NAIWA_P2_PHYS_SKILL_PROBE [^\r\n]*\bpass=\d[^\r\n]*'
} else {
    'NAIWA_P2_PHYS_LEVEL_TEST [^\r\n]*\bpass=\d[^\r\n]*'
}
$summary = [regex]::Matches($output, $summaryPattern) | Select-Object -Last 1
$frameSummary = [regex]::Matches($output, 'NAIWA_P2_FRAME_TIME_PROBE pass=\d[^\r\n]*') | Select-Object -Last 1
$results = [regex]::Matches($output, 'NAIWA_P2_PHYS_(?:GOAL_HIT|PROJECTILE_END|LEVEL_BLOCK|LEVEL_SHOT_RESOLVE|BLOCK_OUT_OF_BOUNDS|GOAL_OUT_OF_BOUNDS) [^\r\n]*') |
    ForEach-Object { $_.Value }
$hits = [regex]::Matches($output, 'NAIWA_P2_PHYS_EGG_HIT [^\r\n]*') | ForEach-Object { $_.Value }
$penetrations = [regex]::Matches($output, 'NAIWA_P2_PHYS_PENETRATION [^\r\n]*') | ForEach-Object { $_.Value }

$sequenceText = if ($MultiShotTest) { " sequenceAngles=$SequenceAngles" } else { '' }
$massScaleText = " structureMassMultiplier=$($StructureMassMultiplier.ToString([Globalization.CultureInfo]::InvariantCulture))"
Write-Output "level=$Level map=$map angle=$angleArg power=$powerArg$sequenceText$massScaleText exit=$($process.ExitCode) log=$logFile"
foreach ($hit in $hits) { Write-Output "  $hit" }
foreach ($result in $results) { Write-Output "  $result" }
Write-Output "  penetration_events=$($penetrations.Count)"
if (-not $summary) {
    throw "Full-level physics result missing; the hidden runtime did not complete. Process exit=$($process.ExitCode), log=$logFile"
}
Write-Output "  $($summary[0].Value)"
if ($FrameTimeProbe) {
    if (-not $frameSummary) {
        throw "Frame-time probe result missing; the runtime did not complete the measurement. Process exit=$($process.ExitCode), log=$logFile"
    }
    Write-Output "  $($frameSummary[0].Value)"
}
if ($process.ExitCode -ne 0) { exit $process.ExitCode }
if ($penetrations.Count -gt 0) { exit 1 }
if ($FrameTimeProbe) {
    if ($summary[0].Value -notmatch '(?:^|\s)physics_pass=1(?:\s|$)') { exit 1 }
    if ($frameSummary[0].Value -notmatch '(?:^|\s)pass=1(?:\s|$)') { exit 1 }
} elseif ($summary[0].Value -notmatch '(?:^|\s)pass=1(?:\s|$)') {
    exit 1
}
exit 0
