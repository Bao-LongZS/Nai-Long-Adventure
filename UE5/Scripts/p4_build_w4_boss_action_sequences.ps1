param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$bossRoot = Join-Path $root 'UE5/SourceAssets/P4/Characters/Alpha/W4_Boss/RuntimeWIP'
$sourceRoot = Join-Path $bossRoot 'Sequences/SourceFrames'
$frameRoot = Join-Path $bossRoot 'Sequences/Frames512'
$baseManifestPath = Join-Path $root 'UE5/SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v04.json'
$outputManifestPath = Join-Path $root 'UE5/SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v05.json'
$reviewPath = Join-Path $root 'UE5/Records/p4_alpha/evidence/W4/P4_W4_BossActionSequenceReview_v01.html'
$baseManifest = Get-Content -Raw -LiteralPath $baseManifestPath | ConvertFrom-Json
$threshold = 8
$canvas = 512
$footY = 466

$poses = @(
    [ordered]@{ state = 'charge_telegraph'; view = 'side_right'; index = 1; duration = 0.45; event_ids = @('boss_telegraph_charge'); intent = 'late windup / low brace'; source = 'boss_nai_tong_charge_telegraph_side_right_01_source.png'; timing_note = '0.90 second engineering warning split across two poses; frame 00 uses 0.45 s, frame 01 uses 0.45 s.' },
    [ordered]@{ state = 'charge_attack'; view = 'side_right'; index = 1; duration = 0.10; event_ids = @('boss_charge_hit', 'boss_charge_missed'); intent = 'alternate grounded stride'; source = 'boss_nai_tong_charge_attack_side_right_01_source.png'; timing_note = 'Loop the two stride key poses during the actual charge presentation; stop on result event. Event start hook remains to be confirmed.' },
    [ordered]@{ state = 'lid_slam_telegraph'; view = 'front'; index = 1; duration = 0.525; event_ids = @('boss_telegraph_lid_slam'); intent = 'final raised-lid anticipation'; source = 'boss_nai_tong_lid_slam_telegraph_front_01_source.png'; timing_note = '1.05 second engineering warning split across two poses; frame 00 uses 0.525 s, frame 01 uses 0.525 s.' },
    [ordered]@{ state = 'lid_slam_impact'; view = 'front'; index = 1; duration = 0.12; event_ids = @('boss_lid_slam_hit', 'boss_lid_slam_missed'); intent = 'low follow-through after lid strike'; source = 'boss_nai_tong_lid_slam_impact_front_01_source.png'; timing_note = 'The ground shock is a separate VFX. This pose follows the impact pose and carries no damage rule.' },
    [ordered]@{ state = 'move'; view = 'side_right'; index = 1; duration = 0.12; event_ids = @(); intent = 'alternate ordinary pursuit step'; source = 'boss_nai_tong_move_side_right_01_source.png'; timing_note = 'Two-pose ordinary movement loop preview; lower speed than the charge stride.' },
    [ordered]@{ state = 'hit'; view = 'front'; index = 1; duration = 0.10; event_ids = @(); intent = 'recoil recovery'; source = 'boss_nai_tong_hit_front_01_source.png'; timing_note = 'Short hit/recover pose proposal; exact duration follows the existing Boss presentation hook.' },
    [ordered]@{ state = 'phase_two'; view = 'front'; index = 1; duration = 0.675; event_ids = @('boss_phase_two'); intent = 'strengthened stance after transition'; source = 'boss_nai_tong_phase_two_front_01_source.png'; timing_note = '1.35 second engineering phase transition split across two poses; VFX remains separate.' },
    [ordered]@{ state = 'death'; view = 'front'; index = 1; duration = 0.18; event_ids = @(); intent = 'defeated collapse follow-through'; source = 'boss_nai_tong_death_front_01_source.png'; timing_note = 'Defeat ending duration is a visual proposal; confirm the Boss death presentation hook with engineering.' }
)

