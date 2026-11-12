param()

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$ueRoot = Split-Path $PSScriptRoot -Parent
$environmentRoot = Join-Path $ueRoot 'SourceAssets\P7\Environment\A1'
$reviewRoot = Join-Path $environmentRoot 'Review'
New-Item -ItemType Directory -Force -Path $reviewRoot | Out-Null
$atlasPath = Join-Path $environmentRoot 'P7_A1_DestructibleObstacleAtlas_v02.png'
$outputPath = Join-Path $reviewRoot 'P7_A1_DestructibleObstacleReadabilityReview_v01.png'
if (-not (Test-Path -LiteralPath $atlasPath)) { throw "Missing atlas: $atlasPath" }

$items = @(
    @{ Name='奶油饼干箱'; State='完好'; X=32; Y=32; Desc='无裂纹，箱体完整'; Accent=[System.Drawing.Color]::FromArgb(54,132,106) },
    @{ Name='奶油饼干箱'; State='裂损'; X=576; Y=32; Desc='贯穿箱面的粗裂口'; Accent=[System.Drawing.Color]::FromArgb(183,116,51) },
    @{ Name='奶油饼干箱'; State='将碎'; X=1120; Y=32; Desc='大块缺口与脱落碎片'; Accent=[System.Drawing.Color]::FromArgb(177,82,69) },
    @{ Name='燕麦饼叠垛'; State='完好'; X=32; Y=576; Desc='双层圆角饼块完整'; Accent=[System.Drawing.Color]::FromArgb(54,132,106) },
    @{ Name='燕麦饼叠垛'; State='裂损'; X=576; Y=576; Desc='上下层分别出现裂痕'; Accent=[System.Drawing.Color]::FromArgb(183,116,51) },
    @{ Name='燕麦饼叠垛'; State='将碎'; X=1120; Y=576; Desc='宽裂口、露出断面'; Accent=[System.Drawing.Color]::FromArgb(177,82,69) }
)

$atlas = [System.Drawing.Bitmap]::new($atlasPath)
$canvas = [System.Drawing.Bitmap]::new(1600,1040)
$g = [System.Drawing.Graphics]::FromImage($canvas)
$g.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.TextRenderingHint=[System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$g.Clear([System.Drawing.Color]::FromArgb(242,240,232))

$titleFont=[System.Drawing.Font]::new('Microsoft YaHei UI',28,[System.Drawing.FontStyle]::Bold)
$subtitleFont=[System.Drawing.Font]::new('Microsoft YaHei UI',14,[System.Drawing.FontStyle]::Regular)
$nameFont=[System.Drawing.Font]::new('Microsoft YaHei UI',18,[System.Drawing.FontStyle]::Bold)
$stateFont=[System.Drawing.Font]::new('Microsoft YaHei UI',20,[System.Drawing.FontStyle]::Bold)
$bodyFont=[System.Drawing.Font]::new('Microsoft YaHei UI',13,[System.Drawing.FontStyle]::Regular)
$smallFont=[System.Drawing.Font]::new('Microsoft YaHei UI',11,[System.Drawing.FontStyle]::Regular)
$tinyFont=[System.Drawing.Font]::new('Microsoft YaHei UI',10,[System.Drawing.FontStyle]::Regular)
$ink=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(42,48,54))
$muted=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(92,98,99))
$cardBrush=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::White)
$cardPen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(218,214,203),2)
$hairPen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(228,224,215),1)
$g.DrawString('P7 A1 可破坏物 v02 · 48px 与灰度预览',$titleFont,$ink,48,24)
$g.DrawString('图集原格 512×512 直接裁片；透明棋盘只作边缘检查，缩略先下采样到 48×48，再以最近邻 2×展示。',$subtitleFont,$muted,52,68)

function Draw-Checker {
    param([System.Drawing.Graphics]$Graphics,[System.Drawing.Rectangle]$Rect)
    $step=18
    $light=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(244,243,238))
    $dark=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(223,224,220))
    for($yy=0;$yy -lt $Rect.Height;$yy+=$step){for($xx=0;$xx -lt $Rect.Width;$xx+=$step){
        $b=if((([int]($xx/$step)+[int]($yy/$step))%2)-eq 0){$light}else{$dark}
        $r=[System.Drawing.Rectangle]::new($Rect.X+$xx,$Rect.Y+$yy,[Math]::Min($step,$Rect.Width-$xx),[Math]::Min($step,$Rect.Height-$yy))
        $Graphics.FillRectangle($b,$r)
    }}
    $light.Dispose();$dark.Dispose()
}

