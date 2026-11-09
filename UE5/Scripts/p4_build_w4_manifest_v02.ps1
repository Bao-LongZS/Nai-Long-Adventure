param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$manifestPath = Join-Path $root 'UE5/SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v01.json'
$outputPath = Join-Path $root 'UE5/SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v02.json'
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$manifest.version = 'v02'
$manifest.created_date = '2026-10-30'
$manifest.status = 'wip_candidate_review'
$manifest | Add-Member -NotePropertyName approved_direction -NotePropertyValue ([ordered]@{
    turnaround = 'boss_nai_tong_turnaround_v02_concept'
    barrel_lid_carry = 'low at hip on strap'
    approval_basis = 'User approved the first-batch art direction before this WIP candidate pass.'
}) -Force
$manifest.global_constraints.concept_only = $false
$manifest.global_constraints.runtime_ready = $false
$manifest.global_constraints.ue_imported = $false
$manifest.global_constraints | Add-Member -NotePropertyName delivery_status -NotePropertyValue 'single-frame WIP candidates; animation, detailed alpha/fringe, fixed-camera and UE integration review pending' -Force
$manifest.global_constraints | Add-Member -NotePropertyName candidate_canvas_px -NotePropertyValue @(512, 512) -Force
$manifest.global_constraints.alpha_edge_qa = 'All candidate PNGs are RGBA 512x512 with four transparent corners and zero visible pixels on the outer canvas border. Full fringe inspection and in-engine review remain pending.'
$manifest | Add-Member -NotePropertyName generation_revision_notes -NotePropertyValue @(
    'Second batch isolates Boss and VFX concepts into transparent RGBA PNG candidates on 512x512 canvases.',
    'Boss charge_attack was regenerated as a grounded forward-driving pose after the first candidate read as a leap; the leap variant is archived under RuntimeWIP/Variants and is excluded from the delivery list.',
    'The source PNGs are flattened image outputs. Layered PSD, Aseprite or other editable authoring projects are not included.'
) -Force
$manifest.generation | Add-Member -NotePropertyName second_batch_prompt_set_summary -NotePropertyValue 'Single isolated full-body Boss state/view keyframes and isolated single-effect samples; transparent alpha; P4 cream, honey oak, brass, coral and mint palette; no HUD, hit rules, field, warning icon or baked gameplay background. Charge attack revision explicitly requests low grounded forward motion with both feet near the ground.' -Force

$bossDirectory = Join-Path $root 'UE5/SourceAssets/P4/Characters/Alpha/W4_Boss/RuntimeWIP'
$fxDirectory = Join-Path $root 'UE5/SourceAssets/P4/Effects/Alpha/W4/RuntimeWIP'

$fxMap = @{
    fx_milk_shot = @{ event_ids = @(); trigger = 'Projectile spawn/render hook; Presenter binding pending.'; notes = 'Player skill effect; orient along projectile travel.' }
    fx_melee_swing = @{ event_ids = @(); trigger = 'Player attack-start hook; Presenter binding pending. Contact impact remains player_melee_hit only.'; notes = 'Player skill effect; never use as a confirmed hit cue.' }
    fx_player_dash_trail = @{ event_ids = @(); trigger = 'Dash-state start/end hook; Presenter binding pending.'; notes = 'Player skill effect; end with dash presentation state.' }
    fx_laugh_burst = @{ event_ids = @(); trigger = 'Laugh action hook; Presenter binding pending.'; notes = 'Player skill effect.' }
    fx_defend_guard = @{ event_ids = @(); trigger = 'Held defend-state presentation hook; Presenter binding pending.'; notes = 'Player skill effect; loop/hold behavior is not authored yet.' }
    fx_player_hit = @{ event_ids = @('player_damaged'); trigger = 'Play when player damage is confirmed.'; notes = 'Do not imply damage during invulnerability.' }
    fx_melee_impact = @{ event_ids = @('player_melee_hit'); trigger = 'Play at confirmed melee contact only.'; notes = 'Never play on player_melee_miss.' }
    fx_dash_impact = @{ event_ids = @('player_dash_hit'); trigger = 'Play at confirmed dash contact only.'; notes = 'Contact cue only; dash trail is a separate effect.' }
    fx_boss_lid_slam = @{ event_ids = @('boss_lid_slam_hit', 'boss_lid_slam_missed'); trigger = 'Play when the lid slam occurs for either result.'; notes = 'At Boss landing point; does not imply player damage.' }
    fx_enemy_hit = @{ event_ids = @('enemy_damaged'); trigger = 'Play when enemy damage is confirmed.'; notes = 'Keep separate from the damage number and hit text.' }
    fx_boss_charge_trail = @{ event_ids = @('boss_charge_hit', 'boss_charge_missed'); trigger = 'Start only when the charge attack presentation actually begins after its warning; end on boss_charge_hit or boss_charge_missed. Charge-state start hook needs engineering confirmation. Do not start on boss_telegraph_charge.'; notes = 'Directional trail/dust candidate.' }
    fx_boss_phase_two = @{ event_ids = @('boss_phase_two'); trigger = 'Play during the 1.35 second phase transition presentation.'; notes = 'No screen flash or long HUD occlusion.' }
    fx_enemy_death = @{ event_ids = @('enemy_killed'); trigger = 'Play on confirmed enemy kill.'; notes = 'One isolated sample; no death sequence yet.' }
}

