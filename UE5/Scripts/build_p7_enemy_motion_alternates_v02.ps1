$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourceRoot = Join-Path $repoRoot 'UE5\SourceAssets\P7'
$enemyRoot = Join-Path $sourceRoot 'Enemies\A1'
$reviewRoot = Join-Path $enemyRoot 'Review'
$sourceFolder = Join-Path $enemyRoot 'GenerationSources\v03'
$reviewPath = Join-Path $reviewRoot 'P7_A1_EnemyMotionAlternateReview_v02.png'
$manifestPath = Join-Path $sourceRoot 'Manifests\A1\P7_A1_EnemyMotionAlternateAssets_v02.csv'
$fastCandidate = Join-Path $enemyRoot 'P7_A1_Enemy_Fast_Frame02_v02.png'
$fastSource = Join-Path $sourceFolder 'P7_A1_Enemy_Fast_Frame02_v02_Source.png'

New-Item -ItemType Directory -Force -Path $reviewRoot, $sourceFolder | Out-Null
if (-not (Test-Path -LiteralPath $fastCandidate)) { throw "Missing fast F01 candidate: $fastCandidate" }
if (-not (Test-Path -LiteralPath $fastSource)) { Copy-Item -LiteralPath $fastCandidate -Destination $fastSource }

$rows = @(
    [pscustomobject]@{ Role='Normal'; Label='普通型'; Status='已通过备选'; F00=(Join-Path $enemyRoot 'P7_A1_Enemy_Normal_v01.png'); F01=(Join-Path $enemyRoot 'P7_A1_Enemy_Normal_Frame02_v01.png'); Alt=(Join-Path $enemyRoot 'P7_A1_Enemy_Normal_Frame02_v02.png'); F02=(Join-Path $enemyRoot 'P7_A1_Enemy_Normal_Frame03_v01.png') },
    [pscustomobject]@{ Role='Fast'; Label='快速型'; Status='本轮新候选 · 待评审'; F00=(Join-Path $enemyRoot 'P7_A1_Enemy_Fast_v01.png'); F01=(Join-Path $enemyRoot 'P7_A1_Enemy_Fast_Frame02_v01.png'); Alt=$fastCandidate; F02=(Join-Path $enemyRoot 'P7_A1_Enemy_Fast_Frame03_v01.png') },
    [pscustomobject]@{ Role='Jammer'; Label='干扰型'; Status='已通过备选'; F00=(Join-Path (Split-Path -Parent $enemyRoot) 'A0\P7_A0_Enemy_Jammer_v01.png'); F01=(Join-Path $enemyRoot 'P7_A1_Enemy_Jammer_Frame02_v01.png'); Alt=(Join-Path $enemyRoot 'P7_A1_Enemy_Jammer_Frame02_v02.png'); F02=(Join-Path $enemyRoot 'P7_A1_Enemy_Jammer_Frame03_v01.png') }
)

$columns = @(
    [pscustomobject]@{ Key='F00'; Title='F00 · 已通过基准' },
    [pscustomobject]@{ Key='F01'; Title='F01 · 原帧 v01' },
    [pscustomobject]@{ Key='Alt'; Title='F01 · 压步备选 v02' },
    [pscustomobject]@{ Key='F02'; Title='F02 · 伸步 v01' }
)
$board = [System.Drawing.Bitmap]::new(1800, 1180, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($board)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$graphics.Clear([System.Drawing.Color]::FromArgb(242, 240, 232))

$fontTitle = [System.Drawing.Font]::new('Microsoft YaHei UI', 30, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$fontSub = [System.Drawing.Font]::new('Microsoft YaHei UI', 16, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$fontColumn = [System.Drawing.Font]::new('Microsoft YaHei UI', 15, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$fontRole = [System.Drawing.Font]::new('Microsoft YaHei UI', 19, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$fontCard = [System.Drawing.Font]::new('Microsoft YaHei UI', 14, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$fontMini = [System.Drawing.Font]::new('Microsoft YaHei UI', 12, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$fontFile = [System.Drawing.Font]::new('Microsoft YaHei UI', 10, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$brushText = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(48, 45, 39))
$brushMuted = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(102, 97, 88))
$brushRole = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(64, 111, 91))
$brushOrange = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(183, 99, 42))
$brushCard = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 254, 250))
$penCard = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(216, 211, 198), 2)
$penApproved = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(76, 141, 110), 3)
$penPending = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(214, 122, 57), 4)
$centerFormat = [System.Drawing.StringFormat]::new()
$centerFormat.Alignment = [System.Drawing.StringAlignment]::Center

$graphics.DrawString('P7 A1 敌人动作候选 · 三类 F01 压步对照 v02', $fontTitle, $brushText, 40, 20)
$graphics.DrawString('每行比较 F00 身份基准、F01 原帧、压步备选及 F02 伸步；右侧为真实 48×48 下采样（2×最近邻放大）和彩色 / 灰度。', $fontSub, $brushMuted, 42, 65)

