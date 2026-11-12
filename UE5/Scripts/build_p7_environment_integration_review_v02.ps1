Add-Type -AssemblyName System.Drawing

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$envDir = Join-Path $root 'UE5\SourceAssets\P7\Environment\A1'
$manifestDir = Join-Path $root 'UE5\SourceAssets\P7\Manifests\A1'
$reviewDir = Join-Path $envDir 'Review'
$terrainPath = Join-Path $envDir 'P7_A1_TerrainTileAtlas_v02.png'
$gatePath = Join-Path $envDir 'P7_A1_ForkGateStates_v03.png'
$obstaclePath = Join-Path $envDir 'P7_A1_DestructibleObstacleAtlas_v02.png'
$terrainManifest = Import-Csv (Join-Path $manifestDir 'P7_A1_EnvironmentAssets_v02.csv')
$obstacleManifest = Import-Csv (Join-Path $manifestDir 'P7_A1_ObstacleAssets_v02.csv')
$outputPath = Join-Path $reviewDir 'P7_A1_TerrainObstacleIntegrationReview_v02.png'

New-Item -ItemType Directory -Force -Path $reviewDir | Out-Null

$width = 2200
$height = 1310
$cellW = 48
$cellH = 45
$mapW = 20 * $cellW
$mapH = 7 * $cellH
$canvas = [System.Drawing.Bitmap]::new($width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($canvas)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

$colors = @{
    Background = [System.Drawing.Color]::FromArgb(243, 240, 232)
    Card = [System.Drawing.Color]::FromArgb(255, 254, 250)
    Ink = [System.Drawing.Color]::FromArgb(48, 45, 39)
    Muted = [System.Drawing.Color]::FromArgb(102, 96, 84)
    Border = [System.Drawing.Color]::FromArgb(217, 210, 193)
    Teal = [System.Drawing.Color]::FromArgb(8, 123, 137)
    Orange = [System.Drawing.Color]::FromArgb(193, 131, 53)
    Mint = [System.Drawing.Color]::FromArgb(24, 158, 130)
    Grid = [System.Drawing.Color]::FromArgb(95, 97, 82, 56)
}
$brushes = @{}
foreach ($name in $colors.Keys) { $brushes[$name] = [System.Drawing.SolidBrush]::new($colors[$name]) }
$fonts = @{
    Title = [System.Drawing.Font]::new('Microsoft YaHei UI', 34, [System.Drawing.FontStyle]::Bold)
    Subtitle = [System.Drawing.Font]::new('Microsoft YaHei UI', 18, [System.Drawing.FontStyle]::Regular)
    PanelTitle = [System.Drawing.Font]::new('Microsoft YaHei UI', 24, [System.Drawing.FontStyle]::Bold)
    Detail = [System.Drawing.Font]::new('Microsoft YaHei UI', 15, [System.Drawing.FontStyle]::Regular)
    Tile = [System.Drawing.Font]::new('Microsoft YaHei UI', 15, [System.Drawing.FontStyle]::Bold)
    Small = [System.Drawing.Font]::new('Microsoft YaHei UI', 13, [System.Drawing.FontStyle]::Regular)
    Tiny = [System.Drawing.Font]::new('Microsoft YaHei UI', 11, [System.Drawing.FontStyle]::Regular)
}

$terrain = [System.Drawing.Bitmap]::new($terrainPath)
$gate = [System.Drawing.Bitmap]::new($gatePath)
$obstacles = [System.Drawing.Bitmap]::new($obstaclePath)

function Get-AtlasRow($Rows, [string]$AssetId) {
    $row = $Rows | Where-Object { $_.AssetId -eq $AssetId } | Select-Object -First 1
    if (-not $row) { throw "Missing art manifest row: $AssetId" }
    return $row
}

function Draw-Text([string]$Text, [float]$X, [float]$Y, $Font, $Brush) {
    $graphics.DrawString($Text, $Font, $Brush, $X, $Y)
}

function Draw-Card([int]$X, [int]$Y, [int]$W, [int]$H) {
    $graphics.FillRectangle($brushes.Card, $X, $Y, $W, $H)
    $pen = [System.Drawing.Pen]::new($colors.Border, 2)
    $graphics.DrawRectangle($pen, $X, $Y, $W, $H)
    $pen.Dispose()
}

function Draw-Crop($Image, [int]$SourceX, [int]$SourceY, [int]$SourceW, [int]$SourceH,
    [int]$DestX, [int]$DestY, [int]$DestW, [int]$DestH) {
    $sourceRect = [System.Drawing.Rectangle]::new($SourceX, $SourceY, $SourceW, $SourceH)
    $destRect = [System.Drawing.Rectangle]::new($DestX, $DestY, $DestW, $DestH)
    $graphics.DrawImage($Image, $destRect, $sourceRect, [System.Drawing.GraphicsUnit]::Pixel)
}

function Draw-TerrainCell([string]$AssetId, [int]$GridX, [int]$GridY, [int]$MapX, [int]$MapY) {
    $row = Get-AtlasRow $terrainManifest $AssetId
    $x = [int]$row.AtlasX
    $y = [int]$row.AtlasY
    Draw-Crop $terrain $x $y ([int]$row.AtlasW) ([int]$row.AtlasH) `
        ($MapX + $GridX * $cellW) ($MapY + ($GridY - 3) * $cellH) $cellW $cellH
}

function Draw-Obstacle([string]$AssetId, [int]$GridX, [int]$GridY, [int]$MapX, [int]$MapY) {
    $row = Get-AtlasRow $obstacleManifest $AssetId
    $size = 32
    $destX = $MapX + $GridX * $cellW + [int](($cellW - $size) / 2)
    $destY = $MapY + ($GridY - 3) * $cellH + [int](($cellH - $size) / 2)
    Draw-Crop $obstacles ([int]$row.AtlasX) ([int]$row.AtlasY) ([int]$row.AtlasW) ([int]$row.AtlasH) $destX $destY $size $size
}

function Draw-Gate([int]$GateIndex, [int]$GridX, [int]$GridY, [int]$MapX, [int]$MapY) {
    $sourceX = @(0, 984, 1968)[$GateIndex]
    Draw-Crop $gate $sourceX 0 960 528 ($MapX + $GridX * $cellW) ($MapY + ($GridY - 3) * $cellH) $cellW $cellH
}

function Draw-BuildSite([int]$GridX, [int]$GridY, [int]$MapX, [int]$MapY) {
    $x = $MapX + $GridX * $cellW + 13
    $y = $MapY + ($GridY - 3) * $cellH + 10
    $pen = [System.Drawing.Pen]::new($colors.Mint, 2)
    $graphics.DrawRectangle($pen, $x, $y, 22, 25)
    $pen.Dispose()
}

function Draw-Board([string]$Route, [int]$CardX, [int]$CardY) {
    $cardW = 1040
    Draw-Card $CardX $CardY $cardW 485
    $mapX = $CardX + 40
    $mapY = $CardY + 100
    if ($Route -eq 'A') {
        Draw-Text 'A 直行' ($CardX + 26) ($CardY + 18) $fonts.PanelTitle $brushes.Teal
        Draw-Text '20×11 逻辑格；显示游戏可见第 4–10 行，西口→东侧终点' ($CardX + 30) ($CardY + 55) $fonts.Detail $brushes.Muted
    } else {
        Draw-Text 'B 绕行' ($CardX + 26) ($CardY + 18) $fonts.PanelTitle $brushes.Orange
        Draw-Text '路径按 Level CSV：西口上折，北侧横行后向东汇合' ($CardX + 30) ($CardY + 55) $fonts.Detail $brushes.Muted
    }

    # Base field uses the atlas ground tile; path and junction units follow the actual Level CSV cells.
    for ($y = 3; $y -le 9; $y++) {
        for ($x = 0; $x -lt 20; $x++) {
            Draw-TerrainCell 'P7-ENV-A1-TILE-GROUND' $x $y $mapX $mapY
        }
    }

    if ($Route -eq 'A') {
        for ($x = 0; $x -lt 20; $x++) {
            if ($x -eq 1 -or $x -eq 18) { $tileId = 'P7-ENV-A1-TILE-T-MISSING-S' }
            elseif ($x -eq 0) { $tileId = 'P7-ENV-A1-TILE-ENTRY-CAP' }
            elseif ($x -eq 19) { $tileId = 'P7-ENV-A1-TILE-EXIT-CAP' }
            else { $tileId = 'P7-ENV-A1-TILE-STRAIGHT-H' }
            Draw-TerrainCell $tileId $x 5 $mapX $mapY
        }
        Draw-Gate 0 1 5 $mapX $mapY
        Draw-Gate 0 18 5 $mapX $mapY
    } else {
        Draw-TerrainCell 'P7-ENV-A1-TILE-ENTRY-CAP' 0 5 $mapX $mapY
        Draw-TerrainCell 'P7-ENV-A1-TILE-T-MISSING-S' 1 5 $mapX $mapY
        Draw-TerrainCell 'P7-ENV-A1-TILE-CORNER-ES' 1 4 $mapX $mapY
        for ($x = 2; $x -le 17; $x++) { Draw-TerrainCell 'P7-ENV-A1-TILE-STRAIGHT-H' $x 4 $mapX $mapY }
        Draw-TerrainCell 'P7-ENV-A1-TILE-CORNER-SW' 18 4 $mapX $mapY
        Draw-TerrainCell 'P7-ENV-A1-TILE-T-MISSING-S' 18 5 $mapX $mapY
        Draw-TerrainCell 'P7-ENV-A1-TILE-EXIT-CAP' 19 5 $mapX $mapY
        Draw-Gate 1 1 5 $mapX $mapY
        Draw-Gate 2 18 5 $mapX $mapY
    }

    # Static sites are shown as a placement reference only; these outlines are not runtime art.
    foreach ($x in @(2, 4, 6, 8, 11, 13, 15, 17)) { Draw-BuildSite $x 7 $mapX $mapY }
    Draw-Obstacle 'P7-ENV-A1-OBST-CRATE-INTACT' 4 9 $mapX $mapY
    Draw-Obstacle 'P7-ENV-A1-OBST-OAT-STACK-INTACT' 15 9 $mapX $mapY

    $gridPen = [System.Drawing.Pen]::new($colors.Grid, 1)
    for ($x = 0; $x -le 20; $x++) { $graphics.DrawLine($gridPen, $mapX + $x * $cellW, $mapY, $mapX + $x * $cellW, $mapY + $mapH) }
    for ($y = 0; $y -le 7; $y++) { $graphics.DrawLine($gridPen, $mapX, $mapY + $y * $cellH, $mapX + $mapW, $mapY + $y * $cellH) }
    $gridPen.Dispose()



    Draw-Text '绿色框：建造位；障碍外形仅作示例；边框是评审坐标，非运行时网格' ($CardX + 30) ($CardY + 430) $fonts.Small $brushes.Muted
}

$graphics.Clear($colors.Background)
Draw-Text 'P7 A1 环境候选 · Alpha01 棋盘拼装复核 v02' 48 34 $fonts.Title $brushes.Ink
Draw-Text '路线地面 v02 + 已通过的 T 形门 v03 + 障碍 v02；按当前可视网格预览，不是 UE 运行截图。' 52 86 $fonts.Subtitle $brushes.Muted
Draw-Board 'A' 40 132
Draw-Board 'B' 1120 132

Draw-Text '地面图集 v02 · 14 个单元（48px 目标棋盘缩略仅用于查看组合，不代表 UE 最终采样）' 48 644 $fonts.PanelTitle $brushes.Ink
$tileLabels = @(
    '地面填充', '直线路径·横', '直线路径·竖', '弯道·北→东', '弯道·东→南', '弯道·南→西', '弯道·西→北',
    '入口端', '出口端', 'T口·缺北', 'T口·缺东', 'T口·缺南', 'T口·缺西', 'Y形分岔·本关未用'
)
for ($i = 0; $i -lt 14; $i++) {
    $column = $i % 7
    $row = [int][Math]::Floor($i / 7)
    $x = 40 + $column * 303
    $y = 688 + $row * 137
    Draw-Card $x $y 294 126
    $assetRow = $terrainManifest | Where-Object { [int]$_.FrameIndex -eq $i } | Select-Object -First 1
    if (-not $assetRow) { throw "Missing terrain frame index: $i" }
    Draw-Crop $terrain ([int]$assetRow.AtlasX) ([int]$assetRow.AtlasY) ([int]$assetRow.AtlasW) ([int]$assetRow.AtlasH) ($x + 12) ($y + 19) 144 79
    Draw-Text $tileLabels[$i] ($x + 164) ($y + 44) $fonts.Tile $brushes.Ink
    Draw-Text ('F{0:D2}' -f $i) ($x + 164) ($y + 72) $fonts.Small $brushes.Muted
}

Draw-Text '可破坏物 v02 · 两种外形 × 三种损伤状态' 48 982 $fonts.PanelTitle $brushes.Ink
$obstacleGroups = @(
    @{ X = 40; Title = '奶油饼干箱'; Ids = @('P7-ENV-A1-OBST-CRATE-INTACT','P7-ENV-A1-OBST-CRATE-CRACKED','P7-ENV-A1-OBST-CRATE-NEARBREAK') },
    @{ X = 1120; Title = '燕麦饼叠垛'; Ids = @('P7-ENV-A1-OBST-OAT-STACK-INTACT','P7-ENV-A1-OBST-OAT-STACK-CRACKED','P7-ENV-A1-OBST-OAT-STACK-NEARBREAK') }
)
$damageLabels = @('完好','裂损','将碎')
foreach ($group in $obstacleGroups) {
    Draw-Card $group.X 1024 1040 224
    Draw-Text $group.Title ($group.X + 22) 1038 $fonts.Tile $brushes.Ink
    for ($i = 0; $i -lt 3; $i++) {
        $row = Get-AtlasRow $obstacleManifest $group.Ids[$i]
        $x = $group.X + 56 + $i * 315
        Draw-Crop $obstacles ([int]$row.AtlasX) ([int]$row.AtlasY) ([int]$row.AtlasW) ([int]$row.AtlasH) $x 1070 112 112
        Draw-Text $damageLabels[$i] ($x + 119) 1112 $fonts.Tile $brushes.Muted
    }
}
Draw-Text '图块、障碍仍是视觉候选；A/B 组合按 P7_Alpha01_Level_v01.csv 的 20×11 CellId 装配。状态图不表示已经通过或导入。' 48 1262 $fonts.Small $brushes.Muted

$canvas.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)

$terrain.Dispose()
$gate.Dispose()
$obstacles.Dispose()
$graphics.Dispose()
$canvas.Dispose()
foreach ($brush in $brushes.Values) { $brush.Dispose() }
foreach ($font in $fonts.Values) { $font.Dispose() }
Write-Output $outputPath

