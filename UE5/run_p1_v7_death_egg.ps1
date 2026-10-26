[CmdletBinding()]
param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')

$ErrorActionPreference = 'Stop'
$projectRoot = Join-Path $PSScriptRoot 'NaiwaP0'
$projectFile = Join-Path $projectRoot 'Nai_Long.uproject'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$mapPackage = '/Game/Naiwa/P1/Maps/L_NaiwaP1_v7'
$savedDirectory = Join-Path $projectRoot 'Saved/NaiwaP1'
$configFile = Join-Path $PSScriptRoot 'Scripts/p1_integrated_v7_config.json'
$mapFile = Join-Path $projectRoot 'Content/Naiwa/P1/Maps/L_NaiwaP1_v7.umap'
$expectedConfigHash = '4fd6914655c2ae7c28567bbbdb808b758b0686e7b9bd260a145891d322aa2e59'
$expectedMapHash = '4a55ced57ddbe47044aa7f4965fc4013cc82ccc48976a9819a6217a4dbef5754'

foreach ($file in @($projectFile, $editor, $configFile, $mapFile)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Required V7 runtime file is missing: $file" }
}
if ((Get-FileHash -LiteralPath $configFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedConfigHash -or
    (Get-FileHash -LiteralPath $mapFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedMapHash) {
    throw 'The saved V7 map or config identity changed; refusing to run this death-egg probe.'
}

$existingReports = @(Get-ChildItem -LiteralPath $savedDirectory -Filter 'v7_hidden_final_pit_recovery_*.json' -File -ErrorAction SilentlyContinue |
    ForEach-Object FullName)
$started = [DateTime]::UtcNow
$stamp = $started.ToString('yyyyMMddTHHmmssZ') + '_' + [Guid]::NewGuid().ToString('N')
$logFile = Join-Path $savedDirectory "v7_death_egg_$stamp.log"
$arguments = @($projectFile, $mapPackage, '-game', '-RenderOffScreen', '-windowed', '-ResX=1600', '-ResY=900',
    '-ForceRes', '-unattended', '-nop4', '-nosplash', '-NaiwaP1Standalone', '-NaiwaP1V7DeathEggProbe',
    '-ExecCmds=t.MaxFPS 60', "-abslog=$logFile")
& $editor @arguments
if ($LASTEXITCODE -ne 0) { throw "V7 third-death probe exited $LASTEXITCODE; inspect $logFile" }

$freshReports = @(Get-ChildItem -LiteralPath $savedDirectory -Filter 'v7_hidden_final_pit_recovery_*.json' -File |
    Where-Object { $_.FullName -notin $existingReports -and $_.LastWriteTimeUtc -ge $started })
if ($freshReports.Count -ne 1) { throw "Expected one fresh death-egg report, found $($freshReports.Count); inspect $logFile" }
$reportFile = $freshReports[0].FullName
$report = Get-Content -LiteralPath $reportFile -Raw -Encoding UTF8 | ConvertFrom-Json
if ($report.schema -ne 'NaiwaP1V7HiddenFinalPitRecoveryV1' -or $report.status -ne 'PASS' -or
    $report.outcome -ne 'PASS' -or $report.map -ne 'L_NaiwaP1_v7' -or
    $report.map_sha256 -ne $expectedMapHash -or $report.config_sha256 -ne $expectedConfigHash -or
    $report.death_events -ne 1 -or $report.deaths_this_run -ne 4 -or
    $report.death_cause -ne 'CompoundHiddenPit04' -or
    $report.active_checkpoint_after_respawn -ne 'IntegratedCheckpoint08' -or
    $report.respawned_at_expected_checkpoint -ne $true -or
    $report.terminal_mechanism_reset_after_death -ne $true -or
    $report.death_egg_followup_selected -ne $true -or
    $report.death_egg_background_music_paused -ne $true -or
    $report.death_egg_background_music_resumed -ne $true -or
    $report.death_egg_returned_to_die1 -ne $true) {
    throw "V7 third-death report failed: $reportFile"
}

$logText = Get-Content -LiteralPath $logFile -Raw -Encoding UTF8
$leadCount = ([regex]::Matches($logText, 'event=death_lead sound=SFX_P1_Feedback_Die count=')).Count
$regularFollowups = ([regex]::Matches($logText, 'event=death_followup sound=SFX_P1_Feedback_Die1')).Count
$eggFollowups = ([regex]::Matches($logText, 'event=death_followup sound=SFX_P1_Feedback_Die3')).Count
$pauseCount = ([regex]::Matches($logText, 'event=background_music_paused_for_egg')).Count
$resumeCount = ([regex]::Matches($logText, 'event=background_music_resumed')).Count
if ($leadCount -ne 4 -or $regularFollowups -ne 3 -or $eggFollowups -ne 1 -or
    $pauseCount -ne 1 -or $resumeCount -ne 1 -or
    $logText -notmatch 'NAIWA_P1_V7_DEATH_EGG third_death_entered total=3') {
    throw "Death-egg sequence mismatch: Die=$leadCount Die_1=$regularFollowups Die_3=$eggFollowups paused=$pauseCount resumed=$resumeCount; inspect $logFile"
}

[pscustomobject]@{ status = 'PASS'; report = $reportFile; log = $logFile; die = $leadCount;
    die1 = $regularFollowups; die3 = $eggFollowups; music_paused = $pauseCount; music_resumed = $resumeCount; returned_to_die1 = $report.death_egg_returned_to_die1 } |
    Format-List