$left = 38
$cardWidth = 414
$cardHeight = 282
$columnGap = 16
$rowGap = 38
$firstCardY = 172
for ($columnIndex = 0; $columnIndex -lt $columns.Count; $columnIndex++) {
    $centerX = $left + $columnIndex * ($cardWidth + $columnGap) + ($cardWidth / 2)
    $graphics.DrawString($columns[$columnIndex].Title, $fontColumn, $brushText, [System.Drawing.RectangleF]::new($centerX - 207, 119, 414, 24), $centerFormat)
}

function Draw-Checker($Graphics, [int]$X, [int]$Y, [int]$Width, [int]$Height, [int]$CellSize = 18) {
    $light = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(246, 246, 242))
    $dark = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(220, 222, 217))
    for ($cy = 0; $cy -lt $Height; $cy += $CellSize) {
        for ($cx = 0; $cx -lt $Width; $cx += $CellSize) {
            $brush = if (([math]::Floor($cx / $CellSize) + [math]::Floor($cy / $CellSize)) % 2 -eq 0) { $light } else { $dark }
            $Graphics.FillRectangle($brush, $X + $cx, $Y + $cy, [math]::Min($CellSize, $Width - $cx), [math]::Min($CellSize, $Height - $cy))
        }
    }
    $light.Dispose(); $dark.Dispose()
}