function Get-RelativeAssetPath {
    param([string]$Path)
    return $Path.Substring($root.Length).TrimStart('\', '/') -replace '\\', '/'
}

function Get-ImageInfo {
    param([string]$Path, [switch]$SkipAlphaEdgeQA)
    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    try {
        $threshold = 8
        $borderVisible = 0
        $cornerAlpha = @()
        if (-not $SkipAlphaEdgeQA) {
            $cornerAlpha = @(
                $bitmap.GetPixel(0, 0).A,
                $bitmap.GetPixel($bitmap.Width - 1, 0).A,
                $bitmap.GetPixel(0, $bitmap.Height - 1).A,
                $bitmap.GetPixel($bitmap.Width - 1, $bitmap.Height - 1).A
            )
            for ($x = 0; $x -lt $bitmap.Width; $x++) {
                if ($bitmap.GetPixel($x, 0).A -gt $threshold) { $borderVisible++ }
                if ($bitmap.GetPixel($x, $bitmap.Height - 1).A -gt $threshold) { $borderVisible++ }
            }
            for ($y = 1; $y -lt ($bitmap.Height - 1); $y++) {
                if ($bitmap.GetPixel(0, $y).A -gt $threshold) { $borderVisible++ }
                if ($bitmap.GetPixel($bitmap.Width - 1, $y).A -gt $threshold) { $borderVisible++ }
            }
        }
        [pscustomobject]@{
            width = $bitmap.Width
            height = $bitmap.Height
            pixel_format = $bitmap.PixelFormat.ToString()
            corner_alpha = $cornerAlpha
            visible_outer_border_pixels = $borderVisible
        }
    }
    finally { $bitmap.Dispose() }
}

function New-CandidateAsset {
    param(
        [System.IO.FileInfo]$CandidateFile,
        [string]$SourceDirectory,
        [string]$Kind
    )
    $sourceName = $CandidateFile.BaseName + '_source.png'
    $sourcePath = Join-Path $SourceDirectory $sourceName
    if (-not (Test-Path -LiteralPath $sourcePath)) { throw "Missing source PNG: $sourcePath" }

    $candidateInfo = Get-ImageInfo -Path $CandidateFile.FullName
    $sourceInfo = Get-ImageInfo -Path $sourcePath -SkipAlphaEdgeQA
    if ($candidateInfo.width -ne 512 -or $candidateInfo.height -ne 512) { throw "Candidate is not 512x512: $($CandidateFile.FullName)" }
    if ($candidateInfo.visible_outer_border_pixels -ne 0) { throw "Visible pixel touches outer border: $($CandidateFile.FullName)" }

    $asset = [ordered]@{
        asset_id = $CandidateFile.BaseName
        type = if ($Kind -eq 'boss') { 'boss_single_keyframe_candidate' } else { 'isolated_vfx_still_candidate' }
        file = Get-RelativeAssetPath -Path $CandidateFile.FullName
        source_file = Get-RelativeAssetPath -Path $sourcePath
        candidate_dimensions_px = @($candidateInfo.width, $candidateInfo.height)
        source_dimensions_px = @($sourceInfo.width, $sourceInfo.height)
        file_size_bytes = $CandidateFile.Length
        source_file_size_bytes = (Get-Item -LiteralPath $sourcePath).Length
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $CandidateFile.FullName).Hash
        source_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourcePath).Hash
        pixel_format = $candidateInfo.pixel_format
        alpha_edge_qa = [ordered]@{
            corner_alpha = $candidateInfo.corner_alpha
            visible_outer_border_pixels = $candidateInfo.visible_outer_border_pixels
            status = 'candidate border transparency checked; detailed fringe review pending'
        }
        frame_index = 0
        sequence_status = 'single still only; no animation frames or playback timing authored'
        runtime_ready = $false
    }

    if ($Kind -eq 'boss') {
        $match = [regex]::Match($CandidateFile.BaseName, '^boss_nai_tong_(?<state>.+)_(?<view>front|back|side_right)_(?<index>\d+)$')
        if (-not $match.Success) { throw "Unexpected Boss keyframe filename: $($CandidateFile.Name)" }
        $state = $match.Groups['state'].Value
        $view = $match.Groups['view'].Value
        $asset.state = $state
        $asset.view = $view
        $asset.frame_index = [int]$match.Groups['index'].Value
        $asset.provisional_foot_anchor_px = @(256, 466)
        $asset.anchor_status = 'estimated preview anchor; calibrate all poses at the fixed camera before freezing'
        $asset.state_timing_seconds = switch ($state) {
            'charge_telegraph' { 0.90; break }
            'lid_slam_telegraph' { 1.05; break }
            'phase_two' { 1.35; break }
            default { $null }
        }
        $asset.timing_status = if ($null -eq $asset.state_timing_seconds) { 'not specified for this still' } else { 'engineering presentation timing reference; not a new gameplay rule' }
        $asset.notes = switch ($state) {
            'charge_attack' { 'Grounded forward-driving keyframe revised from a leap-like candidate; still needs foot contact, direction and three-view animation review.'; break }
            'lid_slam_impact' { 'Character pose only; ground shock is a separate VFX.'; break }
            'death' { 'Single ending pose only; no full defeat/recoil sequence.'; break }
            default { 'Single keyframe candidate only; not a complete animation state.' }
        }
    }
    else {
        $effectId = $CandidateFile.BaseName
        $mapping = $fxMap[$effectId]
        if ($null -eq $mapping) { throw "Missing effect mapping for: $effectId" }
        $asset.event_ids = $mapping.event_ids
        $asset.trigger = $mapping.trigger
        $asset.notes = $mapping.notes
        $asset.provisional_pivot_px = @(256, 256)
        $asset.anchor_status = 'centered canvas default; per-effect emission/contact anchor requires TA review'
    }
    return [pscustomobject]$asset
}

