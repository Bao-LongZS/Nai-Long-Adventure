param()

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$ueRoot = Split-Path $PSScriptRoot -Parent
$sourceRoot = Join-Path $ueRoot 'SourceAssets\P7\Characters\A1\CatalogDirect'
$reviewRoot = Join-Path $ueRoot 'SourceAssets\P7\Characters\A1\Review'
New-Item -ItemType Directory -Force -Path $reviewRoot | Out-Null
$outputPath = Join-Path $reviewRoot 'P7_A1_CatalogHUDPortraitReview_v04.png'

$items = @(
    @{ File = 'P7_A1_CharacterHUD_Idle_Figure139_v02.png'; State = '常态'; Figure = '图录 139 · 仰首静立'; Page = '书内第 110 页 / PDF 第 118 页'; Use = '布防 HUD 状态头像' },
    @{ File = 'P7_A1_CharacterHUD_Laugh_Figure003_v03.png'; State = '大笑'; Figure = '图录 003 · 捧腹大笑'; Page = '书内第 99 页 / PDF 第 107 页'; Use = '防守 HUD 状态头像' },
    @{ File = 'P7_A1_CharacterHUD_Victory_Figure121_v02.png'; State = '胜利'; Figure = '图录 121 · 滑跪庆祝'; Page = '书内第 109 页 / PDF 第 117 页'; Use = '胜利结算反馈' },
    @{ File = 'P7_A1_CharacterHUD_Distress_Figure106_v02.png'; State = '失守'; Figure = '图录 106 · 失落蹲坐'; Page = '书内第 107 页 / PDF 第 115 页'; Use = '失守结算反馈' }
)

$canvasWidth = 1600
$canvasHeight = 1320
$bitmap = [System.Drawing.Bitmap]::new($canvasWidth, $canvasHeight)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$graphics.Clear([System.Drawing.Color]::FromArgb(244, 241, 232))

$titleFont = [System.Drawing.Font]::new('Microsoft YaHei UI', 30, [System.Drawing.FontStyle]::Bold)
$subtitleFont = [System.Drawing.Font]::new('Microsoft YaHei UI', 15, [System.Drawing.FontStyle]::Regular)
$stateFont = [System.Drawing.Font]::new('Microsoft YaHei UI', 22, [System.Drawing.FontStyle]::Bold)
$bodyFont = [System.Drawing.Font]::new('Microsoft YaHei UI', 15, [System.Drawing.FontStyle]::Regular)
$smallFont = [System.Drawing.Font]::new('Microsoft YaHei UI', 12, [System.Drawing.FontStyle]::Regular)
$tinyFont = [System.Drawing.Font]::new('Microsoft YaHei UI', 10, [System.Drawing.FontStyle]::Regular)
$darkBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(48, 52, 58))
$mutedBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(98, 99, 96))
$accentBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(193, 137, 50))
$cardBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::White)
$cardBorder = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(220, 215, 205), 2)
$whitePen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(232, 230, 224), 1)

$graphics.DrawString('奶蛙学图录原图 · P7 HUD 状态资源 v04', $titleFont, $darkBrush, 48, 34)
$graphics.DrawString('直接从图录页面裁切；保留原图白底与造型，不改色、不重绘。大笑姿态按用户要求更换为站立捧腹。', $subtitleFont, $mutedBrush, 52, 82)

$cardWidth = 736
$cardHeight = 520
$lefts = @(48, 816)
$tops = @(130, 670)