function New-Swatch48 {
    param([System.Drawing.Image]$Source,[bool]$Gray)
    $small=[System.Drawing.Bitmap]::new(48,48,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $sg=[System.Drawing.Graphics]::FromImage($small)
    $sg.Clear([System.Drawing.Color]::Transparent)
    $sg.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    if($Gray){
        $m=[System.Drawing.Imaging.ColorMatrix]::new()
        $m.Matrix00=0.299;$m.Matrix01=0.299;$m.Matrix02=0.299
        $m.Matrix10=0.587;$m.Matrix11=0.587;$m.Matrix12=0.587
        $m.Matrix20=0.114;$m.Matrix21=0.114;$m.Matrix22=0.114
        $a=[System.Drawing.Imaging.ImageAttributes]::new();$a.SetColorMatrix($m)
        $sg.DrawImage($Source,[System.Drawing.Rectangle]::new(0,0,48,48),0,0,$Source.Width,$Source.Height,[System.Drawing.GraphicsUnit]::Pixel,$a)
        $a.Dispose()
    }else{
        $sg.DrawImage($Source,[System.Drawing.Rectangle]::new(0,0,48,48),0,0,$Source.Width,$Source.Height,[System.Drawing.GraphicsUnit]::Pixel)
    }
    $sg.Dispose();return $small
}

$cardWidth=488;$cardHeight=408
$xs=@(48,556,1064);$ys=@(126,550)
for($i=0;$i -lt $items.Count;$i++){
    $item=$items[$i];$x=$xs[$i%3];$y=$ys[[int][Math]::Floor($i/3)]
    $card=[System.Drawing.Rectangle]::new($x,$y,$cardWidth,$cardHeight)
    $g.FillRectangle($cardBrush,$card);$g.DrawRectangle($cardPen,$card)
    $cell=$atlas.Clone([System.Drawing.Rectangle]::new($item.X,$item.Y,512,512),[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $artRect=[System.Drawing.Rectangle]::new($x+18,$y+70,230,230)
    Draw-Checker -Graphics $g -Rect $artRect
    $g.DrawImage($cell,$artRect);$g.DrawRectangle($hairPen,$artRect)

    $textX=$x+270
    $nameBrush=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(42,48,54))
    $stateBrush=[System.Drawing.SolidBrush]::new($item.Accent)
    $g.DrawString($item.Name,$nameFont,$nameBrush,$textX,$y+28)
    $g.DrawString($item.State,$stateFont,$stateBrush,$textX,$y+64)
    $g.DrawString($item.Desc,$bodyFont,$muted,$textX,$y+112)
    $g.DrawString('48px 彩色 / 灰度',$smallFont,$ink,$textX,$y+174)
    $color48=New-Swatch48 -Source $cell -Gray $false
    $gray48=New-Swatch48 -Source $cell -Gray $true
    $colorRect=[System.Drawing.Rectangle]::new($textX,$y+202,96,96)
    $grayRect=[System.Drawing.Rectangle]::new($textX+116,$y+202,96,96)
    $g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $g.DrawImage($color48,$colorRect);$g.DrawImage($gray48,$grayRect)
    $g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.DrawRectangle($hairPen,$colorRect);$g.DrawRectangle($hairPen,$grayRect)
    $g.DrawString('彩色',$tinyFont,$muted,$textX+28,$y+304)
    $g.DrawString('灰度',$tinyFont,$muted,$textX+147,$y+304)
    $g.DrawString('Atlas 单格 512×512',$tinyFont,$muted,$textX,$y+350)
    $cell.Dispose();$color48.Dispose();$gray48.Dispose();$nameBrush.Dispose();$stateBrush.Dispose()
}

$g.DrawLine($cardPen,48,982,1552,982)
$g.DrawString('确认的视觉差异：完好无裂；裂损有贯穿裂纹；将碎增加大缺口和碎片。缩略可读性已检查；相对棋盘尺寸、损伤切换节奏、路线遮挡和 UE 拼接仍待验收。',$smallFont,$muted,52,996)
$canvas.Save($outputPath,[System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose();$canvas.Dispose();$atlas.Dispose()
foreach($f in @($titleFont,$subtitleFont,$nameFont,$stateFont,$bodyFont,$smallFont,$tinyFont)){$f.Dispose()}
foreach($b in @($ink,$muted,$cardBrush)){$b.Dispose()}
$cardPen.Dispose();$hairPen.Dispose()
Write-Output $outputPath
