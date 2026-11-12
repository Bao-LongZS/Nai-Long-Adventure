param()

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$ueRoot = Split-Path $PSScriptRoot -Parent
$envRoot = Join-Path $ueRoot 'SourceAssets\P7\Environment\A1'
$reviewRoot = Join-Path $envRoot 'Review'
New-Item -ItemType Directory -Force -Path $reviewRoot | Out-Null
$terrainPath = Join-Path $envRoot 'P7_A1_TerrainTileAtlas_v02.png'
$gatePath = Join-Path $envRoot 'P7_A1_ForkGateStates_v02.png'
$outputPath = Join-Path $reviewRoot 'P7_A1_ForkGateTopologyAudit_v01.png'

$terrain = [System.Drawing.Bitmap]::new($terrainPath)
$gates = [System.Drawing.Bitmap]::new($gatePath)
$tJunction = $terrain.Clone([System.Drawing.Rectangle]::new(1008,1128,960,528),[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$gateA = $gates.Clone([System.Drawing.Rectangle]::new(0,24,960,528),[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$gateB = $gates.Clone([System.Drawing.Rectangle]::new(980,24,960,528),[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)

$canvas=[System.Drawing.Bitmap]::new(1600,1040)
$g=[System.Drawing.Graphics]::FromImage($canvas)
$g.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.TextRenderingHint=[System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$g.Clear([System.Drawing.Color]::FromArgb(242,240,232))
$title=[System.Drawing.Font]::new('Microsoft YaHei UI',28,[System.Drawing.FontStyle]::Bold)
$sub=[System.Drawing.Font]::new('Microsoft YaHei UI',14,[System.Drawing.FontStyle]::Regular)
$heading=[System.Drawing.Font]::new('Microsoft YaHei UI',20,[System.Drawing.FontStyle]::Bold)
$body=[System.Drawing.Font]::new('Microsoft YaHei UI',14,[System.Drawing.FontStyle]::Regular)
$small=[System.Drawing.Font]::new('Microsoft YaHei UI',12,[System.Drawing.FontStyle]::Regular)
$ink=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(42,48,54))
$muted=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(90,96,98))
$blue=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(20,125,139))
$orange=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(184,105,51))
$card=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::White)
$pen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(218,214,203),2)

$g.DrawString('P7 A1 分岔门候选 · 路线拓扑核对',$title,$ink,48,24)
$g.DrawString('用 P7 Alpha01 路线坐标核对地面 T 形支路，与当前 ForkGateStates_v02 图形方向是否一致。',$sub,$muted,52,68)

$left=[System.Drawing.Rectangle]::new(48,124,736,590)
$right=[System.Drawing.Rectangle]::new(816,124,736,590)
$g.FillRectangle($card,$left);$g.DrawRectangle($pen,$left)
$g.FillRectangle($card,$right);$g.DrawRectangle($pen,$right)
$g.DrawString('Alpha01 数据：T 形支路（缺南）',$heading,$blue,72,146)
$g.DrawString('ForkGateStates_v02：Y 形分岔',$heading,$orange,840,146)
$g.DrawImage($tJunction,72,194,688,378)

$gateSlotA=[System.Drawing.Rectangle]::new(840,194,332,182)
$gateSlotB=[System.Drawing.Rectangle]::new(1192,194,332,182)
$g.DrawImage($gateA,$gateSlotA);$g.DrawImage($gateB,$gateSlotB)
$g.DrawRectangle($pen,$gateSlotA);$g.DrawRectangle($pen,$gateSlotB)
$g.DrawString('状态 A',$small,$muted,840,382)
$g.DrawString('状态 B',$small,$muted,1192,382)

$g.DrawString('连接：西 ↔ 东直行，并向北接绕行路线；南侧无路。',$body,$ink,72,592)
$g.DrawString('连接：南侧来路分为东北/东南两个斜向支路。',$body,$ink,840,408)
$g.DrawString('Alpha01 CSV：A 走直线 y=5；B 从 (1,5) 北拐到 y=4，在 (18,5) 汇回。', $body,$ink,840,442)
$g.DrawString('两个共享点 (1,5)/(18,5) 都是西—东—北三向 T 形。', $body,$ink,840,474)

$g.DrawLine($pen,48,748,1552,748)
$routeY=824
$g.DrawString('A 直行', $heading,$blue,64,782)
$g.DrawLine([System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(20,125,139),8),210,$routeY,672,$routeY)
$g.DrawString('B 绕行', $heading,$orange,840,782)
$g.DrawLine([System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(184,105,51),8),990,$routeY+24,1134,$routeY+24)
$g.DrawLine([System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(184,105,51),8),1134,$routeY+24,1134,$routeY-24)
$g.DrawLine([System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(184,105,51),8),1134,$routeY-24,1448,$routeY-24)

$g.DrawString('需要在导入前确认：Y 形是否仅作符号化状态覆盖，还是要重做为与 Alpha01 路线相接的 T 形门片。当前候选未导入 UE。',$body,$muted,52,914)
$g.DrawString('几何依据：P7_Alpha01_Level_v01.csv 的 RouteACells / RouteBCells；图形依据：P7_A1_ForkGateStates_v02.svg。',$small,$muted,52,945)

$canvas.Save($outputPath,[System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose();$canvas.Dispose();$terrain.Dispose();$gates.Dispose();$tJunction.Dispose();$gateA.Dispose();$gateB.Dispose()
foreach($f in @($title,$sub,$heading,$body,$small)){$f.Dispose()}
foreach($b in @($ink,$muted,$blue,$orange,$card)){$b.Dispose()}
$pen.Dispose()
Write-Output $outputPath
