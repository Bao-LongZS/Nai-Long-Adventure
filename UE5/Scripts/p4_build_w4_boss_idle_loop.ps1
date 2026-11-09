param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$bossRoot = Join-Path $root 'UE5/SourceAssets/P4/Characters/Alpha/W4_Boss/RuntimeWIP'
$frameRoot = Join-Path $bossRoot 'Sequences/Frames512'
$baseManifestPath = Join-Path $root 'UE5/SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v03.json'
$outputManifestPath = Join-Path $root 'UE5/SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v04.json'
$reviewPath = Join-Path $root 'UE5/Records/p4_alpha/evidence/W4/P4_W4_BossIdleSequenceReview_v01.html'
$baseManifest = Get-Content -Raw -LiteralPath $baseManifestPath | ConvertFrom-Json
$viewFrames = @(
    @{ view = 'front'; source = 'boss_nai_tong_idle_front_00.png' },
    @{ view = 'back'; source = 'boss_nai_tong_idle_back_00.png' },
    @{ view = 'side_right'; source = 'boss_nai_tong_idle_side_right_00.png' }
)
$breatheScales = @(1.000, 1.014, 1.028, 1.014)
$frameDurationSeconds = 0.20
$anchorX = 256
$anchorY = 466
$threshold = 8

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
    if ($right -lt $left) { throw 'Boss idle source has no visible alpha pixels.' }
    return [System.Drawing.Rectangle]::new($left, $top, ($right - $left + 1), ($bottom - $top + 1))
}

function Export-BossIdleFrame {
    param([string]$SourcePath, [string]$DestinationPath, [double]$Scale)
    $source = [System.Drawing.Bitmap]::FromFile($SourcePath)
    $canvas = [System.Drawing.Bitmap]::new(512, 512, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($canvas)
    try {
        $bounds = Get-VisibleBounds -Bitmap $source -Threshold $threshold
        $width = [Math]::Max(1, [int][Math]::Round($bounds.Width * $Scale))
        $height = [Math]::Max(1, [int][Math]::Round($bounds.Height * $Scale))
        $left = [int][Math]::Round($anchorX - ($width / 2.0))
        $top = $anchorY - $height
        $destination = [System.Drawing.Rectangle]::new($left, $top, $width, $height)
        $sourceRect = [System.Drawing.Rectangle]::new($bounds.X, $bounds.Y, $bounds.Width, $bounds.Height)
        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        $graphics.DrawImage($source, $destination, $sourceRect, [System.Drawing.GraphicsUnit]::Pixel)
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $DestinationPath) | Out-Null
        $canvas.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
        return [pscustomobject]@{ source_visible_bounds_px = @($bounds.X, $bounds.Y, $bounds.Width, $bounds.Height); output_bounds_px = @($left, $top, $width, $height) }
    }
    finally { $graphics.Dispose(); $canvas.Dispose(); $source.Dispose() }
}

New-Item -ItemType Directory -Force -Path $frameRoot | Out-Null
$newFrames = [System.Collections.Generic.List[object]]::new()
$sequences = [System.Collections.Generic.List[object]]::new()