for ($i = 0; $i -lt $items.Count; $i++) {
    $item = $items[$i]
    $x = $lefts[$i % 2]
    $y = $tops[[int][Math]::Floor($i / 2)]
    $cardRect = [System.Drawing.Rectangle]::new($x, $y, $cardWidth, $cardHeight)
    $graphics.FillRectangle($cardBrush, $cardRect)
    $graphics.DrawRectangle($cardBorder, $cardRect)

    $imagePath = Join-Path $sourceRoot $item.File
    if (-not (Test-Path -LiteralPath $imagePath)) { throw "Missing source image: $imagePath" }
    $sourceImage = [System.Drawing.Image]::FromFile($imagePath)
    $artRect = [System.Drawing.Rectangle]::new($x + 18, $y + 18, 374, 374)
    $graphics.FillRectangle([System.Drawing.SolidBrush]::new([System.Drawing.Color]::White), $artRect)
    $graphics.DrawImage($sourceImage, $artRect)
    $graphics.DrawRectangle($whitePen, $artRect)

    $textX = $x + 416
    $graphics.DrawString($item.State, $stateFont, $accentBrush, $textX, $y + 28)
    $graphics.DrawString($item.Figure, $bodyFont, $darkBrush, $textX, $y + 82)
    $graphics.DrawString($item.Page, $smallFont, $mutedBrush, $textX, $y + 116)
    $graphics.DrawString($item.Use, $smallFont, $mutedBrush, $textX, $y + 150)
    $graphics.DrawString('48 px 原尺寸（放大 2×展示）', $smallFont, $darkBrush, $textX, $y + 212)

    $colorThumb = [System.Drawing.Rectangle]::new($textX, $y + 244, 96, 96)
    $grayThumb = [System.Drawing.Rectangle]::new($textX + 120, $y + 244, 96, 96)
    $smallColor = [System.Drawing.Bitmap]::new(48, 48)
    $smallColorGraphics = [System.Drawing.Graphics]::FromImage($smallColor)
    $smallColorGraphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $smallColorGraphics.DrawImage($sourceImage, 0, 0, 48, 48)
    $smallColorGraphics.Dispose()
    $previousInterpolation = $graphics.InterpolationMode
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $graphics.DrawImage($smallColor, $colorThumb)
    $graphics.DrawRectangle($whitePen, $colorThumb)
    $matrix = [System.Drawing.Imaging.ColorMatrix]::new()
    $matrix.Matrix00 = 0.299; $matrix.Matrix01 = 0.299; $matrix.Matrix02 = 0.299
    $matrix.Matrix10 = 0.587; $matrix.Matrix11 = 0.587; $matrix.Matrix12 = 0.587
    $matrix.Matrix20 = 0.114; $matrix.Matrix21 = 0.114; $matrix.Matrix22 = 0.114
    $attributes = [System.Drawing.Imaging.ImageAttributes]::new()
    $attributes.SetColorMatrix($matrix)
    $smallGray = [System.Drawing.Bitmap]::new(48, 48)
    $smallGrayGraphics = [System.Drawing.Graphics]::FromImage($smallGray)
    $smallGrayGraphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $smallGrayGraphics.DrawImage($sourceImage, [System.Drawing.Rectangle]::new(0, 0, 48, 48), 0, 0, $sourceImage.Width, $sourceImage.Height, [System.Drawing.GraphicsUnit]::Pixel, $attributes)
    $smallGrayGraphics.Dispose()
    $graphics.DrawImage($smallGray, $grayThumb)
    $graphics.DrawRectangle($whitePen, $grayThumb)
    $graphics.InterpolationMode = $previousInterpolation
    $graphics.DrawString('彩色', $tinyFont, $mutedBrush, $textX + 26, $y + 348)
    $graphics.DrawString('灰度', $tinyFont, $mutedBrush, $textX + 154, $y + 348)
    $smallColor.Dispose()
    $smallGray.Dispose()
    $attributes.Dispose()
    $sourceImage.Dispose()
}

$graphics.DrawLine($cardBorder, 48, 1208, 1552, 1208)
$graphics.DrawString('来源：《奶蛙学：基础教程》“奶蛙形态图谱”；书内页 99、107、109、110（PDF 页 107、115、117、118）。', $smallFont, $mutedBrush, 52, 1223)
$graphics.DrawString('大笑状态改用图录 003「捧腹大笑」；图录 079 原图和 UE 资源保留为历史，不再用于 HUD。', $smallFont, $mutedBrush, 52, 1250)
$graphics.DrawString('48px 原尺寸先下采样后以最近邻放大 2×展示；新头像接入后，PIE/Standalone 与分辨率画面待验收。', $smallFont, $mutedBrush, 52, 1277)

$bitmap.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$bitmap.Dispose()
foreach ($font in @($titleFont,$subtitleFont,$stateFont,$bodyFont,$smallFont,$tinyFont)) { $font.Dispose() }
foreach ($brush in @($darkBrush,$mutedBrush,$accentBrush,$cardBrush)) { $brush.Dispose() }
$cardBorder.Dispose()
$whitePen.Dispose()

Write-Output $outputPath
