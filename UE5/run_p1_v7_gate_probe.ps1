[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][ValidateSet('A', 'B', 'C')][string]$Gate,
    [Parameter(Mandatory = $true)][ValidateSet('Walk', 'Run')][string]$Gait,
    [ValidateRange(-80, 80)][double]$OffsetCm = 0,
    [switch]$Recovery,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$project = Join-Path $PSScriptRoot 'NaiwaP0/Nai_Long.uproject'
$editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$mapPackage = '/Game/Naiwa/P1/Maps/L_NaiwaP1_v7_GateProbe'
$mapFile = Join-Path $PSScriptRoot 'NaiwaP0/Content/Naiwa/P1/Maps/L_NaiwaP1_v7_GateProbe.umap'
$buildReportFile = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1/v7_gate_probe_build_v2.json'
$saved = Join-Path $PSScriptRoot 'NaiwaP0/Saved/NaiwaP1'
foreach ($path in @($project, $editor, $mapFile, $buildReportFile)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing: $path" }
}
$build = Get-Content -LiteralPath $buildReportFile -Raw -Encoding UTF8 | ConvertFrom-Json
$mapHash = (Get-FileHash -LiteralPath $mapFile -Algorithm SHA256).Hash.ToLowerInvariant()
if ($build.schema -ne 'NaiwaP1V7GateProbeBuildV2' -or
    $build.status -ne 'saved_isolated_fixture_pending_runtime' -or
    $build.actor_count -ne 18 -or $build.isolated_only -ne $true -or
    $build.checkpoint_ids.A -ne 'IntegratedCheckpoint02' -or
    $build.checkpoint_ids.B -ne 'IntegratedCheckpoint04' -or
    $build.checkpoint_ids.C -ne 'IntegratedCheckpoint06' -or
    $build.map -ne $mapPackage -or $build.map_sha256 -ne $mapHash) {
    throw 'Isolated fixture identity/hash drift; rebuild or review before measuring'
}
if ($Recovery -and $OffsetCm -ne 0) { throw 'Recovery mode requires -OffsetCm 0.' }
$namePattern = if ($Recovery) { "v7_gate_recovery_${Gate}_*.json" } else { "v7_gate_probe_v2_${Gate}_*.json" }
$existing = @(Get-ChildItem -LiteralPath $saved -Filter $namePattern -File | ForEach-Object FullName)
$started = [DateTime]::UtcNow
$stamp = $started.ToString('yyyyMMddTHHmmssZ') + '_' + [Guid]::NewGuid().ToString('N')
$modeName = if ($Recovery) { 'recovery' } else { 'baseline' }
$log = Join-Path $saved ("v7_gate_${modeName}_${Gate}_${Gait}_${stamp}.log")
$flag = "-NaiwaP1V7GateProbe=$Gate$Gait"
$arguments = @($project, $mapPackage, '-game', '-RenderOffScreen', '-windowed', '-ResX=1600', '-ResY=900',
               '-ForceRes', '-unattended', '-nop4', '-nosplash', '-NaiwaP1Standalone', $flag,
               "-NaiwaP1V7GateOffsetCm=$OffsetCm", '-ExecCmds=t.MaxFPS 60', "-abslog=$log")
if ($Recovery) { $arguments += '-NaiwaP1V7GateRecovery' }
& $editor @arguments
if ($LASTEXITCODE -ne 0) { throw "Gate probe exited $LASTEXITCODE; inspect $log" }
$fresh = @(Get-ChildItem -LiteralPath $saved -Filter $namePattern -File |
    Where-Object { $_.FullName -notin $existing -and $_.LastWriteTimeUtc -ge $started })
