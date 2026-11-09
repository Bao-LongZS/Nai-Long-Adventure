param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$fxRoot = Join-Path $root 'UE5/SourceAssets/P4/Effects/Alpha/W4'
$candidateRoot = Join-Path $fxRoot 'RuntimeWIP/Frames512'
$sequenceRoot = Join-Path $fxRoot 'RuntimeWIP/Sequences'
$frameRoot = Join-Path $sequenceRoot 'Frames512'
$baseManifestPath = Join-Path $root 'UE5/SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v02.json'
$outputManifestPath = Join-Path $root 'UE5/SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v03.json'
$reviewPath = Join-Path $root 'UE5/Records/p4_alpha/evidence/W4/P4_W4_VFXSequenceReview_v01.html'
$baseManifest = Get-Content -Raw -LiteralPath $baseManifestPath | ConvertFrom-Json

# Each sequence is a first-pass motion treatment of an approved isolated still.
# Shape redraw between frames is intentionally not implied by this package.
$plans = [ordered]@{
    fx_milk_shot = [ordered]@{ frame_duration_seconds = 0.075; playback_mode = 'loop'; transforms = @(
        @{ scale_x = 0.90; scale_y = 0.95; rotation_degrees = 0; opacity = 0.72; offset_x = 0; offset_y = 0 },
        @{ scale_x = 0.97; scale_y = 1.00; rotation_degrees = 0; opacity = 0.90; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.02; scale_y = 1.05; rotation_degrees = 0; opacity = 1.00; offset_x = 0; offset_y = 0 },
        @{ scale_x = 0.96; scale_y = 0.99; rotation_degrees = 0; opacity = 0.82; offset_x = 0; offset_y = 0 }
    ) }
    fx_melee_swing = [ordered]@{ frame_duration_seconds = 0.055; playback_mode = 'once'; transforms = @(
        @{ scale_x = 0.72; scale_y = 0.76; rotation_degrees = 0; opacity = 0.25; offset_x = -12; offset_y = 12 },
        @{ scale_x = 0.86; scale_y = 0.90; rotation_degrees = 0; opacity = 0.62; offset_x = -5; offset_y = 5 },
        @{ scale_x = 1.00; scale_y = 1.00; rotation_degrees = 0; opacity = 1.00; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.08; scale_y = 1.06; rotation_degrees = 0; opacity = 0.65; offset_x = 5; offset_y = -4 },
        @{ scale_x = 1.13; scale_y = 1.10; rotation_degrees = 0; opacity = 0.16; offset_x = 10; offset_y = -7 }
    ) }
    fx_player_dash_trail = [ordered]@{ frame_duration_seconds = 0.05; playback_mode = 'once'; transforms = @(
        @{ scale_x = 0.88; scale_y = 0.90; rotation_degrees = 0; opacity = 1.00; offset_x = 18; offset_y = 0 },
        @{ scale_x = 0.96; scale_y = 0.95; rotation_degrees = 0; opacity = 0.84; offset_x = 9; offset_y = 0 },
        @{ scale_x = 1.04; scale_y = 1.00; rotation_degrees = 0; opacity = 0.66; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.12; scale_y = 1.03; rotation_degrees = 0; opacity = 0.43; offset_x = -8; offset_y = 0 },
        @{ scale_x = 1.18; scale_y = 1.06; rotation_degrees = 0; opacity = 0.20; offset_x = -15; offset_y = 0 }
    ) }
    fx_laugh_burst = [ordered]@{ frame_duration_seconds = 0.07; playback_mode = 'once'; transforms = @(
        @{ scale_x = 0.52; scale_y = 0.52; rotation_degrees = 0; opacity = 0.32; offset_x = 0; offset_y = 0 },
        @{ scale_x = 0.72; scale_y = 0.72; rotation_degrees = 0; opacity = 0.72; offset_x = 0; offset_y = 0 },
        @{ scale_x = 0.94; scale_y = 0.94; rotation_degrees = 0; opacity = 1.00; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.14; scale_y = 1.14; rotation_degrees = 0; opacity = 0.66; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.30; scale_y = 1.30; rotation_degrees = 0; opacity = 0.20; offset_x = 0; offset_y = 0 }
    ) }
    fx_defend_guard = [ordered]@{ frame_duration_seconds = 0.15; playback_mode = 'loop'; transforms = @(
        @{ scale_x = 0.96; scale_y = 0.96; rotation_degrees = 0; opacity = 0.42; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.00; scale_y = 1.00; rotation_degrees = 0; opacity = 0.54; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.04; scale_y = 1.04; rotation_degrees = 0; opacity = 0.58; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.00; scale_y = 1.00; rotation_degrees = 0; opacity = 0.46; offset_x = 0; offset_y = 0 }
    ) }
    fx_player_hit = [ordered]@{ frame_duration_seconds = 0.04; playback_mode = 'once'; transforms = @(
        @{ scale_x = 0.80; scale_y = 0.80; rotation_degrees = 0; opacity = 0.30; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.02; scale_y = 1.02; rotation_degrees = 0; opacity = 1.00; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.08; scale_y = 1.08; rotation_degrees = 0; opacity = 0.62; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.12; scale_y = 1.12; rotation_degrees = 0; opacity = 0.12; offset_x = 0; offset_y = 0 }
    ) }
    fx_melee_impact = [ordered]@{ frame_duration_seconds = 0.035; playback_mode = 'once'; transforms = @(
        @{ scale_x = 0.50; scale_y = 0.50; rotation_degrees = 0; opacity = 0.24; offset_x = 0; offset_y = 0 },
        @{ scale_x = 0.82; scale_y = 0.82; rotation_degrees = 0; opacity = 1.00; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.12; scale_y = 1.12; rotation_degrees = 0; opacity = 0.66; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.34; scale_y = 1.34; rotation_degrees = 0; opacity = 0.10; offset_x = 0; offset_y = 0 }
    ) }
    fx_dash_impact = [ordered]@{ frame_duration_seconds = 0.04; playback_mode = 'once'; transforms = @(
        @{ scale_x = 0.55; scale_y = 0.55; rotation_degrees = 0; opacity = 0.24; offset_x = -8; offset_y = 0 },
        @{ scale_x = 0.86; scale_y = 0.86; rotation_degrees = 0; opacity = 1.00; offset_x = -3; offset_y = 0 },
        @{ scale_x = 1.14; scale_y = 1.14; rotation_degrees = 0; opacity = 0.65; offset_x = 3; offset_y = 0 },
        @{ scale_x = 1.34; scale_y = 1.34; rotation_degrees = 0; opacity = 0.10; offset_x = 8; offset_y = 0 }
    ) }
    fx_enemy_hit = [ordered]@{ frame_duration_seconds = 0.04; playback_mode = 'once'; transforms = @(
        @{ scale_x = 0.72; scale_y = 0.72; rotation_degrees = 0; opacity = 0.38; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.04; scale_y = 1.04; rotation_degrees = 0; opacity = 1.00; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.22; scale_y = 1.22; rotation_degrees = 0; opacity = 0.12; offset_x = 0; offset_y = 0 }
    ) }
    fx_boss_lid_slam = [ordered]@{ frame_duration_seconds = 0.07; playback_mode = 'once'; transforms = @(
        @{ scale_x = 0.48; scale_y = 0.48; rotation_degrees = 0; opacity = 0.24; offset_x = 0; offset_y = 0 },
        @{ scale_x = 0.80; scale_y = 0.80; rotation_degrees = 0; opacity = 1.00; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.16; scale_y = 1.16; rotation_degrees = 0; opacity = 0.66; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.44; scale_y = 1.44; rotation_degrees = 0; opacity = 0.10; offset_x = 0; offset_y = 0 }
    ) }
    fx_boss_charge_trail = [ordered]@{ frame_duration_seconds = 0.045; playback_mode = 'once'; transforms = @(
        @{ scale_x = 0.82; scale_y = 0.92; rotation_degrees = 0; opacity = 1.00; offset_x = 18; offset_y = 0 },
        @{ scale_x = 0.94; scale_y = 0.96; rotation_degrees = 0; opacity = 0.78; offset_x = 9; offset_y = 0 },
        @{ scale_x = 1.04; scale_y = 1.00; rotation_degrees = 0; opacity = 0.56; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.12; scale_y = 1.04; rotation_degrees = 0; opacity = 0.34; offset_x = -9; offset_y = 0 },
        @{ scale_x = 1.18; scale_y = 1.06; rotation_degrees = 0; opacity = 0.16; offset_x = -16; offset_y = 0 }
    ) }
    fx_boss_phase_two = [ordered]@{ frame_duration_seconds = 0.27; playback_mode = 'once'; transforms = @(
        @{ scale_x = 0.50; scale_y = 0.50; rotation_degrees = 0; opacity = 0.22; offset_x = 0; offset_y = 0 },
        @{ scale_x = 0.72; scale_y = 0.72; rotation_degrees = 0; opacity = 0.52; offset_x = 0; offset_y = 0 },
        @{ scale_x = 0.96; scale_y = 0.96; rotation_degrees = 0; opacity = 0.82; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.18; scale_y = 1.18; rotation_degrees = 0; opacity = 0.56; offset_x = 0; offset_y = 0 },
        @{ scale_x = 1.34; scale_y = 1.34; rotation_degrees = 0; opacity = 0.16; offset_x = 0; offset_y = 0 }
    ) }
    fx_enemy_death = [ordered]@{ frame_duration_seconds = 0.10; playback_mode = 'once'; transforms = @(
        @{ scale_x = 0.98; scale_y = 0.98; rotation_degrees = 0; opacity = 1.00; offset_x = 0; offset_y = 0 },
        @{ scale_x = 0.90; scale_y = 0.90; rotation_degrees = 0; opacity = 0.82; offset_x = 0; offset_y = -5 },
        @{ scale_x = 0.76; scale_y = 0.76; rotation_degrees = 0; opacity = 0.58; offset_x = 0; offset_y = -14 },
        @{ scale_x = 0.58; scale_y = 0.58; rotation_degrees = 0; opacity = 0.34; offset_x = 0; offset_y = -24 },
        @{ scale_x = 0.38; scale_y = 0.38; rotation_degrees = 0; opacity = 0.12; offset_x = 0; offset_y = -34 }
    ) }
}

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
    if ($right -lt $left) { throw 'Effect still has no visible alpha pixels.' }
    return [System.Drawing.Rectangle]::new($left, $top, ($right - $left + 1), ($bottom - $top + 1))
}

