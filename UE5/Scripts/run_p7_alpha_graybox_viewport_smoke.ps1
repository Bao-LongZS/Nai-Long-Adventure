[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$StageDirectory,
    [ValidateRange(640, 7680)]
    [int]$Width = 1280,
    [ValidateRange(480, 4320)]
    [int]$Height = 720,
    [int]$TimeoutSeconds = 45,
    [switch]$Fullscreen,
    [string]$MapName = '/Game/Naiwa/P7/Maps/L_P7_Alpha01_Graybox'
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$StageRoot = (Resolve-Path -LiteralPath $StageDirectory).Path
$WindowsRoot = if ((Split-Path -Leaf $StageRoot) -eq 'Windows') { $StageRoot } else { Join-Path $StageRoot 'Windows' }
$Executable = Join-Path $WindowsRoot 'Nai_Long.exe'
$ContentPak = Join-Path $WindowsRoot 'Nai_Long\Content\Paks\Nai_Long-Windows.pak'
$EvidenceRoot = Join-Path $RepositoryRoot 'UE5\Records\p7_td\evidence'
$Timestamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$EngineLog = Join-Path $EvidenceRoot "P7_AlphaGrayboxViewport_${Width}x${Height}_$Timestamp.log"
$StdoutLog = Join-Path $EvidenceRoot "P7_AlphaGrayboxViewport_${Width}x${Height}_$Timestamp.stdout.log"
$StderrLog = Join-Path $EvidenceRoot "P7_AlphaGrayboxViewport_${Width}x${Height}_$Timestamp.stderr.log"
$ReportPath = Join-Path $EvidenceRoot "P7_AlphaGrayboxViewport_${Width}x${Height}_$Timestamp.json"
$ScreenshotPath = Join-Path $EvidenceRoot "P7_AlphaGraybox_${Width}x${Height}_$Timestamp.png"
$ScreenshotDirectory = Join-Path $WindowsRoot 'Nai_Long\Saved\Screenshots\Windows'
$WindowMode = if ($Fullscreen) { '-fullscreen' } else { '-windowed' }
$ExecCommands = if ($Fullscreen) {
    "r.SetRes ${Width}x${Height}f,HighResShot ${Width}x${Height}"
} else {
    "HighResShot ${Width}x${Height}"
}

foreach ($path in @($Executable, $ContentPak)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required cooked P7 viewport input was not found: $path"
    }
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
$StartedAt = Get-Date
$ArgumentLine = "-Map=$MapName $WindowMode -ResX=$Width -ResY=$Height -ExecCmds=`"$ExecCommands`" -stdout -FullStdOutLogOutput -AbsLog=`"$EngineLog`""
$Process = Start-Process -FilePath $Executable -ArgumentList $ArgumentLine `
    -WorkingDirectory $WindowsRoot -PassThru `
    -RedirectStandardOutput $StdoutLog -RedirectStandardError $StderrLog
$Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
$ScreenshotWasLogged = $false
$ScreenshotFileReady = $false
do {
    $Process.Refresh()
    $CurrentLogText = if (Test-Path -LiteralPath $EngineLog -PathType Leaf) { Get-Content -LiteralPath $EngineLog -Raw } else { '' }
    $ScreenshotWasLogged = $CurrentLogText -match 'High resolution screenshot saved as'
    $LatestCandidate = Get-ChildItem -LiteralPath $ScreenshotDirectory -Filter 'HighresScreenshot*.png' -File -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -ge $StartedAt.AddSeconds(-1) } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    $ScreenshotFileReady = $null -ne $LatestCandidate -and $LatestCandidate.Length -gt 1024
    if ($ScreenshotWasLogged -or $ScreenshotFileReady -or $Process.HasExited -or (Get-Date) -ge $Deadline) {
        break
    }
    Start-Sleep -Milliseconds 500
} while ($true)

