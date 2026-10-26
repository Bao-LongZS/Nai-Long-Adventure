[CmdletBinding()]
param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.8')

$ErrorActionPreference = 'Stop'
$projectRoot = Join-Path $PSScriptRoot 'NaiwaP0'
$projectFile = Join-Path $projectRoot 'Nai_Long.uproject'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$mapPackage = '/Game/Naiwa/P1/Maps/L_NaiwaP1_v7'
$savedDirectory = Join-Path $projectRoot 'Saved/NaiwaP1'
$reportPattern = 'v7_hidden_final_pit_recovery_*.json'
$configFile = Join-Path $PSScriptRoot 'Scripts/p1_integrated_v7_config.json'
$mapFile = Join-Path $projectRoot 'Content/Naiwa/P1/Maps/L_NaiwaP1_v7.umap'
$expectedConfigHash = '4fd6914655c2ae7c28567bbbdb808b758b0686e7b9bd260a145891d322aa2e59'
$expectedMapHash = '4a55ced57ddbe47044aa7f4965fc4013cc82ccc48976a9819a6217a4dbef5754'

foreach ($file in @($projectFile, $editor, $configFile, $mapFile)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Required V7 runtime file is missing: $file" }
}
if ((Get-FileHash -LiteralPath $configFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedConfigHash -or
    (Get-FileHash -LiteralPath $mapFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedMapHash) {
    throw 'The saved V7 map or config identity changed; refusing to run this targeted recovery probe.'
}

$existingReports = @(Get-ChildItem -LiteralPath $savedDirectory -Filter $reportPattern -File -ErrorAction SilentlyContinue |
    ForEach-Object FullName)
$started = [DateTime]::UtcNow
$stamp = $started.ToString('yyyyMMddTHHmmssZ') + '_' + [Guid]::NewGuid().ToString('N')
$logFile = Join-Path $savedDirectory "v7_hidden_final_pit_recovery_$stamp.log"
$arguments = @($projectFile, $mapPackage, '-game', '-RenderOffScreen', '-windowed', '-ResX=1600', '-ResY=900',
    '-ForceRes', '-unattended', '-nop4', '-nosplash', '-NaiwaP1Standalone', '-NaiwaP1V7HiddenFinalPitRecoveryProbe',
    '-ExecCmds=t.MaxFPS 60', "-abslog=$logFile")
& $editor @arguments
if ($LASTEXITCODE -ne 0) { throw "V7 hidden-pit runtime exited $LASTEXITCODE; inspect $logFile" }

$freshReports = @(Get-ChildItem -LiteralPath $savedDirectory -Filter $reportPattern -File |
    Where-Object { $_.FullName -notin $existingReports -and $_.LastWriteTimeUtc -ge $started })
if ($freshReports.Count -ne 1) { throw "Expected one fresh terminal-pit recovery report, found $($freshReports.Count); inspect $logFile" }
$reportFile = $freshReports[0].FullName
$report = Get-Content -LiteralPath $reportFile -Raw -Encoding UTF8 | ConvertFrom-Json
if ($report.schema -ne 'NaiwaP1V7HiddenFinalPitRecoveryV1' -or $report.status -ne 'PASS' -or
    $report.outcome -ne 'PASS' -or $report.map -ne 'L_NaiwaP1_v7' -or
    $report.map_package -ne $mapPackage -or $report.map_sha256 -ne $expectedMapHash -or
    $report.config_sha256 -ne $expectedConfigHash -or $report.exact_map_and_config_identity -ne $true -or
    $report.natural_core_route_reached_terminal_compound -ne $true -or $report.checkpoint_count_before_death -ne 8 -or
    $report.death_cause -ne 'CompoundHiddenPit04' -or $report.production_dying_state_observed -ne $true -or
    $report.hidden_pits_visible_during_death -ne $true -or $report.terminal_timer_expired_before_death -ne $true -or
    $report.death_events -ne 1 -or $report.active_checkpoint_after_respawn -ne 'IntegratedCheckpoint08' -or
    $report.respawned_at_expected_checkpoint -ne $true -or $report.grounded_on_configured_safe_bank -ne $true -or
    $report.controls_restored_and_death_pose_cleared -ne $true -or $report.terminal_mechanism_reset_after_death -ne $true -or
    $report.both_terminal_compounds_reset_after_death -ne $true -or
    $report.terminal_timer_trigger_count_delta -ne 1 -or $report.terminal_timer_disable_count_delta -ne 1 -or
    $report.probe_character_teleports -ne 0 -or $report.direct_trigger_injections -ne 0 -or
    $report.platform_control_injections -ne 0 -or $report.production_respawn_placements -ne 1) {
    throw "V7 terminal hidden-pit recovery report failed: $reportFile"
}

[pscustomobject]@{ status = $report.status; report = $reportFile; log = $logFile; detail = $report.detail } |
    Format-List