foreach ($viewRow in $viewFrames) {
    $sourcePath = Join-Path (Join-Path $bossRoot 'Frames512') $viewRow.source
    if (-not (Test-Path -LiteralPath $sourcePath)) { throw "Missing Boss idle candidate: $sourcePath" }
    $baseAssetId = [System.IO.Path]::GetFileNameWithoutExtension($viewRow.source)
    $baseAsset = $baseManifest.assets | Where-Object { $_.asset_id -eq $baseAssetId } | Select-Object -First 1
    if ($null -eq $baseAsset) { throw "Missing Boss base manifest entry: $baseAssetId" }
    $frameFiles = [System.Collections.Generic.List[string]]::new()
    $scaleAssets = [System.Collections.Generic.List[object]]::new()

    for ($index = 0; $index -lt $breatheScales.Count; $index++) {
        $frameName = 'boss_nai_tong_idle_{0}_{1:D2}.png' -f $viewRow.view, $index
        $destinationPath = Join-Path $frameRoot $frameName
        $scale = [double]$breatheScales[$index]
        $render = Export-BossIdleFrame -SourcePath $sourcePath -DestinationPath $destinationPath -Scale $scale

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
            if (($corners -join ',') -ne '0,0,0,0' -or $outerVisible -ne 0) { throw "Alpha edge check failed: $frameName" }
            $visibleBottom = -1
            for ($y = 511; $y -ge 0 -and $visibleBottom -lt 0; $y--) {
                for ($x = 0; $x -lt 512; $x++) { if ($bitmap.GetPixel($x, $y).A -gt 32) { $visibleBottom = $y; break } }
            }
            if ($visibleBottom -ne $anchorY - 1) { throw "Foot baseline changed: $frameName bottom=$visibleBottom" }
        }
        finally { $bitmap.Dispose() }

        $relativeFile = Get-RelativePath -Path $destinationPath
        $frameFiles.Add($relativeFile)
        $entry = [pscustomobject][ordered]@{
            asset_id = ('boss_nai_tong_idle_{0}_frame_{1:D2}' -f $viewRow.view, $index)
            type = 'boss_idle_transform_frame_candidate'
            sequence_id = ('boss_nai_tong_idle_{0}_sequence_v01' -f $viewRow.view)
            state = 'idle'
            view = $viewRow.view
            frame_index = $index
            frame_duration_seconds = $frameDurationSeconds
            playback_mode = 'loop'
            file = $relativeFile
            source_file = Get-RelativePath -Path $sourcePath
            high_resolution_source_file = $baseAsset.source_file
            dimensions_px = @(512, 512)
            provisional_foot_anchor_px = @($anchorX, $anchorY)
            measured_visible_bottom_y_px = $visibleBottom
            transform_only = $true
            frame_transform = [ordered]@{ uniform_scale = $scale; opacity = 1.0; rotation_degrees = 0; foot_anchor_locked = $true }
            corner_alpha = @($corners)
            visible_outer_border_pixels = $outerVisible
            source_visible_bounds_px = $render.source_visible_bounds_px
            output_bounds_px = $render.output_bounds_px
            file_size_bytes = (Get-Item -LiteralPath $destinationPath).Length
            sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $destinationPath).Hash
            runtime_ready = $false
        }
        $scaleAssets.Add($entry)
        $newFrames.Add($entry)
    }

    $sequences.Add([pscustomobject][ordered]@{
        sequence_id = ('boss_nai_tong_idle_{0}_sequence_v01' -f $viewRow.view)
        character_id = 'boss_nai_tong'
        state = 'idle'
        view = $viewRow.view
        frame_files = @($frameFiles.ToArray())
        frame_count = $breatheScales.Count
        frame_duration_seconds = $frameDurationSeconds
        sequence_duration_seconds = [Math]::Round($frameDurationSeconds * $breatheScales.Count, 3)
        playback_mode = 'loop'
        foot_anchor_px = @($anchorX, $anchorY)
        motion_type = 'subtle full-sprite breathing scale; no independently animated limbs, face or prop'
        source_still = Get-RelativePath -Path $sourcePath
        status = 'idle loop timing preview; character shape animation pending'
        runtime_ready = $false
    })
}

$manifest = Get-Content -Raw -LiteralPath $baseManifestPath | ConvertFrom-Json
$manifest.version = 'v04'
$manifest.created_date = '2026-10-30'
$manifest.status = 'wip_boss_idle_and_vfx_transform_sequences'
$manifest.assets = @($manifest.assets) + @($newFrames.ToArray())
$manifest.global_constraints.runtime_ready = $false
$manifest.global_constraints.ue_imported = $false
$manifest.global_constraints | Add-Member -NotePropertyName delivery_status -NotePropertyValue 'WIP Boss idle breathing timing previews and VFX transform sequences; full animation redraw, art cleanup, fixed-camera review and UE integration remain pending.' -Force
$manifest | Add-Member -NotePropertyName boss_sequence_batch -NotePropertyValue ([ordered]@{
    status = 'transform_timing_preview_wip'
    sequence_count = $sequences.Count
    frame_count = $newFrames.Count
    views = @('front', 'back', 'side_right')
    frame_duration_seconds = $frameDurationSeconds
    sequence_duration_seconds = 0.8
    playback_mode = 'loop'
    foot_anchor_px = @($anchorX, $anchorY)
    motion_source = 'Uniform full-sprite scale changes around an estimated foot baseline. No pose redraw, squash/stretch, blinking or independent lid motion.'
    foot_baseline_measured_y_px = 465
    sequences = @($sequences.ToArray())
}) -Force
$manifest | Add-Member -NotePropertyName boss_sequence_review_page -NotePropertyValue 'UE5/Records/p4_alpha/evidence/W4/P4_W4_BossIdleSequenceReview_v01.html' -Force
$manifest | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath $outputManifestPath -Encoding utf8

