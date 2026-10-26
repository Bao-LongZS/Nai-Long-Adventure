[CmdletBinding()]
param(
    [switch]$AuditOnly,
    [switch]$RunCore,
    [ValidateSet('CP01', 'CP02', 'CP03', 'CP04', 'CP05', 'CP06', 'CP07')]
    [string]$RecoveryCheckpoint,
    [string]$ConfigFile,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
if ($AuditOnly -and $RunCore) { throw 'Choose -AuditOnly or -RunCore, not both.' }
if ([string]::IsNullOrWhiteSpace($ConfigFile)) {
    $ConfigFile = Join-Path $PSScriptRoot 'Scripts/p1_integrated_v7_config.json'
}

$projectRoot = Join-Path $PSScriptRoot 'NaiwaP0'
$mapPackage = '/Game/Naiwa/P1/Maps/L_NaiwaP1_v7'
$mapFile = Join-Path $projectRoot 'Content/Naiwa/P1/Maps/L_NaiwaP1_v7.umap'
$oldMapFile = Join-Path $projectRoot 'Content/Naiwa/P1/Maps/L_NaiwaP1_Integrated.umap'
$oldConfigFile = Join-Path $PSScriptRoot 'Scripts/p1_integrated_full_config_v1.json'
$buildReportFile = Join-Path $projectRoot 'Saved/NaiwaP1/integrated_v7_build.json'
$outerBuildReportFile = Join-Path $projectRoot 'Saved/NaiwaP1/integrated_v7_build_outer.json'
$oldMapSha256 = 'ee5c82788fa2290089f27f4021f1f8539f9bfa979cf357910a4eabcdde73a350'
$oldConfigSha256 = '3aad9c28e6a88397aeb6e1652559fb8d60496185fe255633ab259d7e72f2aa5d'
$issues = [System.Collections.Generic.List[string]]::new()

function Add-Issue([string]$Message) { [void]$issues.Add($Message) }
function Hash-OrNull([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function Report-Hash($Object, [string]$Key) {
    if ($null -eq $Object) { return $null }
    $property = $Object.PSObject.Properties[$Key]
    if ($null -eq $property) { return $null }
    return [string]$property.Value
}
function Is-FiniteNumber($Value) {
    if ($null -eq $Value) { return $false }
    try { $number = [double]$Value } catch { return $false }
    return -not ([double]::IsNaN($number) -or [double]::IsInfinity($number))
}

$audit = [ordered]@{
    schema = 'NaiwaP1V7StaticAuditV1'
    status = 'INCOMPLETE'
    scope = 'static_config_and_file_identity_only'
    map_package = $mapPackage
    config_file = $ConfigFile
    map_file = $mapFile
    config_sha256 = Hash-OrNull $ConfigFile
    map_sha256 = Hash-OrNull $mapFile
    build_report_file = $buildReportFile
    outer_build_report_file = $outerBuildReportFile
    old_integrated_map_sha256 = Hash-OrNull $oldMapFile
    old_integrated_config_sha256 = Hash-OrNull $oldConfigFile
    v7_runtime_pass = $false
    real_keyboard_evidence = $false
    gate_probe_evidence = $false
    timed_c_design = $null
    protected_package_sha256_current = [ordered]@{}
    pending_runtime_contract = [ordered]@{
        flag = '-NaiwaP1V7MixedCoreProbe'
        report_schema = 'NaiwaP1V7MixedCoreRouteV1'
        fresh_report_glob = 'v7_mixed_core_route_*.json'
        required_evidence = @('exact_map_and_config_identity', 'physical_mixed_gait_route',
            'eight_grounded_checkpoints', 'eleven_individually_landed_gaps',
            'two_terminal_bounce_timed_compounds_completed', 'hidden_terminal_pits_invisible_and_query_enabled',
            'moving_platform_speed_varied',
            'physical_pickup_and_goal_won', 'randomized_shuttle_traps_avoided',
            'c_shuttle_rises_to_timed_dock', 'zero_probe_teleports_or_trigger_injections')
        separate_acceptance = @('every_checkpoint_death_and_recovery', 'real_keyboard_and_camera',
            'pause_and_restart',
            'C_Walk_landing_after_timer_disable_is_allowed_and_does_not_make_C_a_Run_only_gate')
    }
    issues = @()
}

if ($audit.old_integrated_map_sha256 -ne $oldMapSha256) {
    Add-Issue 'The protected L_NaiwaP1_Integrated.umap differs from its frozen candidate SHA256.'
}
if ($audit.old_integrated_config_sha256 -ne $oldConfigSha256) {
    Add-Issue 'The protected p1_integrated_full_config_v1.json differs from its frozen candidate SHA256.'
}
if (-not $audit.config_sha256) { Add-Issue "V7 config is missing: $ConfigFile" }
if (-not $audit.map_sha256) { Add-Issue "V7 map is missing: $mapFile" }

$config = $null
if ($audit.config_sha256) {
    try { $config = Get-Content -LiteralPath $ConfigFile -Raw -Encoding UTF8 | ConvertFrom-Json }
    catch { Add-Issue "V7 config is not valid JSON: $($_.Exception.Message)" }
}

if ($null -ne $config) {
    if ($config.schema -ne 'NaiwaP1IntegratedV7V1') { Add-Issue 'Unexpected V7 config schema.' }
    if ($config.map -ne $mapPackage) { Add-Issue 'V7 config points to the wrong map package.' }
    if ($config.required_pickup_count -ne 1) { Add-Issue 'V7 must require only the final dedicated pickup to unlock its goal.' }
    if (-not (Is-FiniteNumber $config.kill_z) -or [double]$config.kill_z -ne -1000.0) {
        Add-Issue 'V7 KillZ must be -1000 cm.'
    }
    if ('Content/Naiwa/P1/Maps/L_NaiwaP1_Integrated.umap' -notin @($config.protected_packages)) {
        Add-Issue 'The existing integrated map is absent from V7 protected_packages.'
    }
    $audit.timed_c_design = $config.timed_c_design
    if ($config.timed_c_design.role -ne 'mandatory_timed_transition' -or
        $config.timed_c_design.run_only_gate -ne $false -or
        $config.timed_c_design.walk_landing_after_disable_allowed -ne $true) {
        Add-Issue 'C must remain a required timed transition; isolated Walk landing after disable cannot be classified as a Run-only gate.'
    }
    if ($config.show_checkpoint_labels -ne $true) { Add-Issue 'V7 checkpoint labels must be enabled.' }
    $retainedOverrides = $config.retained_source_overrides
    if ($null -eq $retainedOverrides -or
        [double]$retainedOverrides.Terrain_BounceLanding.top -ne 420.0 -or
        [double]$retainedOverrides.Bounce.launch_velocity_z -ne 1250.0 -or
        @($retainedOverrides.Checkpoint.at).Count -ne 3 -or
        [double]$retainedOverrides.Checkpoint.at[2] -ne 512.0) {
        Add-Issue 'The teaching bounce landing and checkpoint must be raised to 420 cm and driven by the retained bounce pad.'
    }
    if ((420.0 - 120.0) -le 264.7) { Add-Issue 'The teaching landing can still be reached by the documented normal-jump envelope.' }
    $actors = @($config.extension_actors)
    if ($actors.Count -ne 59) { Add-Issue "Expected 59 V7 extension actors, found $($actors.Count)." }
    $protected = @($config.protected_packages)
    if ($protected.Count -ne 12 -or @($protected | Select-Object -Unique).Count -ne 12) {
        Add-Issue 'Expected twelve distinct protected package paths.'
    }
    $projectRootAbsolute = [IO.Path]::GetFullPath($projectRoot).TrimEnd('\') + '\'
    foreach ($relative in $protected) {
        $target = [IO.Path]::GetFullPath((Join-Path $projectRoot $relative))
        if (-not $target.StartsWith($projectRootAbsolute, [StringComparison]::OrdinalIgnoreCase)) {
            Add-Issue "Protected package path escapes the project: $relative"
        } elseif (-not (Test-Path -LiteralPath $target -PathType Leaf)) {
            Add-Issue "Protected package is missing: $relative"
        } else {
            $audit.protected_package_sha256_current[$relative] = Hash-OrNull $target
        }
    }

    $checkpointZones = @(
        @{ id = 'IntegratedCheckpoint01'; left = 1740.0; right = 3120.0; top = 420.0 },
        @{ id = 'IntegratedCheckpoint02'; left = 4500.0; right = 4900.0; top = 180.0 },
        @{ id = 'IntegratedCheckpoint03'; left = 7500.0; right = 7900.0; top = 180.0 },
        @{ id = 'IntegratedCheckpoint04'; left = 10220.0; right = 10520.0; top = 180.0 },
        @{ id = 'IntegratedCheckpoint05'; left = 13420.0; right = 14120.0; top = 480.0 },
        @{ id = 'IntegratedCheckpoint06'; left = 16440.0; right = 16840.0; top = 240.0 },
        @{ id = 'IntegratedCheckpoint07'; left = 19260.0; right = 20260.0; top = 240.0 },
        @{ id = 'IntegratedCheckpoint08'; left = 23860.0; right = 24340.0; top = 240.0 }
    )
    $checkpoints = @($config.checkpoints)
    if ($checkpoints.Count -ne 8) { Add-Issue "Expected eight checkpoint config entries, found $($checkpoints.Count)." }
    foreach ($zone in $checkpointZones) {
        $matches = @($checkpoints | Where-Object { $_.id -eq $zone.id })
        if ($matches.Count -ne 1) { Add-Issue "Expected exactly one $($zone.id)."; continue }
        $cp = $matches[0]
        if (-not (Is-FiniteNumber $cp.x) -or -not (Is-FiniteNumber $cp.top) -or
            [double]$cp.x -le $zone.left + 40.0 -or [double]$cp.x -ge $zone.right - 40.0 -or
            [double]$cp.top -ne $zone.top) {
            Add-Issue "$($zone.id) is not centered on its planned safe static bank/top."
        }
        if ($zone.id -eq 'IntegratedCheckpoint01') {
            if ($cp.source -ne 'retained_teaching' -or $cp.role -ne 'Checkpoint' -or
                $retainedOverrides.Checkpoint.show_checkpoint_label -ne $true) {
                Add-Issue 'Checkpoint01 must be the retained teaching checkpoint.'
            }
        } else {
            $cpActors = @($actors | Where-Object { $_.kind -eq 'Checkpoint' -and $_.role -eq $cp.role })
            if ($cp.source -ne 'v7_extension' -or $cpActors.Count -ne 1 -or
                $cpActors[0].id -ne $cp.id -or @($cpActors[0].at).Count -ne 3 -or
                [double]$cpActors[0].at[0] -ne [double]$cp.x -or
                [double]$cpActors[0].at[1] -ne 0.0 -or
                [double]$cpActors[0].at[2] -ne [double]$cp.top + 92.0 -or
                $cpActors[0].show_checkpoint_label -ne $true -or
                @($cpActors[0].respawn_offset).Count -ne 3 -or
                @($cpActors[0].respawn_offset | Where-Object { [double]$_ -ne 0.0 }).Count -ne 0) {
                Add-Issue "$($zone.id) actor does not match its safe-bank checkpoint configuration."
            }
        }
    }

    $chainZones = @(
        @{ id = 'A-4'; left = 7900.0; right = 10220.0; top = 180.0; count = 3 },
        @{ id = 'B-5'; left = 14120.0; right = 16440.0; top = 480.0; count = 3 },
        @{ id = 'C-5'; left = 20260.0; right = 23860.0; top = 240.0; count = 5 }
    )
    $chains = @($config.gap_chains)
    if ($chains.Count -ne 3) { Add-Issue "Expected three gap chains, found $($chains.Count)." }
    $totalGaps = 0
    foreach ($zone in $chainZones) {
        $matches = @($chains | Where-Object { $_.id -eq $zone.id })
        if ($matches.Count -ne 1) { Add-Issue "Expected exactly one $($zone.id) gap chain."; continue }
        $chain = $matches[0]
        $gaps = @($chain.gaps)
        $totalGaps += $gaps.Count
        if ($gaps.Count -ne $zone.count -or -not (Is-FiniteNumber $chain.top) -or
            [double]$chain.top -ne $zone.top) {
            Add-Issue "$($zone.id) count or walking top differs from the reviewed V7 candidate."
        }
        $previousRight = $zone.left
        for ($index = 0; $index -lt $gaps.Count; $index++) {
            $pair = @($gaps[$index])
            if ($pair.Count -ne 2 -or -not (Is-FiniteNumber $pair[0]) -or -not (Is-FiniteNumber $pair[1])) {
                Add-Issue "$($zone.id) gap $($index + 1) has no finite [left,right] coordinates."
                continue
            }
            $left = [double]$pair[0]
            $right = [double]$pair[1]
            if ($left -lt $zone.left -or $right -gt $zone.right -or
                [Math]::Abs(($right - $left) - 240.0) -gt 0.01 -or
                [Math]::Abs(($left - $previousRight) - 400.0) -gt 0.01) {
                Add-Issue "$($zone.id) gap $($index + 1) is outside the chain or violates the 400 cm island / 240 cm gap candidate."
            }
            $island = @($actors | Where-Object { $_.kind -eq 'Terrain' -and
                [double]$_.top -eq [double]$zone.top -and
                [double]$_.left -eq $previousRight -and [double]$_.right -eq $left })
            if ($island.Count -ne 1) {
                Add-Issue "$($zone.id) gap $($index + 1) has no exact static takeoff island."
            }
            $bridge = @($actors | Where-Object { $_.kind -eq 'Terrain' -and
                [double]$_.top -eq [double]$zone.top -and
                [double]$_.left -lt $right -and [double]$_.right -gt $left })
            if ($bridge.Count -ne 0) {
                Add-Issue "$($zone.id) gap $($index + 1) is bridged by collision terrain."
            }
            $spikes = @($actors | Where-Object { $_.kind -eq 'Hazard' -and
                $_.gap_chain -eq $zone.id -and [int]$_.gap_index -eq ($index + 1) })
            if ($spikes.Count -ne 1 -or $spikes[0].b_reveal_on_spawn -ne $true -or
                @($spikes[0].at).Count -ne 3 -or @($spikes[0].extent).Count -ne 3 -or
                [double]$spikes[0].at[0] -ne ($left + $right) / 2.0 -or
                [double]$spikes[0].at[1] -ne 0.0 -or
                [double]$spikes[0].at[2] -ne [double]$zone.top - 90.0 -or
                [double]$spikes[0].extent[0] -ne 120.0 -or
                [double]$spikes[0].extent[1] -ne 160.0 -or
                [double]$spikes[0].extent[2] -ne 50.0 -or $spikes[0].b_use_spike_cluster -ne $true) {
                Add-Issue "$($zone.id) gap $($index + 1) lacks its exact visible pit hazard."
            }
            $previousRight = $right
        }
        if ($gaps.Count -gt 0 -and [Math]::Abs(($zone.right - $previousRight) - 400.0) -gt 0.01) {
            Add-Issue "$($zone.id) has no 400 cm landing island after its final gap."
        }
        if ($gaps.Count -gt 0) {
            $landingIsland = @($actors | Where-Object { $_.kind -eq 'Terrain' -and
                [double]$_.top -eq [double]$zone.top -and
                [double]$_.left -eq $previousRight -and [double]$_.right -eq $zone.right })
            if ($landingIsland.Count -ne 1) {
                Add-Issue "$($zone.id) has no exact static landing island after its final gap."
            }
        }
    }
    if ($totalGaps -ne 11) { Add-Issue "Expected 11 explicit gaps, found $totalGaps." }

    $pitHazards = @($actors | Where-Object { $_.kind -eq 'Hazard' -and $_.gap_chain })
    if ($pitHazards.Count -ne 11) { Add-Issue "Expected 11 V7 gap hazards, found $($pitHazards.Count)." }
    foreach ($hazard in $pitHazards) {
        if ($hazard.b_reveal_on_spawn -ne $true -or $hazard.b_use_spike_cluster -ne $true -or
            $hazard.gap_chain -notin @('A-4', 'B-5', 'C-5') -or
            -not (Is-FiniteNumber $hazard.gap_index)) {
            Add-Issue "Gap hazard $($hazard.id) is not visibly and uniquely associated with a valid gap."
        }
    }
    $hazardKeys = @($pitHazards | ForEach-Object { "$(($_.gap_chain)):$($_.gap_index)" })
    if (@($hazardKeys | Select-Object -Unique).Count -ne 11) { Add-Issue 'Gap hazard chain/index associations are duplicated.' }
    $pickups = @($actors | Where-Object { $_.kind -eq 'Pickup' } | Sort-Object pickup_order)
    if ($pickups.Count -ne 4 -or
        @($pickups | Where-Object { [int]$_.pickup_order -ge 1 -and [int]$_.pickup_order -le 4 }).Count -ne 4 -or
        (($pickups | ForEach-Object { [int]$_.pickup_order }) -join ',') -ne '1,2,3,4' -or
        $pickups[1].id -ne 'IntegratedPickup02' -or $pickups[2].id -ne 'IntegratedPickup03' -or
        [double]$pickups[0].at[0] -ne 1000.0 -or [double]$pickups[1].at[0] -ne 8740.0 -or
        [double]$pickups[2].at[0] -ne 14960.0 -or [double]$pickups[3].at[0] -ne 26800.0) {
        Add-Issue 'V7 needs four ordered pickups on the planned safe route.'
    }
    if (@($actors | Where-Object { $_.kind -eq 'Goal' }).Count -ne 1) { Add-Issue 'V7 config needs one goal actor.' }
    if (@($actors | Where-Object { $_.kind -eq 'Checkpoint' }).Count -ne 7) {
        Add-Issue 'V7 config needs seven new checkpoint actors in addition to the retained teaching checkpoint.'
    }
    foreach ($id in @('SwitchA', 'SwitchB', 'SwitchC', 'BounceB', 'TimedC', 'BounceFinal', 'TimedFinal', 'BounceFinal2', 'TimedFinal2')) {
        if (@($actors | Where-Object { $_.id -eq $id }).Count -ne 1) {
            Add-Issue "V7 config needs exactly one mechanism actor $id."
        }
    }
    $switchA = $actors | Where-Object { $_.id -eq 'SwitchA' } | Select-Object -First 1
    $switchB = $actors | Where-Object { $_.id -eq 'SwitchB' } | Select-Object -First 1
    $switchC = $actors | Where-Object { $_.id -eq 'SwitchC' } | Select-Object -First 1
    $bounceB = $actors | Where-Object { $_.id -eq 'BounceB' } | Select-Object -First 1
    $timedC = $actors | Where-Object { $_.id -eq 'TimedC' } | Select-Object -First 1
    if ($switchA -and ([double]$switchA.at[0] + [double]$switchA.dock_right_x + [double]$switchA.extent[0] -ne 7260.0)) {
        Add-Issue 'A shuttle end must leave the planned 240 cm jump to the right bank.'
    }
    if ($switchB -and $bounceB -and
        ([double]$switchB.at[0] + [double]$switchB.dock_right_x + [double]$switchB.extent[0] -ne 13120.0 -or
         [double]$bounceB.at[0] - [double]$bounceB.extent[0] -ne 13180.0 -or
         [double]$bounceB.at[2] + [double]$bounceB.extent[2] -ne 180.0)) {
        Add-Issue 'B shuttle and solid bounce pad must preserve the 60 cm same-height candidate seam.'
    }
    if ($switchC -and $timedC -and
        ([double]$switchC.at[0] + [double]$switchC.dock_right_x + [double]$switchC.extent[0] -ne 18830.0 -or
         [double]$timedC.at[0] - [double]$timedC.extent[0] -ne 18840.0 -or
         [double]$timedC.at[0] + [double]$timedC.extent[0] -ne 19260.0 -or
         [double]$timedC.top_z -ne 340.0 -or
         [double]$timedC.active_seconds -ne 1.36 -or
         [double]$timedC.stable_landing_seconds -ne 0.18)) {
        Add-Issue 'C shuttle and elevated timed platform differ from the measured isolation candidate.'
    }
    $finalApproach = $actors | Where-Object { $_.id -eq 'FinalApproach' } | Select-Object -First 1
    $bounceFinal = $actors | Where-Object { $_.id -eq 'BounceFinal' } | Select-Object -First 1
    $timedFinal = $actors | Where-Object { $_.id -eq 'TimedFinal' } | Select-Object -First 1
    $intermediateBank = $actors | Where-Object { $_.id -eq 'FinalIntermediate' } | Select-Object -First 1
    $bounceFinal2 = $actors | Where-Object { $_.id -eq 'BounceFinal2' } | Select-Object -First 1
    $timedFinal2 = $actors | Where-Object { $_.id -eq 'TimedFinal2' } | Select-Object -First 1
    $finalBank = $actors | Where-Object { $_.id -eq 'Final' } | Select-Object -First 1
    $hiddenFinalPits = @($actors | Where-Object { $_.id -in @('CompoundHiddenPit01', 'CompoundHiddenPit02', 'CompoundHiddenPit03', 'CompoundHiddenPit04') })
    if ($null -eq $finalApproach -or [double]$finalApproach.left -ne 23860.0 -or
        [double]$finalApproach.right -ne 24340.0 -or [double]$finalApproach.top -ne 240.0 -or
        $null -eq $bounceFinal -or [double]$bounceFinal.at[0] -ne 24420.0 -or
        [double]$bounceFinal.at[2] + [double]$bounceFinal.extent[2] -ne 200.0 -or
        [double]$bounceFinal.launch_velocity_z -ne 1100.0 -or
        $null -eq $timedFinal -or [double]$timedFinal.top_z -ne 540.0 -or
        [double]$timedFinal.active_seconds -ne 0.60 -or
        [double]$timedFinal.stable_landing_seconds -ne 0.1 -or
        [double]$timedFinal.at[0] -ne 24810.0 -or [double]$timedFinal.extent[0] -ne 140.0 -or
        $null -eq $intermediateBank -or [double]$intermediateBank.left -ne 25130.0 -or
        [double]$intermediateBank.right -ne 25440.0 -or [double]$intermediateBank.top -ne 540.0 -or
        $null -eq $bounceFinal2 -or [double]$bounceFinal2.at[0] -ne 25220.0 -or
        [double]$bounceFinal2.at[2] + [double]$bounceFinal2.extent[2] -ne 541.0 -or
        [double]$bounceFinal2.launch_velocity_z -ne 1100.0 -or
        $null -eq $timedFinal2 -or [double]$timedFinal2.at[0] -ne 25640.0 -or
        [double]$timedFinal2.top_z -ne 840.0 -or [double]$timedFinal2.extent[0] -ne 140.0 -or
        [double]$timedFinal2.active_seconds -ne 0.50 -or
        $null -eq $finalBank -or [double]$finalBank.left -ne 26040.0 -or
        [double]$finalBank.right -ne 28200.0 -or [double]$finalBank.top -ne 840.0 -or
        $hiddenFinalPits.Count -ne 4 -or
        @($hiddenFinalPits | Where-Object { $_.kind -ne 'Hazard' -or $_.b_reveal_on_spawn -ne $false -or
            $_.b_use_spike_cluster -ne $true }).Count -ne 0) {
        Add-Issue 'The terminal bounce, elevated timed platform, hidden spike pits and goal bank do not match the reviewed compound layout.'
    }
    if ($config.final_compound_design.checkpoint_id -ne 'IntegratedCheckpoint08' -or
        $config.final_compound_design.compound_count -ne 2 -or
        @($config.final_compound_design.bounce_pad_top_z_cm).Count -ne 2 -or
        [double]$config.final_compound_design.bounce_pad_top_z_cm[0] -ne 200 -or
        [double]$config.final_compound_design.bounce_pad_top_z_cm[1] -ne 541 -or
        @($config.final_compound_design.timed_platform_top_z_cm).Count -ne 2 -or
        [double]$config.final_compound_design.timed_platform_top_z_cm[0] -ne 540 -or
        [double]$config.final_compound_design.timed_platform_top_z_cm[1] -ne 840 -or
        @($config.final_compound_design.timed_platform_active_seconds).Count -ne 2 -or
        [double]$config.final_compound_design.timed_platform_active_seconds[0] -ne 0.60 -or
        [double]$config.final_compound_design.timed_platform_active_seconds[1] -ne 0.50 -or
        @($config.final_compound_design.hidden_pit_hazard_ids).Count -ne 4) {
        Add-Issue 'Terminal compound must declare two sequential bounce/timed pairs and four hidden pit hazards.'
    }
    $expectedHiddenPits = @(
        @{ id = 'CompoundHiddenPit01'; x = 24580.0; z = 100.0; extent_x = 80.0; extent_z = 50.0 },
        @{ id = 'CompoundHiddenPit02'; x = 25040.0; z = 400.0; extent_x = 80.0; extent_z = 50.0 },
        @{ id = 'CompoundHiddenPit03'; x = 25455.0; z = 430.0; extent_x = 80.0; extent_z = 50.0 },
        @{ id = 'CompoundHiddenPit04'; x = 25840.0; z = 750.0; extent_x = 80.0; extent_z = 50.0 }
    )
    foreach ($expectedPit in $expectedHiddenPits) {
        $pit = $hiddenFinalPits | Where-Object { $_.id -eq $expectedPit.id } | Select-Object -First 1
        if ($null -eq $pit -or [double]$pit.at[0] -ne $expectedPit.x -or
            [double]$pit.at[2] -ne $expectedPit.z -or [double]$pit.extent[0] -ne $expectedPit.extent_x -or
            [double]$pit.extent[2] -ne $expectedPit.extent_z) {
            Add-Issue "Terminal hidden spike pit geometry changed: $($expectedPit.id)."
        }
    }
    foreach ($switch in @($switchA, $switchB, $switchC)) {
        if ($null -eq $switch -or [double]$switch.travel_speed_cm_per_second -ne 300.0 -or
            [double]$switch.travel_speed_variation_cm_per_second -ne 100.0 -or
            [int]$switch.transit_trap_min_count -ne 1 -or
            [int]$switch.transit_trap_max_count -ne 2 -or [int]$switch.transit_trap_seed -le 0) {
            Add-Issue 'Each V7 shuttle must use the configured variable speed and generate one or two reproducible transit traps per departure.'
        }
    }
    if ($switchC -and ([double]$switchC.dock_left_top_z -ne 240.0 -or
        [double]$switchC.dock_right_top_z -ne 340.0)) {
        Add-Issue 'C shuttle must rise 100 cm from its safe bank to the timed-platform dock.'
    }
}

if (-not (Test-Path -LiteralPath $buildReportFile -PathType Leaf)) {
    Add-Issue "Saved V7 build report is missing: $buildReportFile"
} else {
    try {
        $build = Get-Content -LiteralPath $buildReportFile -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($build.schema -ne 'NaiwaP1IntegratedV7V1' -or
            $build.status -ne 'saved_v7_candidate_pending_route_and_visual_review' -or
            $build.protected_packages_unchanged -ne $true -or
            $build.config_sha256 -ne $audit.config_sha256 -or
            $build.target_map_sha256_after -ne $audit.map_sha256 -or
            $build.layout.verified_actor_count -ne 73 -or
            $build.layout.extension_actor_count -ne 59 -or
            $build.layout.required_pickup_count -ne 1 -or
            $build.layout.physical_pickup_count -ne 4 -or
            $build.layout.checkpoint_count -ne 8 -or
            $build.layout.gap_count -ne 11 -or
            $build.layout.visible_pit_hazard_count -ne 11 -or
            $build.layout.hidden_terminal_pit_hazard_count -ne 4 -or
            $build.layout.visible_pit_spike_points -ne 33 -or
            $build.layout.shuttle_speed_profile.base_cm_s -ne 300 -or
            $build.layout.shuttle_speed_profile.variation_cm_s -ne 100 -or
            $build.layout.shuttle_speed_profile.cycles_per_crossing -ne 2 -or
            @($build.layout.shuttle_speed_profile.sampled_range_cm_s).Count -ne 2 -or
            [double]$build.layout.shuttle_speed_profile.sampled_range_cm_s[0] -ne 200 -or
            [double]$build.layout.shuttle_speed_profile.sampled_range_cm_s[1] -ne 400 -or
            $build.layout.checkpoint_labels_enabled -ne $true -or
            $build.layout.retained_teaching_bounce_rise_cm -ne 300 -or
            @($build.layout.c_shuttle_endpoint_top_z_cm).Count -ne 2 -or
            [double]$build.layout.c_shuttle_endpoint_top_z_cm[0] -ne 240 -or
            [double]$build.layout.c_shuttle_endpoint_top_z_cm[1] -ne 340 -or
            $build.layout.timed_c_design.role -ne 'mandatory_timed_transition' -or
            $build.layout.timed_c_design.run_only_gate -ne $false -or
            $build.layout.timed_c_design.walk_landing_after_disable_allowed -ne $true) {
            Add-Issue 'Saved V7 build report does not match the current config/map hashes, status or protected-asset audit.'
        }
        foreach ($relative in $audit.protected_package_sha256_current.Keys) {
            $current = $audit.protected_package_sha256_current[$relative]
            if ((Report-Hash $build.protected_sha256_before $relative) -ne $current -or
                (Report-Hash $build.protected_sha256_after $relative) -ne $current) {
                Add-Issue "Saved V7 build report protected hash drift: $relative"
            }
        }
    } catch { Add-Issue "Saved V7 build report is invalid JSON: $($_.Exception.Message)" }
}

if (-not (Test-Path -LiteralPath $outerBuildReportFile -PathType Leaf)) {
    Add-Issue "Independent V7 build report is missing: $outerBuildReportFile"
} else {
    try {
        $outerBuild = Get-Content -LiteralPath $outerBuildReportFile -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($outerBuild.schema -ne 'NaiwaP1IntegratedV7OuterBuildV1' -or
            $outerBuild.target_map -ne $mapPackage -or
            $outerBuild.status -ne 'saved_v7_candidate_pending_route_and_visual_review' -or
            $outerBuild.protected_packages_unchanged -ne $true -or
            $outerBuild.config_sha256 -ne $audit.config_sha256 -or
            $outerBuild.target_map_sha256_after -ne $audit.map_sha256) {
            Add-Issue 'Independent V7 build report does not match the current map/config/protected status.'
        }
        foreach ($relative in $audit.protected_package_sha256_current.Keys) {
            $current = $audit.protected_package_sha256_current[$relative]
            if ((Report-Hash $outerBuild.protected_sha256_before $relative) -ne $current -or
                (Report-Hash $outerBuild.protected_sha256_after $relative) -ne $current) {
                Add-Issue "Independent V7 build report protected hash drift: $relative"
            }
        }
    } catch { Add-Issue "Independent V7 build report is invalid JSON: $($_.Exception.Message)" }
}

$audit.issues = @($issues)
if ($issues.Count -eq 0) { $audit.status = 'STATIC_READY' }
if ($issues.Count -gt 0) { throw "INCOMPLETE: V7 static audit has $($issues.Count) issue(s)." }

if ($RunCore) {
    $editor = Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    $projectFile = Join-Path $projectRoot 'Nai_Long.uproject'
    $savedDirectory = Join-Path $projectRoot 'Saved/NaiwaP1'
    if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw "UnrealEditor-Cmd is missing: $editor" }
    if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) { throw "Project file is missing: $projectFile" }
    $expectedConfigSha = '4fd6914655c2ae7c28567bbbdb808b758b0686e7b9bd260a145891d322aa2e59'
    $expectedMapSha = '4a55ced57ddbe47044aa7f4965fc4013cc82ccc48976a9819a6217a4dbef5754'
    if ($audit.config_sha256 -ne $expectedConfigSha -or $audit.map_sha256 -ne $expectedMapSha) {
        throw 'V7 mixed-core runtime has no reviewed code identity for the current config/map hash.'
    }
    if ([string]::IsNullOrWhiteSpace($RecoveryCheckpoint)) {
    $gateCases = @(
        @{ name = 'A Run exit'; pattern = 'v7_gate_probe_v2_A_run_*.json'; test = {
            param($r) $r.schema -eq 'NaiwaP1V7GateProbeV2' -and $r.status -eq 'measured' -and
                $r.map -eq 'L_NaiwaP1_v7_GateProbe' -and $r.gate -eq 'A' -and $r.gait -eq 'Run300' -and
                $r.jump_offset_cm -eq 0 -and $r.outcome -eq 'landed_target' -and $r.jump_launched -eq $true -and
                $r.boarded_real_shuttle_support -eq $true -and $r.checkpoint_activated_by_native_overlap -eq $true
        } },
        @{ name = 'B Run bounce'; pattern = 'v7_gate_probe_v2_B_run_*.json'; test = {
            param($r) $r.schema -eq 'NaiwaP1V7GateProbeV2' -and $r.status -eq 'measured' -and
                $r.map -eq 'L_NaiwaP1_v7_GateProbe' -and $r.gate -eq 'B' -and $r.gait -eq 'Run300' -and
                $r.jump_offset_cm -eq 0 -and $r.outcome -eq 'landed_target' -and $r.pad_supported -eq $true -and
                $r.pad_activation_count -eq 1 -and $r.boarded_real_shuttle_support -eq $true
        } },
        @{ name = 'C Run -40 cm'; pattern = 'v7_gate_probe_v2_C_run_*.json'; test = {
            param($r) $r.schema -eq 'NaiwaP1V7GateProbeV2' -and $r.status -eq 'measured' -and
                $r.map -eq 'L_NaiwaP1_v7_GateProbe' -and $r.gate -eq 'C' -and $r.gait -eq 'Run300' -and
                $r.jump_offset_cm -eq -40 -and $r.outcome -eq 'landed_target_with_active_timer' -and
                $r.timed_supported -eq $true -and $r.timed_triggered -eq $true -and
                $r.timed_disabled_before_exit -eq $false -and $r.timed_remaining_at_exit_s -gt 0
        } },
        @{ name = 'C Walk after timer expiry'; pattern = 'v7_gate_probe_v2_C_walk_*.json'; test = {
            param($r) $r.schema -eq 'NaiwaP1V7GateProbeV2' -and $r.status -eq 'measured' -and
                $r.map -eq 'L_NaiwaP1_v7_GateProbe' -and $r.gate -eq 'C' -and $r.gait -eq 'Walk150' -and
                $r.outcome -eq 'landed_target_after_timed_expired' -and $r.timed_disabled_before_exit -eq $true -and
                $r.timed_remaining_at_exit_s -le 0
        } },
        @{ name = 'A natural checkpoint recovery'; pattern = 'v7_gate_recovery_A_walk_*.json'; checkpoint = 'IntegratedCheckpoint02' },
        @{ name = 'B natural checkpoint recovery'; pattern = 'v7_gate_recovery_B_walk_*.json'; checkpoint = 'IntegratedCheckpoint04' },
        @{ name = 'C natural checkpoint recovery'; pattern = 'v7_gate_recovery_C_walk_*.json'; checkpoint = 'IntegratedCheckpoint06' }
    )
    $verifiedGateReports = [System.Collections.Generic.List[object]]::new()
    foreach ($case in $gateCases) {
        $matchedPath = $null
        foreach ($candidate in (Get-ChildItem -LiteralPath $savedDirectory -Filter $case.pattern -File |
            Sort-Object LastWriteTimeUtc -Descending)) {
            try { $candidateReport = Get-Content -LiteralPath $candidate.FullName -Raw -Encoding UTF8 | ConvertFrom-Json }
            catch { continue }
            $valid = if ($case.checkpoint) {
                $candidateReport.schema -eq 'NaiwaP1V7GateRecoveryV1' -and $candidateReport.status -eq 'PASS' -and
                    $candidateReport.script_verdict -eq 'PASS' -and
                    $candidateReport.outcome -eq 'natural_killz_checkpoint_retry_docked_right' -and
                    $candidateReport.checkpoint_id -eq $case.checkpoint -and
                    $candidateReport.naturally_left_right_docked_shuttle -eq $true -and
                    $candidateReport.fell_below_world_killz -eq $true -and
                    $candidateReport.production_dying_state_observed -eq $true -and
                    $candidateReport.respawned_at_expected_checkpoint_without_fallback -eq $true -and
                    $candidateReport.mechanisms_reset_after_death -eq $true -and
                    $candidateReport.retry_boarded_real_shuttle_support -eq $true -and
                    $candidateReport.retry_docked_right_on_real_support -eq $true -and
                    $candidateReport.death_events -eq 1 -and $candidateReport.probe_direct_teleports_after_boarding -eq 0
            } else { & $case.test $candidateReport }
            if ($valid) { $matchedPath = $candidate.FullName; break }
        }
        if (-not $matchedPath) { throw "Missing verified isolated gate evidence: $($case.name)" }
        $verifiedGateReports.Add([ordered]@{ gate = $case.name; report = $matchedPath })
    }
    $audit.gate_probe_evidence = $true
    $audit.isolated_gate_reports = @($verifiedGateReports)
    }
    $reportPattern = 'v7_mixed_core_route_*.json'
    $existingReports = @(Get-ChildItem -LiteralPath $savedDirectory -Filter $reportPattern -File | ForEach-Object FullName)
    $started = [DateTime]::UtcNow
    $stamp = $started.ToString('yyyyMMddTHHmmssZ') + '_' + [Guid]::NewGuid().ToString('N')
    $logFile = Join-Path $savedDirectory "v7_mixed_core_route_$stamp.log"
    $arguments = @($projectFile, $mapPackage, '-game', '-RenderOffScreen', '-windowed', '-ResX=1600', '-ResY=900',
        '-ForceRes', '-unattended', '-nop4', '-nosplash', '-NaiwaP1Standalone', '-NaiwaP1V7MixedCoreProbe',
        '-ExecCmds=t.MaxFPS 60', "-abslog=$logFile")
    if (-not [string]::IsNullOrWhiteSpace($RecoveryCheckpoint)) {
        $arguments += "-NaiwaP1V7RecoverAtCheckpoint=$RecoveryCheckpoint"
    }
    & $editor @arguments
    if ($LASTEXITCODE -ne 0) { throw "V7 mixed-core runtime exited $LASTEXITCODE; inspect $logFile" }
    $freshReports = @(Get-ChildItem -LiteralPath $savedDirectory -Filter $reportPattern -File |
        Where-Object { $_.FullName -notin $existingReports -and $_.LastWriteTimeUtc -ge $started })
    if ($freshReports.Count -ne 1) { throw "Expected one fresh V7 mixed-core report, found $($freshReports.Count); inspect $logFile" }
    $reportFile = $freshReports[0].FullName
    $runtime = Get-Content -LiteralPath $reportFile -Raw -Encoding UTF8 | ConvertFrom-Json
    $expectedDeaths = if ([string]::IsNullOrWhiteSpace($RecoveryCheckpoint)) { 0 } else { 1 }
    if ([string]::IsNullOrWhiteSpace($RecoveryCheckpoint)) { $expectedPickups = 4 }
    elseif ([int]$RecoveryCheckpoint.Substring(2) -le 3) { $expectedPickups = 3 }
    elseif ([int]$RecoveryCheckpoint.Substring(2) -le 5) { $expectedPickups = 2 }
    else { $expectedPickups = 1 }
    $expectedRespawnPlacements = if ([string]::IsNullOrWhiteSpace($RecoveryCheckpoint)) { 0 } else { 1 }
    $checkRandomizedTraps = [string]::IsNullOrWhiteSpace($RecoveryCheckpoint)
    $bounceBActivationValid = if ($checkRandomizedTraps) {
        $runtime.bounce_b_activation_count_delta -eq 1
    } else {
        $runtime.bounce_b_activation_count_delta -ge 1
    }
    if ($runtime.schema -ne 'NaiwaP1V7MixedCoreRouteV1' -or $runtime.status -ne 'PASS' -or
        $runtime.outcome -ne 'PASS' -or $runtime.map -ne 'L_NaiwaP1_v7' -or $runtime.map_package -ne $mapPackage -or
        $runtime.map_sha256 -ne $expectedMapSha -or $runtime.config_sha256 -ne $expectedConfigSha -or
        $runtime.exact_map_and_config_identity -ne $true -or $runtime.physical_mixed_gait_route -ne $true -or
        $runtime.full_p1_v7_acceptance -ne $false -or $runtime.real_keyboard_evidence_in_this_process -ne $false -or
        $runtime.death_events_delta -ne $expectedDeaths -or $runtime.probe_character_teleports -ne 0 -or
        $runtime.direct_trigger_injections -ne 0 -or $runtime.platform_control_injections -ne 0 -or
        $runtime.production_respawn_placements -ne $expectedRespawnPlacements -or $runtime.zero_probe_teleports_or_trigger_injections -ne $true -or
        ($checkRandomizedTraps -and ($runtime.random_shuttle_traps_avoided -ne $true -or $runtime.random_shuttle_traps_spawned -lt 3 -or
            $runtime.random_shuttle_traps_jumped -ne $runtime.random_shuttle_traps_spawned)) -or
        $runtime.c_shuttle_rise_to_timed_dock_observed -ne $true -or
        [double]$runtime.c_shuttle_top_z_at_midpoint_cm -lt 280.0 -or
        [double]$runtime.c_shuttle_top_z_at_midpoint_cm -gt 300.0 -or
        [Math]::Abs([double]$runtime.c_shuttle_top_z_at_dock_cm - 340.0) -gt 1.0 -or
        $runtime.eight_grounded_checkpoints -ne $true -or $runtime.checkpoint_count -ne 8 -or
        $runtime.checkpoints_grounded -ne 8 -or @($runtime.checkpoints).Count -ne 8 -or
        $runtime.eleven_individually_landed_gaps -ne $true -or $runtime.gap_count -ne 11 -or
        $runtime.gaps_landed_on_expected_static_islands -ne 11 -or @($runtime.gaps).Count -ne 11 -or
        $runtime.required_pickup_physically_collected -ne $true -or $runtime.pickup_count -ne $expectedPickups -or
        $runtime.four_ordered_pickups_collected -ne ([string]::IsNullOrWhiteSpace($RecoveryCheckpoint)) -or $runtime.physical_goal_won -ne $true -or
        $runtime.checkpoint_recovery_requested -ne (-not [string]::IsNullOrWhiteSpace($RecoveryCheckpoint)) -or
        $runtime.checkpoint_recovery_passed -ne (-not [string]::IsNullOrWhiteSpace($RecoveryCheckpoint)) -or
        $runtime.required_pickup_reacquired_after_recovery -ne (-not [string]::IsNullOrWhiteSpace($RecoveryCheckpoint)) -or
        ($RecoveryCheckpoint -and ($runtime.checkpoint_recovery.checkpoint_id -ne "IntegratedCheckpoint$($RecoveryCheckpoint.Substring(2))" -or
            $runtime.checkpoint_recovery.native_checkpoint_activation_observed -ne $true -or
            $runtime.checkpoint_recovery.production_dying_state_observed -ne $true -or
            $runtime.checkpoint_recovery.exactly_one_natural_death -ne $true -or
            $runtime.checkpoint_recovery.respawned_at_expected_checkpoint -ne $true -or
            $runtime.checkpoint_recovery.respawned_on_configured_safe_bank -ne $true -or
            $runtime.checkpoint_recovery.controls_restored -ne $true -or
            $runtime.checkpoint_recovery.initial_spawn_fallback_used -ne $false)) -or
        $runtime.final_state -ne 'Won' -or $runtime.bounce_b_real_support_launch_and_landing -ne $true -or
        -not $bounceBActivationValid -or $runtime.timed_c_real_support_countdown_and_safe_exit -ne $true -or
        $runtime.timed_c_trigger_count_delta -ne 1 -or $runtime.timed_c_remaining_at_exit_s -le 0.20 -or
        $runtime.terminal_bounce_timed_compound_completed -ne $true -or
        $runtime.two_terminal_bounce_timed_compounds_completed -ne $true -or
        $runtime.first_compound_intermediate_bank_landing -ne $true -or
        $runtime.terminal_bounce_real_support_and_launch -ne $true -or
        $runtime.terminal_bounce_activation_count_delta -ne 1 -or
        [double]$runtime.terminal_bounce_launch_velocity_z_cm_s -le 100.0 -or
        [Math]::Abs([double]$runtime.terminal_timed_landing_feet_z_cm - 540.0) -gt 5.0 -or
        $runtime.terminal_timed_real_support_and_countdown -ne $true -or
        $runtime.terminal_timed_trigger_count_delta -ne 1 -or
        [double]$runtime.terminal_timed_active_remaining_at_takeoff_s -le 0.12 -or
        [double]$runtime.terminal_timed_active_remaining_at_takeoff_s -gt 0.60 -or
        $runtime.terminal_edge_jump_landed_on_goal_bank -ne $true -or
        $runtime.second_terminal_bounce_real_support_and_launch -ne $true -or
        $runtime.second_terminal_timed_real_support_and_countdown -ne $true -or
        $runtime.second_terminal_timed_trigger_count_delta -ne 1 -or
        [double]$runtime.second_terminal_timed_active_remaining_at_takeoff_s -le 0.02 -or
        [double]$runtime.second_terminal_timed_active_remaining_at_takeoff_s -gt 0.50 -or
        $runtime.hidden_terminal_pit_count -ne 4 -or
        $runtime.hidden_terminal_pits_invisible_and_query_enabled -ne $true -or
        $runtime.moving_platform_speed_varied -ne $true -or
        @($runtime.moving_platform_speed_profiles).Count -ne 3 -or
        $runtime.teaching_timer_exited_before_disable -ne $true -or $runtime.teaching_timer_remaining_at_exit_s -le 0 -or
        $runtime.gait_change_count -lt 2 -or @($runtime.gait_changes).Count -lt 2) {
        throw "V7 mixed-core report failed identity or route assertions: $reportFile"
    }

    $expectedShuttleIds = @('SwitchA', 'SwitchB', 'SwitchC')
    for ($index = 0; $index -lt $expectedShuttleIds.Count; $index++) {
        $profile = $runtime.moving_platform_speed_profiles[$index]
        if ($profile.id -ne $expectedShuttleIds[$index] -or $profile.speed_varied -ne $true -or
            $profile.sample_count -lt 10 -or [double]$profile.minimum_cm_s -ge 250.0 -or
            [double]$profile.maximum_cm_s -le 350.0) {
            throw "V7 shuttle speed variation evidence is incomplete for $($expectedShuttleIds[$index]): $reportFile"
        }
    }

    $expectedCheckpointIds = @('IntegratedCheckpoint01', 'IntegratedCheckpoint02', 'IntegratedCheckpoint03',
        'IntegratedCheckpoint04', 'IntegratedCheckpoint05', 'IntegratedCheckpoint06', 'IntegratedCheckpoint07',
        'IntegratedCheckpoint08')
    for ($index = 0; $index -lt 8; $index++) {
        $checkpoint = $runtime.checkpoints[$index]
        if ($checkpoint.id -ne $expectedCheckpointIds[$index] -or
            $checkpoint.activated_by_native_overlap -ne $true -or
            $checkpoint.grounded_on_configured_safe_bank -ne $true) {
            throw "V7 checkpoint evidence is incomplete or out of order at index ${index}: $reportFile"
        }
    }
    $expectedGaps = @(
        @{ chain = 'A-4'; left = 8300; right = 8540; takeoff = 'A4Island01'; landing = 'A4Island02'; top = 180 },
        @{ chain = 'A-4'; left = 8940; right = 9180; takeoff = 'A4Island02'; landing = 'A4Island03'; top = 180 },
        @{ chain = 'A-4'; left = 9580; right = 9820; takeoff = 'A4Island03'; landing = 'A4Island04'; top = 180 },
        @{ chain = 'B-5'; left = 14520; right = 14760; takeoff = 'B5Island01'; landing = 'B5Island02'; top = 480 },
        @{ chain = 'B-5'; left = 15160; right = 15400; takeoff = 'B5Island02'; landing = 'B5Island03'; top = 480 },
        @{ chain = 'B-5'; left = 15800; right = 16040; takeoff = 'B5Island03'; landing = 'B5Island04'; top = 480 },
        @{ chain = 'C-5'; left = 20660; right = 20900; takeoff = 'C5Island01'; landing = 'C5Island02'; top = 240 },
        @{ chain = 'C-5'; left = 21300; right = 21540; takeoff = 'C5Island02'; landing = 'C5Island03'; top = 240 },
        @{ chain = 'C-5'; left = 21940; right = 22180; takeoff = 'C5Island03'; landing = 'C5Island04'; top = 240 },
        @{ chain = 'C-5'; left = 22580; right = 22820; takeoff = 'C5Island04'; landing = 'C5Island05'; top = 240 },
        @{ chain = 'C-5'; left = 23220; right = 23460; takeoff = 'C5Island05'; landing = 'C5Island06'; top = 240 }
    )
    for ($index = 0; $index -lt $expectedGaps.Count; $index++) {
        $gap = $runtime.gaps[$index]
        $expected = $expectedGaps[$index]
        if ($gap.chain -ne $expected.chain -or [double]$gap.gap_left_cm -ne $expected.left -or
            [double]$gap.gap_right_cm -ne $expected.right -or $gap.takeoff_island -ne $expected.takeoff -or
            $gap.landing_island -ne $expected.landing -or $gap.grounded_on_expected_takeoff_island -ne $true -or
            $gap.jump_launched -ne $true -or [double]$gap.landing_x_cm -le $expected.right -or
            [Math]::Abs([double]$gap.landing_feet_z_cm - $expected.top) -gt 5.0 -or
            [double]$gap.launch_velocity_z_cm_s -le 50.0 -or [double]$gap.air_seconds -le 0) {
            throw "V7 gap landing evidence is incomplete or mismatched at index ${index}: $reportFile"
        }
    }
    if ((Get-FileHash -LiteralPath $mapFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedMapSha -or
        (Get-FileHash -LiteralPath $oldMapFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $oldMapSha256 -or
        (Get-FileHash -LiteralPath $oldConfigFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $oldConfigSha256) {
        throw 'Protected or V7 map/config identity changed during the runtime probe.'
    }
    foreach ($relative in $audit.protected_package_sha256_current.Keys) {
        $target = Join-Path $projectRoot $relative
        if ((Hash-OrNull $target) -ne $audit.protected_package_sha256_current[$relative]) {
            throw "Protected package changed during the V7 runtime probe: $relative"
        }
    }
    $audit.status = 'CORE_RUNTIME_PASS_PENDING_SEPARATE_ACCEPTANCE'
    $audit.scope = 'static_config_file_identity_isolated_gates_and_full_mixed_core_route'
    $audit.v7_runtime_pass = $true
    $audit.mixed_core_report = $reportFile
    $audit.mixed_core_log = $logFile
    $audit.mixed_core_wall_time_s = $runtime.wall_time_s
    $audit.real_keyboard_evidence = $false
    $audit.route_assertions = [ordered]@{
        exact_map_and_config_identity = $true
        physical_mixed_gait_route = $true
        eight_grounded_checkpoints = $true
        two_terminal_bounce_timed_compounds_completed = $true
        hidden_terminal_pits_invisible_and_query_enabled = $true
        moving_platform_speed_varied = $true
        eleven_individually_landed_gaps = $true
        physical_pickup_and_goal_won = $true
        zero_probe_teleports_or_trigger_injections = $true
        checkpoint_recovery_passed = $runtime.checkpoint_recovery_passed
        required_pickup_reacquired_after_recovery = $runtime.required_pickup_reacquired_after_recovery
    }
    $audit.separate_acceptance = @('every_checkpoint_death_and_recovery', 'real_keyboard_and_camera',
        'pause_and_restart',
        'C_Walk_landing_after_timer_disable_is_allowed_and_does_not_make_C_a_Run_only_gate')
    $audit | ConvertTo-Json -Depth 20
    Write-Output "V7 mixed-core physical route PASS: report=$reportFile"
} else {
    $audit | ConvertTo-Json -Depth 20
}