function Get-RelativePath {
    param([string]$Path)
    return $Path.Substring($root.Length).TrimStart([char[]]@('\', '/')) -replace '\\', '/'
}

function Get-VisibleBounds {
    param([System.Drawing.Bitmap]$Bitmap, [int]$Threshold = 8)
    $left = $Bitmap.Width; $top = $Bitmap.Height; $right = -1; $bottom = -1
    for ($y = 0; $y -lt $Bitmap.Height; $y++) {
        for ($x = 0; $x -lt $Bitmap.Width; $x++) {
            if ($Bitmap.GetPixel($x, $y).A -gt $Threshold) {
                if ($x -lt $left) { $left = $x }
                if ($x -gt $right) { $right = $x }
                if ($y -lt $top) { $top = $y }
                if ($y -gt $bottom) { $bottom = $y }
            }
        }
    }
    if ($right -lt $left) { throw 'Boss action source has no visible pixels.' }
    return [System.Drawing.Rectangle]::new($left, $top, ($right - $left + 1), ($bottom - $top + 1))
}

function Export-BossActionFrame {
    param([string]$SourcePath, [string]$DestinationPath)
    $source = [System.Drawing.Bitmap]::FromFile($SourcePath)
    $output = [System.Drawing.Bitmap]::new($canvas, $canvas, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($output)
    try {
        $bounds = Get-VisibleBounds -Bitmap $source -Threshold $threshold
        $scale = [Math]::Min(440.0 / $bounds.Width, ($footY - 24.0) / $bounds.Height)
        $width = [Math]::Max(1, [int][Math]::Round($bounds.Width * $scale))
        $height = [Math]::Max(1, [int][Math]::Round($bounds.Height * $scale))
        $left = [int][Math]::Round(($canvas - $width) / 2.0)
        $top = $footY - $height
        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        $graphics.DrawImage($source, [System.Drawing.Rectangle]::new($left, $top, $width, $height), $bounds, [System.Drawing.GraphicsUnit]::Pixel)
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $DestinationPath) | Out-Null
        $output.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
        return [pscustomobject]@{ visible_bounds_px = @($bounds.X, $bounds.Y, $bounds.Width, $bounds.Height); output_bounds_px = @($left, $top, $width, $height); scale = $scale }
    }
    finally { $graphics.Dispose(); $output.Dispose(); $source.Dispose() }
}

New-Item -ItemType Directory -Force -Path $frameRoot | Out-Null
$newAssets = [System.Collections.Generic.List[object]]::new()
$sequences = [System.Collections.Generic.List[object]]::new()

foreach ($pose in $poses) {
    $sourcePath = Join-Path $sourceRoot $pose.source
    if (-not (Test-Path -LiteralPath $sourcePath)) { throw "Missing generated action source: $sourcePath" }
    $newName = $pose.source -replace '_source\.png$', '.png'
    $destinationPath = Join-Path $frameRoot $newName
    $render = Export-BossActionFrame -SourcePath $sourcePath -DestinationPath $destinationPath
    $bitmap = [System.Drawing.Bitmap]::FromFile($destinationPath)
    try {
        $corners = @($bitmap.GetPixel(0, 0).A, $bitmap.GetPixel(511, 0).A, $bitmap.GetPixel(0, 511).A, $bitmap.GetPixel(511, 511).A)
        $outerVisible = 0
        for ($edge = 0; $edge -lt 512; $edge++) {
            if ($bitmap.GetPixel($edge, 0).A -gt $threshold) { $outerVisible++ }
            if ($bitmap.GetPixel($edge, 511).A -gt $threshold) { $outerVisible++ }
            if ($bitmap.GetPixel(0, $edge).A -gt $threshold) { $outerVisible++ }
            if ($bitmap.GetPixel(511, $edge).A -gt $threshold) { $outerVisible++ }
        }
        if ((@($corners | Where-Object { $_ -gt $threshold }).Count -gt 0) -or $outerVisible -ne 0) { throw "Alpha edge check failed: $newName" }
        $visibleBottom = -1
        for ($y = 511; $y -ge 0 -and $visibleBottom -lt 0; $y--) {
            for ($x = 0; $x -lt 512; $x++) { if ($bitmap.GetPixel($x, $y).A -gt 32) { $visibleBottom = $y; break } }
        }
        if ($visibleBottom -ne 465) { throw "Foot anchor check failed: $newName bottom=$visibleBottom" }
    }
    finally { $bitmap.Dispose() }

    $frameStem = [System.IO.Path]::GetFileNameWithoutExtension($newName)
    $baseId = $frameStem -replace '_01$', '_00'
    $baseAsset = $baseManifest.assets | Where-Object { $_.asset_id -eq $baseId } | Select-Object -First 1
    if ($null -eq $baseAsset) { throw "Missing initial sequence frame in base manifest: $baseId" }
    $sequenceId = 'boss_nai_tong_{0}_{1}_sequence_v01' -f $pose.state, $pose.view
    $newAsset = [pscustomobject][ordered]@{
        asset_id = $frameStem
        type = 'boss_action_sequence_frame_candidate'
        sequence_id = $sequenceId
        state = $pose.state
        view = $pose.view
        frame_index = [int]$pose.index
        frame_duration_seconds = [double]$pose.duration
        provisional_foot_anchor_px = @(256, $footY)
        anchor_status = 'common fixed-camera candidate anchor; verify in engine'
        file = Get-RelativePath -Path $destinationPath
        source_file = Get-RelativePath -Path $sourcePath
        source_visible_bounds_px = $render.visible_bounds_px
        output_visible_bounds_px = $render.output_bounds_px
        candidate_dimensions_px = @($canvas, $canvas)
        corner_alpha = @($corners)
        visible_outer_border_pixels = $outerVisible
        file_size_bytes = (Get-Item -LiteralPath $destinationPath).Length
        source_file_size_bytes = (Get-Item -LiteralPath $sourcePath).Length
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $destinationPath).Hash
        source_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourcePath).Hash
        generation_tool = 'built-in image_gen; prompted as an adjacent animation pose using the accepted frame as reference'
        pose_intent = $pose.intent
        runtime_ready = $false
    }
    $newAssets.Add($newAsset)

    $frame00Path = Get-RelativePath -Path (Join-Path (Join-Path $bossRoot 'Frames512') ($baseId + '.png'))
    $frame01Path = Get-RelativePath -Path $destinationPath
    $frame00Seconds = switch ($pose.state) {
        'charge_telegraph' { 0.45; break }
        'lid_slam_telegraph' { 0.525; break }
        'charge_attack' { 0.10; break }
        'lid_slam_impact' { 0.12; break }
        'move' { 0.12; break }
        'hit' { 0.10; break }
        'phase_two' { 0.675; break }
        'death' { 0.18; break }
    }
    $sequenceFiles = @($frame00Path, $frame01Path)
    $frameDurations = @([double]$frame00Seconds, [double]$pose.duration)
    $playback = switch ($pose.state) {
        'charge_attack' { 'loop_until_result_event'; break }
        'move' { 'loop'; break }
        default { 'once' }
    }
    $cycleDuration = [Math]::Round(($frameDurations[0] + $frameDurations[1]), 4)
    $totalDuration = if ($playback -in @('once', 'loop')) { $cycleDuration } else { $null }
    $sequences.Add([pscustomobject][ordered]@{
        sequence_id = $sequenceId
        character_id = 'boss_nai_tong'
        state = $pose.state
        view = $pose.view
        frame_files = $sequenceFiles
        frame_indices = @(0, 1)
        frame_durations_seconds = $frameDurations
        frame_count = 2
        playback_mode = $playback
        proposed_total_duration_seconds = $totalDuration
        proposed_cycle_duration_seconds = $cycleDuration
        event_ids = @($pose.event_ids)
        source_frame_00_asset_id = $baseId
        source_frame_01_asset_id = $frameStem
        anchor_px = @(256, $footY)
        pose_intents = @('approved state key pose', $pose.intent)
        timing_note = $pose.timing_note
        runtime_ready = $false
    })
}