$html = [System.Text.StringBuilder]::new()
[void]$html.AppendLine('<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>P4 W4 Boss 待机循环候选</title><style>body{margin:0;padding:28px;background:#f1f4f4;color:#26323a;font:14px/1.55 "Segoe UI","Microsoft YaHei",sans-serif}main{max-width:1280px;margin:auto}h1{font-size:28px;margin:0 0 6px}.muted{color:#63717a}.warn{padding:13px 16px;background:#fff7e7;border-left:5px solid #c99a4a;border-radius:8px;margin:18px 0 24px}.row{background:#fff;border:1px solid #d7dfe2;border-radius:12px;margin:16px 0;padding:14px}.frames{display:grid;grid-template-columns:repeat(4,1fr);gap:12px}.cell{padding:8px;border:1px solid #e2e6e7;border-radius:8px}.board{height:280px;display:grid;place-items:center;background-color:#fff;background-image:linear-gradient(45deg,#e8ebec 25%,transparent 25%),linear-gradient(-45deg,#e8ebec 25%,transparent 25%),linear-gradient(45deg,transparent 75%,#e8ebec 75%),linear-gradient(-45deg,transparent 75%,#e8ebec 75%);background-size:20px 20px;background-position:0 0,0 10px,10px -10px,-10px 0}.board img{max-width:100%;max-height:100%;object-fit:contain}.cap{font:11px Consolas,monospace;overflow-wrap:anywhere}@media(max-width:760px){.frames{grid-template-columns:repeat(2,1fr)}.board{height:200px}}</style></head><body><main>')
[void]$html.AppendLine('<h1>P4 W4 奶桶队长待机循环候选</h1><p class="muted">v01 · 2026-10-30 · 正面、背面、右侧面 · 4 帧 × 200 ms · 0.8 s 循环</p>')
[void]$html.AppendLine('<div class="warn"><strong>当前方式：</strong>保持 `(256,466)` 脚底基线，对完整角色做轻微均匀缩放呼吸预览。它不包含眼睛、手足、桶盖的独立动作，也不替代逐姿势重绘；目前供锚点和循环节奏检查。</div>')
foreach ($sequence in $sequences) {
    [void]$html.AppendLine('<section class="row"><h2>' + $sequence.view + ' · ' + $sequence.frame_count + ' 帧循环</h2><div class="frames">')
    foreach ($frame in $sequence.frame_files) {
        $leaf = [IO.Path]::GetFileName($frame)
        [void]$html.AppendLine('<div class="cell"><div class="board"><img src="../../../../../' + $frame + '" alt="' + $leaf + '"></div><p class="cap">' + $leaf + '</p></div>')
    }
    [void]$html.AppendLine('</div></section>')
}
[void]$html.AppendLine('<p class="muted">序列帧和哈希见 <a href="../../../../SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v04.json">manifest v04</a>。其他 8 个 Boss 状态仍为单姿势候选，完整制作待后续推进。</p></main></body></html>')
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $reviewPath) | Out-Null
[System.IO.File]::WriteAllText($reviewPath, $html.ToString(), [System.Text.UTF8Encoding]::new($false))
Write-Output "Wrote $($sequences.Count) Boss idle loops / $($newFrames.Count) frames, manifest v04, and review page."