function Export-TransformedFrame {
    param(
        [string]$SourcePath,
        [string]$DestinationPath,
        [hashtable]$Transform,
        [int]$CanvasSize = 512,
        [double]$BaseFit = 0.70
    )
    $source = [System.Drawing.Bitmap]::FromFile($SourcePath)
    $canvas = [System.Drawing.Bitmap]::new($CanvasSize, $CanvasSize, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($canvas)
    $scaled = $null
    $scaleGraphics = $null
    $attributes = [System.Drawing.Imaging.ImageAttributes]::new()
    try {
        $bounds = Get-VisibleBounds -Bitmap $source
        $longest = [double][Math]::Max($bounds.Width, $bounds.Height)
        $fit = ($CanvasSize * $BaseFit) / $longest
        $destWidth = [Math]::Max(1, [int][Math]::Round($bounds.Width * $fit * [double]$Transform.scale_x))
        $destHeight = [Math]::Max(1, [int][Math]::Round($bounds.Height * $fit * [double]$Transform.scale_y))
        $scaleX = [double]$Transform.scale_x
        $scaleY = [double]$Transform.scale_y
        $matrix = [System.Drawing.Imaging.ColorMatrix]::new()
        $matrix.Matrix33 = [single]$Transform.opacity
        $attributes.SetColorMatrix($matrix, [System.Drawing.Imaging.ColorMatrixFlag]::Default, [System.Drawing.Imaging.ColorAdjustType]::Bitmap)
        $scaled = [System.Drawing.Bitmap]::new($destWidth, $destHeight, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $scaleGraphics = [System.Drawing.Graphics]::FromImage($scaled)
        $scaleGraphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $scaleGraphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $scaleGraphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $scaleGraphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $scaleGraphics.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        $scaleGraphics.DrawImage($source, [System.Drawing.Rectangle]::new(0, 0, $destWidth, $destHeight), $bounds.X, $bounds.Y, $bounds.Width, $bounds.Height, [System.Drawing.GraphicsUnit]::Pixel, $attributes)

        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        $drawX = [int][Math]::Round(($CanvasSize - $destWidth) / 2.0 + [double]$Transform.offset_x)
        $drawY = [int][Math]::Round(($CanvasSize - $destHeight) / 2.0 + [double]$Transform.offset_y)
        $graphics.DrawImage($scaled, $drawX, $drawY, $destWidth, $destHeight)
        $parent = Split-Path -Parent $DestinationPath
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
        $canvas.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
        return [pscustomobject]@{ bounds_px = @($bounds.X, $bounds.Y, $bounds.Width, $bounds.Height); dimensions_px = @($CanvasSize, $CanvasSize) }
    }
    finally {
        $attributes.Dispose()
        if ($scaleGraphics) { $scaleGraphics.Dispose() }
        if ($scaled) { $scaled.Dispose() }
        $graphics.Dispose()
        $canvas.Dispose()
        $source.Dispose()
    }
}

New-Item -ItemType Directory -Force -Path $frameRoot | Out-Null
$sequences = [System.Collections.Generic.List[object]]::new()
$frameAssets = [System.Collections.Generic.List[object]]::new()
$threshold = 8

foreach ($effectId in $plans.Keys) {
    $plan = $plans[$effectId]
    $sourceCandidate = Join-Path $candidateRoot ($effectId + '.png')
    if (-not (Test-Path -LiteralPath $sourceCandidate)) { throw "Missing approved still candidate: $sourceCandidate" }
    $baseAsset = $baseManifest.assets | Where-Object { $_.asset_id -eq $effectId } | Select-Object -First 1
    if ($null -eq $baseAsset) { throw "Missing base manifest entry: $effectId" }

    $sequenceFiles = [System.Collections.Generic.List[string]]::new()
    $sequenceFrameAssets = [System.Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt $plan.transforms.Count; $index++) {
        $frameName = '{0}_{1:D2}.png' -f $effectId, $index
        $destinationPath = Join-Path $frameRoot $frameName
        $transform = $plan.transforms[$index]
        $renderInfo = Export-TransformedFrame -SourcePath $sourceCandidate -DestinationPath $destinationPath -Transform $transform
        $bitmap = [System.Drawing.Bitmap]::FromFile($destinationPath)
        try {
            $cornerAlpha = @($bitmap.GetPixel(0, 0).A, $bitmap.GetPixel(511, 0).A, $bitmap.GetPixel(0, 511).A, $bitmap.GetPixel(511, 511).A)
            $borderVisible = 0
            for ($edge = 0; $edge -lt 512; $edge++) {
                if ($bitmap.GetPixel($edge, 0).A -gt $threshold) { $borderVisible++ }
                if ($bitmap.GetPixel($edge, 511).A -gt $threshold) { $borderVisible++ }
                if ($bitmap.GetPixel(0, $edge).A -gt $threshold) { $borderVisible++ }
                if ($bitmap.GetPixel(511, $edge).A -gt $threshold) { $borderVisible++ }
            }
            if (($cornerAlpha -join ',') -ne '0,0,0,0' -or $borderVisible -ne 0) { throw "Alpha-edge check failed: $frameName" }
        }
        finally { $bitmap.Dispose() }

        $relativeFile = Get-RelativePath -Path $destinationPath
        $sequenceFiles.Add($relativeFile)
        $candidateHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $destinationPath).Hash
        $frameEntry = [pscustomobject][ordered]@{
            asset_id = ($effectId + '_frame_' + ('{0:D2}' -f $index))
            type = 'transform_animated_vfx_frame_candidate'
            sequence_id = ($effectId + '_sequence_v01')
            effect_id = $effectId
            file = $relativeFile
            source_file = Get-RelativePath -Path $sourceCandidate
            high_resolution_source_file = $baseAsset.source_file
            dimensions_px = @(512, 512)
            frame_index = $index
            frame_duration_seconds = [double]$plan.frame_duration_seconds
            playback_mode = $plan.playback_mode
            provisional_pivot_px = @(256, 256)
            transform_only = $true
            frame_transform = [ordered]@{
                scale_x = [double]$transform.scale_x
                scale_y = [double]$transform.scale_y
                rotation_degrees = 0.0
                opacity = [double]$transform.opacity
                offset_px = @([double]$transform.offset_x, [double]$transform.offset_y)
                source_visible_bounds_px = $renderInfo.bounds_px
            }
            corner_alpha = @($cornerAlpha)
            visible_outer_border_pixels = $borderVisible
            file_size_bytes = (Get-Item -LiteralPath $destinationPath).Length
            sha256 = $candidateHash
            runtime_ready = $false
        }
        $sequenceFrameAssets.Add($frameEntry)
        $frameAssets.Add($frameEntry)
    }

    $sequences.Add([pscustomobject][ordered]@{
        sequence_id = ($effectId + '_sequence_v01')
        effect_id = $effectId
        frame_files = @($sequenceFiles.ToArray())
        frame_count = $plan.transforms.Count
        frame_duration_seconds = [double]$plan.frame_duration_seconds
        sequence_duration_seconds = [Math]::Round(($plan.frame_duration_seconds * $plan.transforms.Count), 4)
        playback_mode = $plan.playback_mode
        event_ids = @($baseAsset.event_ids)
        trigger = $baseAsset.trigger
        source_still = Get-RelativePath -Path $sourceCandidate
        frame_generation = 'Non-destructive transform variants from the approved isolated still: scale, opacity, rotation and small pivot offsets only; no per-frame shape redraw.'
        status = 'timing_preview_candidate; shape animation and gameplay review pending'
        runtime_ready = $false
    })
}

$manifest = Get-Content -Raw -LiteralPath $baseManifestPath | ConvertFrom-Json
$manifest.version = 'v03'
$manifest.created_date = '2026-10-30'
$manifest.status = 'wip_vfx_transform_sequences'
$manifest.global_constraints.runtime_ready = $false
$manifest.global_constraints.ue_imported = $false
$manifest.global_constraints | Add-Member -NotePropertyName delivery_status -NotePropertyValue 'Static candidate sheets plus first-pass transform-only VFX sequences; per-frame silhouette redraw, full alpha fringe review, fixed-camera review, presenter hooks and UE integration remain pending.' -Force
$manifest.assets = @($manifest.assets) + @($frameAssets.ToArray())
$manifest | Add-Member -NotePropertyName vfx_sequence_batch -NotePropertyValue ([ordered]@{
    status = 'transform_timing_preview_wip'
    source_manifest = 'UE5/SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v02.json'
    sequence_root = 'UE5/SourceAssets/P4/Effects/Alpha/W4/RuntimeWIP/Sequences/Frames512/'
    sequence_count = $sequences.Count
    total_frame_count = $frameAssets.Count
    canvas_px = @(512, 512)
    nominal_frame_rate = 'per-frame durations are authoritative; sequences use variable display rates'
    motion_source = 'source stills transformed by scale, opacity and small pivot offsets only; rotation was removed after alpha-edge QA detected clipping'
    editable_layered_authoring_source = $false
    low_intensity_fallback = 'Fade/opacity frames are provided in each sequence; dedicated hand-redrawn accessibility variants remain pending.'
    sequences = @($sequences.ToArray())
}) -Force
$manifest | Add-Member -NotePropertyName vfx_sequence_review_page -NotePropertyValue 'UE5/Records/p4_alpha/evidence/W4/P4_W4_VFXSequenceReview_v01.html' -Force
$manifest | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath $outputManifestPath -Encoding utf8

$html = [System.Text.StringBuilder]::new()
[void]$html.AppendLine('<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>P4 W4 VFX 序列候选 v01</title>')
[void]$html.AppendLine('<style>body{margin:0;padding:28px;background:#f1f4f4;color:#26323a;font:14px/1.5 "Segoe UI","Microsoft YaHei",sans-serif}main{max-width:1400px;margin:auto}h1{margin:0 0 6px;font-size:28px}h2{margin:0 0 12px}.note{color:#63717a}.warn{padding:12px 16px;background:#fff7e7;border-left:5px solid #c99a4a;border-radius:8px;margin:18px 0 24px}.group{background:#fff;border:1px solid #d7dfe2;border-radius:12px;padding:14px;margin:16px 0}.frames{display:grid;grid-template-columns:repeat(5,minmax(0,1fr));gap:10px}.cell{min-width:0;padding:8px;border:1px solid #e2e6e7;border-radius:8px}.board{height:190px;display:grid;place-items:center;background-color:#fff;background-image:linear-gradient(45deg,#e8ebec 25%,transparent 25%),linear-gradient(-45deg,#e8ebec 25%,transparent 25%),linear-gradient(45deg,transparent 75%,#e8ebec 75%),linear-gradient(-45deg,transparent 75%,#e8ebec 75%);background-size:20px 20px;background-position:0 0,0 10px,10px -10px,-10px 0}.board img{max-width:100%;max-height:100%;object-fit:contain}.caption{font:11px Consolas,monospace;overflow-wrap:anywhere;color:#59666d}.small{color:#63717a;font-size:12px}@media(max-width:900px){.frames{grid-template-columns:repeat(3,1fr)}}@media(max-width:600px){.frames{grid-template-columns:repeat(2,1fr)}}</style></head><body><main>')
[void]$html.AppendLine('<h1>P4 W4 特效首轮播放序列候选</h1><p class="note">v01 · 2026-10-30 · 主角技能特效与 Boss / 战斗反馈</p>')
[void]$html.AppendLine('<div class="warn"><strong>动画来源：</strong>当前帧由各自已评审的独立特效静帧变换生成，仅包含缩放、透明度和少量位移，不含逐帧重绘或旋转。可用于节奏、锚点和画面占比预览；生产前仍需补形变、低闪烁版本与实际机位验收。</div>')
foreach ($sequence in $sequences) {
    $seconds = [Math]::Round($sequence.frame_duration_seconds * 1000)
    [void]$html.AppendLine('<section class="group"><h2>' + $sequence.effect_id + '</h2><p class="small">' + $sequence.frame_count + ' 帧 · 每帧 ' + $seconds + ' ms · ' + $sequence.playback_mode + ' · 总时长 ' + $sequence.sequence_duration_seconds + ' s · 事件：' + ($sequence.event_ids -join ', ') + '</p><div class="frames">')
    foreach ($frame in $sequence.frame_files) {
        $leaf = [IO.Path]::GetFileName($frame)
        $imageRef = '../../../../../' + $frame
        [void]$html.AppendLine('<div class="cell"><div class="board"><img src="' + $imageRef + '" alt="' + $leaf + '"></div><p class="caption">' + $leaf + '</p></div>')
    }
    [void]$html.AppendLine('</div><p class="small"><a href="../../../../../' + $sequence.source_still + '">源静帧</a></p></section>')
}
[void]$html.AppendLine('<p class="note">逐帧哈希和精确事件说明见 <a href="../../../../SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v03.json">manifest v03</a>。Boss 角色动作序列仍需逐姿势制作，本页不替代 Boss 动画交付。</p></main></body></html>')
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $reviewPath) | Out-Null
[System.IO.File]::WriteAllText($reviewPath, $html.ToString(), [System.Text.UTF8Encoding]::new($false))
Write-Output "Wrote $($sequences.Count) VFX sequences / $($frameAssets.Count) frames, manifest v03, and review page."