$manifest = Get-Content -Raw -LiteralPath $baseManifestPath | ConvertFrom-Json
$manifest.version = 'v05'
$manifest.created_date = '2026-10-30'
$manifest.status = 'wip_boss_action_keypose_sequences'
$manifest.assets = @($manifest.assets) + @($newAssets.ToArray())
$manifest.global_constraints.runtime_ready = $false
$manifest.global_constraints.ue_imported = $false
$manifest.global_constraints | Add-Member -NotePropertyName delivery_status -NotePropertyValue 'P4 W4 has Boss idle transform loops, four two-pose action candidates and VFX transform sequences. Complete Boss animation redraw, per-frame QA, fixed-camera review and UE integration remain pending.' -Force
$manifest | Add-Member -NotePropertyName boss_action_sequence_batch -NotePropertyValue ([ordered]@{
    status = 'adjacent_keypose_sequence_wip'
    sequence_count = $sequences.Count
    new_generated_frames = $newAssets.Count
    total_boss_state_count = 9
    states_with_two_pose_sequences = @('move', 'charge_telegraph', 'charge_attack', 'lid_slam_telegraph', 'lid_slam_impact', 'hit', 'phase_two', 'death')
    other_states = @('idle has a separate three-view transform loop; its body shape is not hand-animated')
    anchor_px = @(256, $footY)
    image_generation_note = 'Eight additional adjacent Boss pose stills were generated with the selected low-hanging lid design as reference, then fit to the 512 canvas with the foot baseline locked.'
    sequences = @($sequences.ToArray())
}) -Force
$manifest | Add-Member -NotePropertyName boss_action_review_page -NotePropertyValue 'UE5/Records/p4_alpha/evidence/W4/P4_W4_BossActionSequenceReview_v01.html' -Force
$manifest | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $outputManifestPath -Encoding utf8

