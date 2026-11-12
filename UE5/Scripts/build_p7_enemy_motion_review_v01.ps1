param()

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$ueRoot = Split-Path $PSScriptRoot -Parent
$enemyRoot = Join-Path $ueRoot 'SourceAssets\P7\Enemies'
$outputRoot = Join-Path $enemyRoot 'A1\Review'
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
$outputPath = Join-Path $outputRoot 'P7_A1_EnemyMotionReadabilityReview_v01.png'

$items = @(
    @{ File='A1\P7_A1_Enemy_Normal_v01.png'; Role='普通型'; Frame='帧 1 / 原通过稿'; Status='低重心厚实剪影 · 沿用'; Approved=$true },
    @{ File='A1\P7_A1_Enemy_Normal_Frame02_v01.png'; Role='普通型'; Frame='帧 2 / 接触步'; Status='新动作候选 · 待看连续性'; Approved=$false },
    @{ File='A1\P7_A1_Enemy_Normal_Frame03_v01.png'; Role='普通型'; Frame='帧 3 / 交替步'; Status='新动作候选 · 待看连续性'; Approved=$false },
    @{ File='A1\P7_A1_Enemy_Fast_v01.png'; Role='快速型'; Frame='帧 1 / 原通过稿'; Status='前倾窄身剪影 · 沿用'; Approved=$true },
    @{ File='A1\P7_A1_Enemy_Fast_Frame02_v01.png'; Role='快速型'; Frame='帧 2 / 压缩步'; Status='新动作候选 · 待看连续性'; Approved=$false },
    @{ File='A1\P7_A1_Enemy_Fast_Frame03_v01.png'; Role='快速型'; Frame='帧 3 / 伸展步'; Status='新动作候选 · 待看连续性'; Approved=$false },
    @{ File='A0\P7_A0_Enemy_Jammer_v01.png'; Role='干扰型'; Frame='帧 1 / A0 原通过稿'; Status='紫色干扰身份 · 沿用'; Approved=$true },
    @{ File='A1\P7_A1_Enemy_Jammer_Frame02_v01.png'; Role='干扰型'; Frame='帧 2 / 压缩步'; Status='新动作候选 · 保留闪电标记'; Approved=$false },
    @{ File='A1\P7_A1_Enemy_Jammer_Frame03_v01.png'; Role='干扰型'; Frame='帧 3 / 伸展步'; Status='新动作候选 · 保留闪电标记'; Approved=$false }
)

$canvasWidth = 1600
$canvasHeight = 1380
$bitmap = [System.Drawing.Bitmap]::new($canvasWidth, $canvasHeight)
$g = [System.Drawing.Graphics]::FromImage($bitmap)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$g.Clear([System.Drawing.Color]::FromArgb(242, 240, 232))

$titleFont = [System.Drawing.Font]::new('Microsoft YaHei UI', 28, [System.Drawing.FontStyle]::Bold)
$subtitleFont = [System.Drawing.Font]::new('Microsoft YaHei UI', 14, [System.Drawing.FontStyle]::Regular)
$roleFont = [System.Drawing.Font]::new('Microsoft YaHei UI', 19, [System.Drawing.FontStyle]::Bold)
$bodyFont = [System.Drawing.Font]::new('Microsoft YaHei UI', 13, [System.Drawing.FontStyle]::Regular)
$smallFont = [System.Drawing.Font]::new('Microsoft YaHei UI', 11, [System.Drawing.FontStyle]::Regular)
$tinyFont = [System.Drawing.Font]::new('Microsoft YaHei UI', 10, [System.Drawing.FontStyle]::Regular)
$ink = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(42, 48, 54))
$muted = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(92, 98, 99))
$green = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(48, 139, 103))
$orange = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(185, 111, 50))
$cardBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::White)
$cardPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(218, 214, 203), 2)
$thinPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(225, 221, 212), 1)

$g.DrawString('P7 A1 敌人三帧组 · 缩略与灰度审阅', $titleFont, $ink, 48, 26)
$g.DrawString('透明棋盘只用于检查 Alpha 边缘；彩色与灰度缩略先真实下采样到 48px，再以最近邻 2×显示。', $subtitleFont, $muted, 52, 72)

function Draw-Checkerboard {
    param([System.Drawing.Graphics]$Graphics, [System.Drawing.Rectangle]$Rect)
    $step = 18
    $a = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(244, 243, 238))
    $b = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(223, 224, 220))
    for ($yy = 0; $yy -lt $Rect.Height; $yy += $step) {
        for ($xx = 0; $xx -lt $Rect.Width; $xx += $step) {
            $brush = if (([Math]::Floor($xx / $step) + [Math]::Floor($yy / $step)) % 2 -eq 0) { $a } else { $b }
            $tile = [System.Drawing.Rectangle]::new($Rect.X + $xx, $Rect.Y + $yy,
                [Math]::Min($step, $Rect.Width - $xx), [Math]::Min($step, $Rect.Height - $yy))
            $Graphics.FillRectangle($brush, $tile)
        }
    }
    $a.Dispose(); $b.Dispose()
}