$newAssets = [System.Collections.Generic.List[object]]::new()
Get-ChildItem -LiteralPath (Join-Path $bossDirectory 'Frames512') -File -Filter '*.png' | Sort-Object Name | ForEach-Object {
    $newAssets.Add((New-CandidateAsset -CandidateFile $_ -SourceDirectory (Join-Path $bossDirectory 'SourceFrames') -Kind boss))
}
Get-ChildItem -LiteralPath (Join-Path $fxDirectory 'Frames512') -File -Filter '*.png' | Sort-Object Name | ForEach-Object {
    $newAssets.Add((New-CandidateAsset -CandidateFile $_ -SourceDirectory (Join-Path $fxDirectory 'SourceFrames') -Kind effect))
}

$manifest.assets = @($manifest.assets) + @($newAssets)
$manifest | Add-Member -NotePropertyName second_batch_summary -NotePropertyValue ([ordered]@{
    boss_single_keyframes = @($newAssets | Where-Object { $_.type -eq 'boss_single_keyframe_candidate' }).Count
    boss_views = @('front', 'back', 'side_right')
    boss_state_coverage = @('idle', 'move', 'charge_telegraph', 'charge_attack', 'lid_slam_telegraph', 'lid_slam_impact', 'hit', 'phase_two', 'death')
    boss_states_with_incomplete_views = @('move', 'charge_telegraph', 'charge_attack', 'lid_slam_telegraph', 'lid_slam_impact', 'hit', 'phase_two', 'death')
    isolated_vfx_stills = @($newAssets | Where-Object { $_.type -eq 'isolated_vfx_still_candidate' }).Count
    all_candidates_canvas_px = @(512, 512)
    all_candidates_are_complete_sequences = $false
    all_candidates_runtime_ready = $false
    ue_imported = $false
    required_before_runtime = @('Complete animation/VFX sequences and frame timing', 'Freeze source format, anchors and contact/emission points', 'Full alpha/fringe and fixed-camera review', 'Confirm Presenter hooks for player action start/hold states', 'Engineer-led UE import and P4 viewport/PIE review')
}) -Force
$manifest | Add-Member -NotePropertyName engineering_event_contract -NotePropertyValue ([ordered]@{
    source = 'UE5/Records/p4_alpha/P4_W4_Boss与战斗特效美术要求_2026-10-29.md'
    warning_seconds = [ordered]@{ boss_telegraph_charge = 0.90; boss_telegraph_lid_slam = 1.05 }
    phase_two_seconds = 1.35
    boss_charge_distance_uu = [ordered]@{ normal = 280; phase_two = 340 }
    boss_lid_slam_radius_uu = [ordered]@{ normal = 205; phase_two = 248 }
    rule = 'Art timing only follows the existing simulation contract and never changes hitboxes, damage or event IDs.'
    event_mapping_notes = @('player_melee_miss must not show a confirmed contact impact.', 'fx_boss_lid_slam may play for hit or missed results because the slam occurred, but it is not a player damage signal.', 'fx_boss_charge_trail starts only during the actual charge presentation, not in its telegraph.')
}) -Force
$manifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $outputPath -Encoding utf8
Write-Output "Wrote $outputPath with $($newAssets.Count) second-batch candidates."