$html = [System.Text.StringBuilder]::new()
[void]$html.AppendLine('<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>P4 W4 Boss 攻击姿势序列候选</title><style>body{margin:0;padding:28px;background:#f1f4f4;color:#26323a;font:14px/1.55 "Segoe UI","Microsoft YaHei",sans-serif}main{max-width:1280px;margin:auto}h1{font-size:28px;margin:0 0 6px}.muted{color:#63717a}.warn{padding:13px 16px;background:#fff7e7;border-left:5px solid #c99a4a;border-radius:8px;margin:18px 0 24px}.row{background:#fff;border:1px solid #d7dfe2;border-radius:12px;margin:16px 0;padding:14px}.frames{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}.cell{padding:10px;border:1px solid #e2e6e7;border-radius:8px}.board{height:330px;display:grid;place-items:center;background-color:#fff;background-image:linear-gradient(45deg,#e8ebec 25%,transparent 25%),linear-gradient(-45deg,#e8ebec 25%,transparent 25%),linear-gradient(45deg,transparent 75%,#e8ebec 75%),linear-gradient(-45deg,transparent 75%,#e8ebec 75%);background-size:20px 20px;background-position:0 0,0 10px,10px -10px,-10px 0}.board img{max-width:100%;max-height:100%;object-fit:contain}.cap{font:12px Consolas,monospace;overflow-wrap:anywhere}.pill{display:inline-block;background:#f1e7d2;color:#684a16;border-radius:999px;padding:2px 9px;margin-right:5px;font-size:12px}@media(max-width:720px){.frames{grid-template-columns:1fr}.board{height:280px}}</style></head><body><main>')
[void]$html.AppendLine('<h1>P4 W4 Boss 攻击姿势序列候选</h1><p class="muted">v01 · 2026-10-30 · 桶盖低挂髋侧设计 · 8 组双姿势对拍样本</p>')
[void]$html.AppendLine('<div class="warn"><strong>候选说明：</strong>八个非待机状态各有两张相邻姿势，用于检查方向、脚底和事件时间。`move` 为普通移动循环，`charge_attack` 循环到命中/落空；其余状态一次播放。当前仍为双姿势候选，动作中间帧、完整时序、三向一致性和美术逐帧修整待制作。落地冲击 VFX 保持独立。</div>')
foreach ($sequence in $sequences) {
    [void]$html.AppendLine('<section class="row"><h2>' + $sequence.state + ' · ' + $sequence.view + '</h2><p><span class="pill">' + $sequence.playback_mode + '</span><span class="pill">事件：' + ($sequence.event_ids -join ', ') + '</span><span class="pill">' + $sequence.timing_note + '</span></p><div class="frames">')
    for ($i = 0; $i -lt $sequence.frame_files.Count; $i++) {
        $file = $sequence.frame_files[$i]
        $leaf = [IO.Path]::GetFileName($file)
        [void]$html.AppendLine('<div class="cell"><div class="board"><img src="../../../../../' + $file + '" alt="' + $leaf + '"></div><p class="cap">frame ' + $sequence.frame_indices[$i] + ' · ' + $sequence.frame_durations_seconds[$i] + ' s · ' + $leaf + '</p></div>')
    }
    [void]$html.AppendLine('</div></section>')
}
[void]$html.AppendLine('<p class="muted">姿势和播放候选的逐帧文件、哈希、锚点及事件映射见 <a href="../../../../SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v05.json">manifest v05</a>。VFX 序列见 <a href="P4_W4_VFXSequenceReview_v01.html">特效评审页</a>。</p></main></body></html>')
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $reviewPath) | Out-Null
[System.IO.File]::WriteAllText($reviewPath, $html.ToString(), [System.Text.UTF8Encoding]::new($false))
Write-Output "Wrote $($sequences.Count) Boss action pose sequences, manifest v05, and review page."