function New-Downsample48 {
    param([System.Drawing.Image]$Source, [bool]$Gray)
    $small = [System.Drawing.Bitmap]::new(48, 48, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $sg = [System.Drawing.Graphics]::FromImage($small)
    $sg.Clear([System.Drawing.Color]::Transparent)
    $sg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $sg.DrawImage($Source, [System.Drawing.Rectangle]::new(0,0,48,48), 0,0,$Source.Width,$Source.Height,[System.Drawing.GraphicsUnit]::Pixel)
    $sg.Dispose()
    if ($Gray) {
        for ($py=0; $py -lt 48; $py++) {
            for ($px=0; $px -lt 48; $px++) {
                $c = $small.GetPixel($px,$py)
                if ($c.A -gt 0) {
                    $l = [int][Math]::Round(0.299*$c.R + 0.587*$c.G + 0.114*$c.B)
                    $small.SetPixel($px,$py,[System.Drawing.Color]::FromArgb($c.A,$l,$l,$l))
                }
            }
        }
    }
    return $small
}

$cardWidth = 488
$cardHeight = 378
$xPositions = @(48, 556, 1064)
$yPositions = @(128, 524, 920)
for ($i=0; $i -lt $items.Count; $i++) {
    $item = $items[$i]
    $x = $xPositions[$i % 3]
    $y = $yPositions[[int][Math]::Floor($i / 3)]
    $cardRect = [System.Drawing.Rectangle]::new($x,$y,$cardWidth,$cardHeight)
    $g.FillRectangle($cardBrush,$cardRect)
    $g.DrawRectangle($cardPen,$cardRect)

    $imagePath = Join-Path $enemyRoot $item.File
    if (-not (Test-Path -LiteralPath $imagePath)) { throw "Missing enemy frame: $imagePath" }
    $source = [System.Drawing.Image]::FromFile($imagePath)
    $artRect = [System.Drawing.Rectangle]::new($x+16,$y+58,242,242)
    Draw-Checkerboard -Graphics $g -Rect $artRect
    $g.DrawImage($source,$artRect)
    $g.DrawRectangle($thinPen,$artRect)

    $textX = $x+282
    $roleBrush = if ($item.Approved) { $green } else { $orange }
    $g.DrawString($item.Role,$roleFont,$roleBrush,$textX,$y+26)
    $g.DrawString($item.Frame,$bodyFont,$ink,$textX,$y+68)
    $g.DrawString($item.Status,$smallFont,$muted,$textX,$y+98)
    $g.DrawString('48px 彩色 / 灰度', $smallFont, $ink, $textX, $y+150)

    $color48 = New-Downsample48 -Source $source -Gray $false
    $gray48 = New-Downsample48 -Source $source -Gray $true
    $colorRect = [System.Drawing.Rectangle]::new($textX,$y+180,96,96)
    $grayRect = [System.Drawing.Rectangle]::new($textX+116,$y+180,96,96)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $g.DrawImage($color48,$colorRect)
    $g.DrawImage($gray48,$grayRect)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.DrawRectangle($thinPen,$colorRect); $g.DrawRectangle($thinPen,$grayRect)
    $g.DrawString('彩色', $tinyFont, $muted, $textX+28, $y+282)
    $g.DrawString('灰度', $tinyFont, $muted, $textX+147, $y+282)
    $g.DrawString("$($source.Width)×$($source.Height) RGBA", $tinyFont, $muted, $textX, $y+320)
    $source.Dispose(); $color48.Dispose(); $gray48.Dispose()
}

$g.DrawLine($cardPen,48,1314,1552,1314)
$g.DrawString('原通过首帧：普通型 v01、快速型 v01、干扰型 A0 v01。6 张新增动作帧保持候选；图集锚点、播放时长和游戏内比例仍待验收。', $smallFont, $muted, 52, 1327)
$g.DrawString('评审顺序为帧 1 → 帧 2 → 帧 3；本页不模拟动画，也不代表帧时长签收。', $smallFont, $muted, 52, 1350)

$bitmap.Save($outputPath,[System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bitmap.Dispose()
foreach ($font in @($titleFont,$subtitleFont,$roleFont,$bodyFont,$smallFont,$tinyFont)) { $font.Dispose() }
foreach ($brush in @($ink,$muted,$green,$orange,$cardBrush)) { $brush.Dispose() }
$cardPen.Dispose(); $thinPen.Dispose()
Write-Output $outputPath
