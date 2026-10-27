[CmdletBinding()]
param(
    [ValidateSet('SelfTest', 'Play')]
    [string]$Mode = 'SelfTest',
    [switch]$SkipBuild,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$buildScript = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
$editorCommand = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor.exe'
$materialAsset = Join-Path $PSScriptRoot 'NaiwaP0/Content/Naiwa/P3/Materials/M_P3M1FlatColor.uasset'
$mapAsset = Join-Path $PSScriptRoot 'NaiwaP0/Content/Naiwa/P3/Maps/L_NaiwaP3_M1Prototype.umap'
$materialScript = Join-Path $PSScriptRoot 'Scripts/p3_create_m1_flat_material.py'
$mapScript = Join-Path $PSScriptRoot 'Scripts/p3_build_m1_map.py'
$mapPath = '/Game/Naiwa/P3/Maps/L_NaiwaP3_M1Prototype'
$recordDirectory = Join-Path $PSScriptRoot 'Records/p3_m1'
$workDirectory = Join-Path $repoRoot '.workbuddy/work/20261026_P3方案/M1'
$ubaDirectory = Join-Path $workDirectory 'uba'
$mapReport = Join-Path $workDirectory 'map_build_report.json'
$mapLog = Join-Path $workDirectory 'map_build.log'
$materialReport = Join-Path $workDirectory 'material_build_report.json'
$materialLog = Join-Path $workDirectory 'material_build.log'

foreach ($requiredPath in @($projectFile, $buildScript, $editorCommand, $editor, $materialScript, $mapScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Required P3 M1 input not found: $requiredPath" }
}
New-Item -ItemType Directory -Path $workDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ubaDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $recordDirectory -Force | Out-Null

if (-not $SkipBuild) {
    Write-Output 'Building Nai_LongEditor Win64 Development...'
    & $buildScript Nai_LongEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=1 "-UBARootDir=$ubaDirectory" -Verbose
    if ($LASTEXITCODE -ne 0) { throw "P3 M1 editor build failed with exit code $LASTEXITCODE" }

    Write-Output 'Building Nai_Long Win64 Development...'
    & $buildScript Nai_Long Win64 Development "-Project=$projectFile" -WaitMutex -NoUBA -MaxParallelActions=1 "-UBARootDir=$ubaDirectory" -Verbose
    if ($LASTEXITCODE -ne 0) { throw "P3 M1 game build failed with exit code $LASTEXITCODE" }
}

$reuseExistingAssets = (Test-Path -LiteralPath $materialAsset) -and
    (Test-Path -LiteralPath $mapAsset) -and
    (Test-Path -LiteralPath $mapReport)

if ($reuseExistingAssets) {
    $mapResult = Get-Content -LiteralPath $mapReport -Raw | ConvertFrom-Json
    if ($mapResult.status -ne 'existing_owned_map_validated' -or
        $mapResult.default_game_mode -ne '/Script/NaiwaRuntime.NaiwaP3M1GameMode') {
        throw "P3 M1 existing map report is not valid; inspect $mapReport"
    }
    Write-Output "Using existing P3 M1 assets: $mapPath"
} else {
    Remove-Item -LiteralPath $materialReport -Force -ErrorAction SilentlyContinue
    & $editorCommand $projectFile -run=PythonScript "-script=$materialScript" -unattended -nop4 -nosplash -NullRHI "-abslog=$materialLog"
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $materialReport)) {
        throw "P3 M1 material setup failed; inspect $materialLog and $materialReport"
    }
    $materialResult = Get-Content -LiteralPath $materialReport -Raw | ConvertFrom-Json
    if ($materialResult.status -ne 'created_or_validated') {
        throw "P3 M1 material validation failed: $($materialResult.status)"
    }

    Remove-Item -LiteralPath $mapReport -Force -ErrorAction SilentlyContinue
    & $editorCommand $projectFile -run=PythonScript "-script=$mapScript" -unattended -nop4 -nosplash -NullRHI "-abslog=$mapLog"
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $mapReport)) {
        throw "P3 M1 map creation failed; inspect $mapLog and $mapReport"
    }
    $mapResult = Get-Content -LiteralPath $mapReport -Raw | ConvertFrom-Json
    if ($mapResult.status -notin @('new_isolated_map_saved', 'existing_owned_map_validated')) {
        throw "P3 M1 map validation failed: $($mapResult.status)"
    }
}

if ($Mode -eq 'Play') {
    $gameArguments = @($projectFile, $mapPath, '-game', '-windowed', '-ResX=1600', '-ResY=1000', '-ForceRes', '-nop4', '-nosplash')
    Start-Process -FilePath $editor -ArgumentList $gameArguments -WorkingDirectory (Split-Path -Parent $projectFile) -WindowStyle Normal | Out-Null
    Write-Output "P3 M1 launched: $mapPath"
    exit 0
}

$selfTestLog = Join-Path $recordDirectory 'm1_selftest.log'
$selfTestResult = Join-Path $recordDirectory 'm1_selftest_result.json'
Remove-Item -LiteralPath $selfTestLog,$selfTestResult -Force -ErrorAction SilentlyContinue
$gameArguments = @($projectFile, $mapPath, '-game', '-NullRHI', '-unattended', '-nop4', '-nosplash', '-NaiwaP3M1SelfTest', "-abslog=$selfTestLog")
$process = Start-Process -FilePath $editorCommand -ArgumentList $gameArguments -WindowStyle Hidden -PassThru -Wait
$logText = if (Test-Path -LiteralPath $selfTestLog) { Get-Content -LiteralPath $selfTestLog -Raw } else { '' }
$passMatch = [regex]::Match($logText, 'NAIWA_P3_M1_PASS ([^\r\n]+)')
$failMatch = [regex]::Match($logText, 'NAIWA_P3_M1_FAIL ([^\r\n]+)')
$result = @{
    verdict = $(if ($passMatch.Success -and $process.ExitCode -eq 0) { 'PASS' } else { 'FAIL' })
    map = $mapPath
    engine = $mapResult.engine
    details = $(if ($passMatch.Success) { $passMatch.Groups[1].Value } elseif ($failMatch.Success) { $failMatch.Groups[1].Value } else { 'No P3 M1 self-test marker in log' })
    exit_code = $process.ExitCode
    log = $selfTestLog
}
$result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $selfTestResult -Encoding utf8
if ($result.verdict -ne 'PASS') { throw "P3 M1 self-test failed; inspect $selfTestLog and $selfTestResult" }
Write-Output "P3 M1 self-test PASS: $($result.details)"
Write-Output "Report: $selfTestResult"