function New-48pxImages([System.Drawing.Bitmap]$Source) {
    $color48 = [System.Drawing.Bitmap]::new(48, 48, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $colorGraphics = [System.Drawing.Graphics]::FromImage($color48)
    $colorGraphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $colorGraphics.Clear([System.Drawing.Color]::Transparent)
    $colorGraphics.DrawImage($Source, 0, 0, 48, 48)
    $colorGraphics.Dispose()
    $gray48 = [System.Drawing.Bitmap]::new(48, 48, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($py = 0; $py -lt 48; $py++) {
        for ($px = 0; $px -lt 48; $px++) {
            $pixel = $color48.GetPixel($px, $py)
            $grayValue = [int][math]::Round((0.299 * $pixel.R) + (0.587 * $pixel.G) + (0.114 * $pixel.B))
            $gray48.SetPixel($px, $py, [System.Drawing.Color]::FromArgb($pixel.A, $grayValue, $grayValue, $grayValue))
        }
    }
    return @($color48, $gray48)
}

$manifestRows = @()
$oldManifest = Join-Path $sourceRoot 'Manifests\A1\P7_A1_EnemyMotionAlternateAssets_v01.csv'
if (Test-Path -LiteralPath $oldManifest) {
    $manifestRows += Import-Csv -LiteralPath $oldManifest | ForEach-Object { $_.ReviewBoard = 'Enemies/A1/Review/P7_A1_EnemyMotionAlternateReview_v02.png'; $_ }
}

for ($rowIndex = 0; $rowIndex -lt $rows.Count; $rowIndex++) {
    $row = $rows[$rowIndex]
    $rowY = $firstCardY + $rowIndex * ($cardHeight + $rowGap)
    $statusBrush = if ($row.Role -eq 'Fast') { $brushOrange } else { $brushRole }
    $graphics.DrawString("$($row.Label) · $($row.Status)", $fontRole, $statusBrush, $left, $rowY - 29)
    for ($columnIndex = 0; $columnIndex -lt $columns.Count; $columnIndex++) {
        $column = $columns[$columnIndex]
        $file = $row.($column.Key)
        if (-not (Test-Path -LiteralPath $file)) { throw "Missing comparison frame: $file" }
        $x = $left + $columnIndex * ($cardWidth + $columnGap)
        $edgePen = if ($column.Key -eq 'Alt' -and $row.Role -eq 'Fast') { $penPending } elseif ($column.Key -eq 'Alt') { $penApproved } else { $penCard }
        $graphics.FillRectangle($brushCard, $x, $rowY, $cardWidth, $cardHeight)
        $graphics.DrawRectangle($edgePen, $x, $rowY, $cardWidth, $cardHeight)
        $imageRect = [System.Drawing.Rectangle]::new($x + 12, $rowY + 49, 202, 202)
        Draw-Checker $graphics $imageRect.X $imageRect.Y $imageRect.Width $imageRect.Height 18
        $sourceBitmap = [System.Drawing.Bitmap]::FromFile($file)
        if ($sourceBitmap.Width -ne 1254 -or $sourceBitmap.Height -ne 1254) { throw "Unexpected frame dimensions: $file ($($sourceBitmap.Width)x$($sourceBitmap.Height))" }
        if ($sourceBitmap.PixelFormat -ne [System.Drawing.Imaging.PixelFormat]::Format32bppArgb -and $sourceBitmap.PixelFormat -ne [System.Drawing.Imaging.PixelFormat]::Format32bppPArgb) {
            # PNG readers may report another 32-bit layout; verify alpha through representative pixels below.
        }
        if ($sourceBitmap.GetPixel(0, 0).A -ne 0 -or $sourceBitmap.GetPixel(1253, 1253).A -ne 0) { throw "Frame corners are not transparent: $file" }
        $graphics.DrawImage($sourceBitmap, $imageRect)
        $thumbs = New-48pxImages $sourceBitmap
        $thumbX = $x + 244
        $graphics.DrawString('48px 彩色', $fontMini, $brushMuted, $thumbX, $rowY + 70)
        $graphics.DrawString('灰度', $fontMini, $brushMuted, $thumbX + 83, $rowY + 70)
        Draw-Checker $graphics $thumbX ($rowY + 88) 72 72 12
        Draw-Checker $graphics ($thumbX + 82) ($rowY + 88) 72 72 12
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
        $graphics.DrawImage($thumbs[0], [System.Drawing.Rectangle]::new($thumbX, $rowY + 88, 72, 72))
        $graphics.DrawImage($thumbs[1], [System.Drawing.Rectangle]::new($thumbX + 82, $rowY + 88, 72, 72))
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.DrawString([System.IO.Path]::GetFileName($file), $fontFile, $brushMuted, $x + 12, $rowY + 260)
        $sourceBitmap.Dispose(); $thumbs[0].Dispose(); $thumbs[1].Dispose()
    }
}

$graphics.DrawString('本轮仅新增快速型 F01 压步备选；普通型和干扰型压步备选已通过。新增图尚待评审，不导入 Unreal；全组动作节奏、锚点和实际尺寸仍待运行时确认。', $fontSub, $brushMuted, 40, 1150)
$board.Save($reviewPath, [System.Drawing.Imaging.ImageFormat]::Png)

$sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $fastSource).Hash.ToLowerInvariant()
$exportHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $fastCandidate).Hash.ToLowerInvariant()
$fastPrompt = '保持快速型蓝青角色的面部、眼睛、奶油色脸腹、蓝紫色块、青色长鳍、深海军蓝描边与右向四分之三身份不变；只改为压低前倾的紧凑过步姿势，长腿收向身体下方，一脚经过躯干、一脚后抬，手臂略反向摆动；透明背景，48px 下动作仍清晰。'
$manifestRows += [pscustomobject]@{
    AssetId='P7-ENM-A1-FAST-F02-ALT'; Owner='P7'; Role='Fast'; FrameIndex='1'
    SourceImagePath='Enemies/A1/GenerationSources/v03/P7_A1_Enemy_Fast_Frame02_v02_Source.png'; SourceImageSHA256=$sourceHash
    ExportPath='Enemies/A1/P7_A1_Enemy_Fast_Frame02_v02.png'; ExportSHA256=$exportHash; Width='1254'; Height='1254'
    Alpha='Straight RGBA transparent'; IntendedUse='Optional alternate F01 frame for fast enemy motion loop'
    ReferenceFrame0='Enemies/A1/P7_A1_Enemy_Fast_v01.png'; AlternativeTo='Enemies/A1/P7_A1_Enemy_Fast_Frame02_v01.png'
    Version='v02'; GenerationMethod='Built-in ImageGen; identity-preserving pose-only variation; exported at 1254x1254'
    ReviewStatus='Candidate; pending user visual approval; not imported into Unreal'; UnrealPath=''
    ReviewBoard='Enemies/A1/Review/P7_A1_EnemyMotionAlternateReview_v02.png'; PromptSummary=$fastPrompt
}

$manifestPath = Join-Path $sourceRoot 'Manifests\A1\P7_A1_EnemyMotionAlternateAssets_v02.csv'
$manifestRows | Export-Csv -LiteralPath $manifestPath -NoTypeInformation -Encoding UTF8

$graphics.Dispose(); $board.Dispose()
$fontTitle.Dispose(); $fontSub.Dispose(); $fontColumn.Dispose(); $fontRole.Dispose(); $fontCard.Dispose(); $fontMini.Dispose(); $fontFile.Dispose()
$brushText.Dispose(); $brushMuted.Dispose(); $brushRole.Dispose(); $brushOrange.Dispose(); $brushCard.Dispose()
$penCard.Dispose(); $penApproved.Dispose(); $penPending.Dispose(); $centerFormat.Dispose()

Write-Output "Review=$reviewPath"
Write-Output "Manifest=$manifestPath"
Write-Output "GenerationSource=$fastSource"
Write-Output "SourceSHA256=$sourceHash"
Write-Output "ExportSHA256=$exportHash"