if ($fresh.Count -ne 1) { throw "Expected exactly one fresh gate report, found $($fresh.Count); inspect $log" }
$report = Get-Content -LiteralPath $fresh[0].FullName -Raw -Encoding UTF8 | ConvertFrom-Json
$expectedGait = if ($Gait -eq 'Run') { 'Run300' } else { 'Walk150' }
$expectedSchema = if ($Recovery) { 'NaiwaP1V7GateRecoveryV1' } else { 'NaiwaP1V7GateProbeV2' }
$expectedStatus = if ($Recovery) { 'PASS' } else { 'measured' }
if ($report.schema -ne $expectedSchema -or $report.status -ne $expectedStatus -or
    $report.map -ne 'L_NaiwaP1_v7_GateProbe' -or $report.gate -ne $Gate -or
    $report.gait -ne $expectedGait -or $report.isolated_only -ne $true -or
    $report.jump_offset_cm -ne $OffsetCm -or $report.boarded_real_shuttle_support -ne $true -or
    $report.checkpoint_activated_by_native_overlap -ne $true) {
    throw "Gate report failed identity/physical boarding preflight: $($fresh[0].FullName)"
}
if ($Recovery) {
    $expectedCheckpoint = switch ($Gate) {
        'A' { 'IntegratedCheckpoint02' }
        'B' { 'IntegratedCheckpoint04' }
        'C' { 'IntegratedCheckpoint06' }
    }
    $recoveryPassed = $report.schema -eq 'NaiwaP1V7GateRecoveryV1' -and
        $report.status -eq 'PASS' -and $report.script_verdict -eq 'PASS' -and
        $report.outcome -eq 'natural_killz_checkpoint_retry_docked_right' -and
        $report.checkpoint_id -eq $expectedCheckpoint -and
        $report.naturally_left_right_docked_shuttle -eq $true -and
        $report.fell_below_world_killz -eq $true -and
        $report.production_dying_state_observed -eq $true -and
        $report.respawned_at_expected_checkpoint_without_fallback -eq $true -and
        $report.mechanisms_reset_after_death -eq $true -and
        $report.retry_boarded_real_shuttle_support -eq $true -and
        $report.retry_docked_right_on_real_support -eq $true -and
        $report.death_events -eq 1 -and $report.probe_direct_teleports_after_boarding -eq 0
    if (-not $recoveryPassed) { throw "Gate recovery assertions failed; inspect $($fresh[0].FullName)" }
    Write-Output "Gate $Gate $expectedGait natural KillZ/checkpoint recovery PASS: report=$($fresh[0].FullName)"
} else {
    $expectedOutcome = $false
    switch ($Gate) {
        'A' {
            if ($Gait -eq 'Run') {
                $expectedOutcome = $report.outcome -eq 'landed_target' -and $report.jump_launched -eq $true
            } else {
                $expectedOutcome = $report.outcome -eq 'fell_before_target' -and $report.jump_launched -eq $true
            }
        }
        'B' {
            $expectedOutcome = $report.outcome -eq 'landed_target' -and $report.pad_supported -eq $true -and
                $report.pad_activation_count -eq 1 -and $report.peak_feet_z_cm -ge 480
        }
        'C' {
            if ($Gait -eq 'Run') {
                if ($OffsetCm -eq -40) {
                    $expectedOutcome = $report.outcome -eq 'landed_target_with_active_timer' -and
                        $report.timed_supported -eq $true -and $report.timed_triggered -eq $true -and
                        $report.timed_disabled_before_exit -eq $false -and $report.timed_remaining_at_exit_s -gt 0
                } else {
                    # Other C Run offsets are valid timing measurements; -40 is the currently repeated pass point.
                    $expectedOutcome = $report.outcome -eq 'blocked_before_jump' -or
                        ($report.timed_triggered -eq $true -and
                         $report.outcome -in @('landed_target_with_active_timer', 'landed_target_after_timed_expired'))
                }
            } else {
                # C Walk is exploratory only; the v7 contract does not classify it as a Run gate.
                $expectedOutcome = $report.outcome -eq 'landed_target_after_timed_expired' -and
                    $report.timed_disabled_before_exit -eq $true -and $report.timed_remaining_at_exit_s -le 0
            }
        }
    }
    if (-not $expectedOutcome) { throw "Gate physical acceptance failed for $Gate $expectedGait; inspect $($fresh[0].FullName)" }
    $classification = if ($Gate -eq 'C' -and ($Gait -eq 'Walk' -or $OffsetCm -ne -40)) {
        'exploratory measurement'
    } else { 'physical gate PASS' }
    Write-Output ('Gate {0} {1} {2}: outcome={3}, report={4}' -f $Gate, $expectedGait, $classification, $report.outcome, $fresh[0].FullName)
}
if ((Get-FileHash -LiteralPath $mapFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $mapHash) {
    throw 'Isolated fixture map changed during runtime probe'
}
