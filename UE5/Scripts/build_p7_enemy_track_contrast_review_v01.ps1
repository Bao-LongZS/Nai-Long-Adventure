param()

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$ueRoot = Split-Path $PSScriptRoot -Parent
$projectRoot = Split-Path $ueRoot -Parent
$enemyRoot = Join-Path $ueRoot 'SourceAssets\P7\Enemies'
$reviewRoot = Join-Path $enemyRoot 'A1\Review'
$basePath = Join-Path $ueRoot 'Records\p7_td\evidence\P7_ChineseMapUI_DesktopPreview_1280x720_2026-11-11_22-02-12.png'
$outputPath = Join-Path $reviewRoot 'P7_A1_EnemyTrackContrastReview_v01.png'
New-Item -ItemType Directory -Force -Path $reviewRoot | Out-Null

$sprites = @(
    @{ Name='普通型'; Path=(Join-Path $enemyRoot 'A1\P7_A1_Enemy_Normal_v01.png'); X=300; Y=307 },
    @{ Name='快速型'; Path=(Join-Path $enemyRoot 'A1\P7_A1_Enemy_Fast_v01.png'); X=630; Y=343 },
    @{ Name='干扰型'; Path=(Join-Path $enemyRoot 'A0\P7_A0_Enemy_Jammer_v01.png'); X=950; Y=307 }
)
foreach ($path in @($basePath) + @($sprites | ForEach-Object Path)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing preview input: $path" }
}

function New-GrayImage {
    param([System.Drawing.Image]$Source)
    $bmp = [System.Drawing.Bitmap]::new($Source.Width,$Source.Height,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $matrix = [System.Drawing.Imaging.ColorMatrix]::new()
    $matrix.Matrix00=0.299; $matrix.Matrix01=0.299; $matrix.Matrix02=0.299
    $matrix.Matrix10=0.587; $matrix.Matrix11=0.587; $matrix.Matrix12=0.587
    $matrix.Matrix20=0.114; $matrix.Matrix21=0.114; $matrix.Matrix22=0.114
    $attributes = [System.Drawing.Imaging.ImageAttributes]::new()
    $attributes.SetColorMatrix($matrix)
    $g.DrawImage($Source,[System.Drawing.Rectangle]::new(0,0,$Source.Width,$Source.Height),0,0,$Source.Width,$Source.Height,[System.Drawing.GraphicsUnit]::Pixel,$attributes)
    $attributes.Dispose(); $g.Dispose()
    return $bmp
}

function Draw-TrackPreview {
    param([System.Drawing.Image]$Base,[bool]$Gray,[int]$SpriteSize,[int]$XOffset,[int]$YOffset)
    $bmp = [System.Drawing.Bitmap]::new($Base.Width,$Base.Height)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    if ($Gray) {
        $grayBase = New-GrayImage -Source $Base
        $g.DrawImage($grayBase,0,0,$Base.Width,$Base.Height)
        $grayBase.Dispose()
    } else {
        $g.DrawImage($Base,0,0,$Base.Width,$Base.Height)
    }
    foreach ($sprite in $sprites) {
        $source = [System.Drawing.Image]::FromFile($sprite.Path)
        if ($Gray) { $drawImage = New-GrayImage -Source $source } else { $drawImage = $source }
        $dest = [System.Drawing.Rectangle]::new($sprite.X + $XOffset,$sprite.Y + $YOffset,$SpriteSize,$SpriteSize)
        $g.DrawImage($drawImage,$dest)
        if ($Gray) { $drawImage.Dispose() }
        $source.Dispose()
    }
    $g.Dispose()
    return $bmp
}

$base = [System.Drawing.Image]::FromFile($basePath)
$variants = @(
    @{ Name='36px · 彩色'; Gray=$false; Size=36; XOffset=0; YOffset=0 },
    @{ Name='36px · 灰度'; Gray=$true; Size=36; XOffset=0; YOffset=0 },
    @{ Name='48px · 彩色'; Gray=$false; Size=48; XOffset=0; YOffset=0 },
    @{ Name='48px · 灰度'; Gray=$true; Size=48; XOffset=0; YOffset=0 }
)

$canvas = [System.Drawing.Bitmap]::new(1600,1100)
$g = [System.Drawing.Graphics]::FromImage($canvas)
$g.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.TextRenderingHint=[System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$g.Clear([System.Drawing.Color]::FromArgb(242,240,232))
$titleFont=[System.Drawing.Font]::new('Microsoft YaHei UI',27,[System.Drawing.FontStyle]::Bold)
$subtitleFont=[System.Drawing.Font]::new('Microsoft YaHei UI',14,[System.Drawing.FontStyle]::Regular)
$labelFont=[System.Drawing.Font]::new('Microsoft YaHei UI',18,[System.Drawing.FontStyle]::Bold)
$footerFont=[System.Drawing.Font]::new('Microsoft YaHei UI',12,[System.Drawing.FontStyle]::Regular)
$ink=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(42,48,54))
$muted=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(92,98,99))
$cardPen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(218,214,203),2)
$g.DrawString('P7 A1 敌人棋盘路线比例与灰度预览', $titleFont,$ink,48,24)
$g.DrawString('使用已通过的普通型、快速型和干扰型首帧；叠加到 720p 灰盒路线，比较 36px 与 48px。', $subtitleFont,$muted,52,68)

$screenWidth=736; $screenHeight=414
$positions=@(@(48,118),@(816,118),@(48,576),@(816,576))
for($i=0;$i -lt $variants.Count;$i++){
    $v=$variants[$i]
    $x=$positions[$i][0]; $y=$positions[$i][1]
    $g.DrawString($v.Name,$labelFont,$ink,$x,$y-32)
    $preview=Draw-TrackPreview -Base $base -Gray $v.Gray -SpriteSize $v.Size -XOffset 0 -YOffset 0
    $g.DrawImage($preview,$x,$y,$screenWidth,$screenHeight)
    $g.DrawRectangle($cardPen,$x,$y,$screenWidth,$screenHeight)
    $preview.Dispose()
}
$g.DrawString('每种敌人在路线直道上各放一张静态首帧；不包含移动轨迹、受击特效或遮挡判定。', $footerFont,$muted,52,1020)
$g.DrawString('底图为 2026-11-11 1280×720 P7 灰盒截图；此板用于定运行时尺寸候选，非 UE 运行时录屏。', $footerFont,$muted,52,1044)
$canvas.Save($outputPath,[System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose();$canvas.Dispose();$base.Dispose()
foreach($f in @($titleFont,$subtitleFont,$labelFont,$footerFont)){$f.Dispose()}
foreach($b in @($ink,$muted)){$b.Dispose()}
$cardPen.Dispose()
Write-Output $outputPath