if (($ScreenshotWasLogged -or $ScreenshotFileReady) -and -not $Process.HasExited) {
    # This smoke validates a real-RHI frame capture. Process lifecycle is covered
    # by the separate cooked-startup smoke, so close this isolated viewport only
    # after the screenshot has been written.
    Start-Sleep -Seconds 2
    $Process.Refresh()
}
$Finished = $Process.HasExited
if (-not $Finished) {
    Stop-Process -InputObject $Process -Force -ErrorAction SilentlyContinue
    $Process.WaitForExit()
}
$Process.Refresh()
$ExitCode = if ($Finished) { $Process.ExitCode } else { $null }

$LogText = if (Test-Path -LiteralPath $EngineLog -PathType Leaf) { Get-Content -LiteralPath $EngineLog -Raw } else { '' }
$CandidateScreenshot = Get-ChildItem -LiteralPath $ScreenshotDirectory -Filter 'HighresScreenshot*.png' -File -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTime -ge $StartedAt.AddSeconds(-1) } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if ($CandidateScreenshot) {
    Copy-Item -LiteralPath $CandidateScreenshot.FullName -Destination $ScreenshotPath -Force
}

$ImageWidth = $null
$ImageHeight = $null
if (Test-Path -LiteralPath $ScreenshotPath -PathType Leaf) {
    Add-Type -AssemblyName System.Drawing
    $Bitmap = [System.Drawing.Bitmap]::FromFile($ScreenshotPath)
    try {
        $ImageWidth = $Bitmap.Width
        $ImageHeight = $Bitmap.Height
    } finally {
        $Bitmap.Dispose()
    }
}

$LoadedWorld = $LogText -match ('(?m)LogWorld: Bringing World .*' + [regex]::Escape($MapName))
$FixtureLoaded = $LogText -match 'P7 graybox loaded level P7_Alpha01 from five configured DataTables\.'
$EngineInitialized = $LogText -match 'LogInit: Display: Engine is initialized\. Leaving FEngineLoop::Init\(\)'
$ScreenshotLogged = $LogText -match 'High resolution screenshot saved as'
$ScreenshotFileWritten = Test-Path -LiteralPath $ScreenshotPath -PathType Leaf
$CleanExit = $LogText -match '(?m)LogExit: Exiting\.'
$DimensionsMatch = $ImageWidth -eq $Width -and $ImageHeight -eq $Height
$Passed = $LoadedWorld -and $FixtureLoaded -and $EngineInitialized -and
    $ScreenshotFileWritten -and $DimensionsMatch

$Report = [ordered]@{
    test = "Cooked P7 real-RHI viewport screenshot ${Width}x${Height}"
    status = if ($Passed) { 'PASS_RENDER_CAPTURE' } else { 'FAIL' }
    startedAt = $StartedAt.ToString('yyyy-MM-dd HH:mm:ssK')
    processExitCode = $ExitCode
    renderPath = 'Direct3D 12 / PCD3D_SM6 expected; see engine log'
    windowMode = if ($Fullscreen) { 'fullscreen' } else { 'windowed' }
    map = $MapName
    targetMapReportedLoaded = $LoadedWorld
    p7FixtureLoaded = $FixtureLoaded
    engineInitialized = $EngineInitialized
    screenshotLogged = $ScreenshotLogged
    screenshotFileWritten = $ScreenshotFileWritten
    requestedWidth = $Width
    requestedHeight = $Height
    actualWidth = $ImageWidth
    actualHeight = $ImageHeight
    cleanExitReported = $CleanExit
    processClosedByHarness = (-not $CleanExit -and -not $Finished)
    screenshot = if (Test-Path -LiteralPath $ScreenshotPath) { $ScreenshotPath } else { $null }
    engineLog = $EngineLog
    stdoutLog = $StdoutLog
    stderrLog = $StderrLog
}
$Report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ReportPath -Encoding utf8

[pscustomobject]@{
    Status = $Report.status
    ProcessExitCode = $ExitCode
    MapLoaded = $LoadedWorld
    P7FixtureLoaded = $FixtureLoaded
    Screenshot = $Report.screenshot
    Dimensions = if ($ImageWidth) { "${ImageWidth}x${ImageHeight}" } else { $null }
    Report = $ReportPath
    EngineLog = $EngineLog
} | Format-List

if (-not $Passed) {
    throw "P7 Alpha viewport screenshot smoke failed. See $ReportPath and $EngineLog"
}
