[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Build', 'CameraAudit', 'Slice')]
    [string]$Stage,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$savedDirectory = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1'
New-Item -ItemType Directory -Path $savedDirectory -Force | Out-Null

if ($Stage -eq 'Build') {
    $buildScript = Join-Path $Engine 'Engine/Build/BatchFiles/Build.bat'
    if (-not (Test-Path -LiteralPath $buildScript)) { throw "UE build tool not found: $buildScript" }
    & $buildScript Nai_LongEditor Win64 Development "-Project=$projectFile" -WaitMutex
    if ($LASTEXITCODE -ne 0) { throw "P1 native build failed with exit code $LASTEXITCODE" }
    return
}

$editorCommand = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editorCommand)) { throw "UE command editor not found: $editorCommand" }
if ($Stage -eq 'CameraAudit') {
    $scriptFile = Join-Path $PSScriptRoot 'Scripts/audit_p1_camera.py'
    $reportFile = Join-Path $savedDirectory 'camera_audit.json'
} else {
    $scriptFile = Join-Path $PSScriptRoot 'Scripts/build_p1_slice.py'
    $reportFile = Join-Path $savedDirectory 'slice_build.json'
}
if (-not (Test-Path -LiteralPath $scriptFile)) { throw "P1 script missing: $scriptFile" }

$startedUtc = [DateTime]::UtcNow
$logFile = Join-Path $savedDirectory (($Stage.ToLowerInvariant()) + '.log')
$arguments = @($projectFile, '-run=PythonScript', "-script=$scriptFile", '-unattended', '-nop4', '-nosplash', '-NullRHI', "-abslog=$logFile")
& $editorCommand @arguments
if ($LASTEXITCODE -ne 0) { throw "P1 $Stage failed with exit code $LASTEXITCODE; inspect $logFile" }
if (-not (Test-Path -LiteralPath $reportFile)) {
    throw "P1 $Stage exited without its report: $reportFile. The Python script may not have run."
}
$reportItem = Get-Item -LiteralPath $reportFile
if ($reportItem.LastWriteTimeUtc -lt $startedUtc) {
    throw "P1 $Stage left a stale report: $reportFile. Check UE startup and $logFile."
}
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
$expected = if ($Stage -eq 'Slice') { 'saved_candidate_pending_playable_camera_and_walk_run_review' } else { 'read_only_complete' }
if ($report.status -ne $expected) {
    throw "P1 $Stage report status '$($report.status)' differs from '$expected'; inspect $reportFile."
}
Write-Output "P1 $Stage completed; report: $reportFile"
