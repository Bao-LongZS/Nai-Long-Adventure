param()

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$ueRoot = Split-Path $PSScriptRoot -Parent
$reviewRoot = Join-Path $ueRoot 'SourceAssets\P7\Characters\A1\Review'
$evidenceRoot = Join-Path $ueRoot 'Records\p7_td\evidence'
$basePath = Join-Path $evidenceRoot 'P7_ChineseMapUI_DesktopPreview_1280x720_2026-11-11_22-02-12.png'
$winPath = Join-Path $ueRoot 'SourceAssets\P7\Characters\A1\CatalogDirect\P7_A1_CharacterHUD_Victory_Figure121_v02.png'
$lossPath = Join-Path $ueRoot 'SourceAssets\P7\Characters\A1\CatalogDirect\P7_A1_CharacterHUD_Distress_Figure106_v02.png'
$winOut720 = Join-Path $reviewRoot 'P7_A1_CatalogResult_WinMock_v01_1280x720.png'
$lossOut720 = Join-Path $reviewRoot 'P7_A1_CatalogResult_LossMock_v01_1280x720.png'
$winOut1080 = Join-Path $reviewRoot 'P7_A1_CatalogResult_WinMock_v01_1920x1080.png'
$lossOut1080 = Join-Path $reviewRoot 'P7_A1_CatalogResult_LossMock_v01_1920x1080.png'
$boardOut = Join-Path $reviewRoot 'P7_A1_CatalogResultPanelMockReview_v01.png'

foreach ($path in @($basePath, $winPath, $lossPath)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing input image: $path" }
}

$base = [System.Drawing.Image]::FromFile($basePath)
$winArt = [System.Drawing.Image]::FromFile($winPath)
$lossArt = [System.Drawing.Image]::FromFile($lossPath)
function Draw-ResultScreen {
    param(
        [System.Drawing.Image]$Base,
        [System.Drawing.Image]$Portrait,
        [string]$State,
        [int]$Width,
        [int]$Height,
        [string]$OutputPath
    )

    $scale = $Height / 720.0
    $bitmap = [System.Drawing.Bitmap]::new($Width, $Height)
    $g = [System.Drawing.Graphics]::FromImage($bitmap)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.DrawImage($Base, 0, 0, $Width, $Height)
    $g.FillRectangle([System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(174, 4, 7, 10)), 0, 0, $Width, $Height)

    $panelWidth = [int](480 * $scale)
    $panelHeight = [int](228 * $scale)
    $panelX = [int](($Width - $panelWidth) / 2)
    $panelY = [int](($Height - $panelHeight) / 2)
    $panelRect = [System.Drawing.Rectangle]::new($panelX, $panelY, $panelWidth, $panelHeight)
    $panelFill = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(250, 9, 14, 19))
    $g.FillRectangle($panelFill, $panelRect)

    if ($State -eq 'Win') {
        $accent = [System.Drawing.Color]::FromArgb(75, 211, 168)
        $title = '守住了！奶罐平安'
        $body = '八波敌人已全部击退。'
        $summary = '奶罐无伤：达成   清障：完成 2 / 2'
    } else {
        $accent = [System.Drawing.Color]::FromArgb(241, 142, 82)
        $title = '奶罐被打破了'
        $body = '重新布防后再试一次。'
        $summary = '奶罐状态：失守   清障：1 / 2'
    }

    $borderPen = [System.Drawing.Pen]::new($accent, [single](1.8 * $scale))
    $g.DrawRectangle($borderPen, $panelRect)

    $tileSize = [int](152 * $scale)
    $tileX = [int]($panelX + 20 * $scale)
    $tileY = [int]($panelY + ($panelHeight - $tileSize) / 2)
    $tileRect = [System.Drawing.Rectangle]::new($tileX, $tileY, $tileSize, $tileSize)
    $g.FillRectangle([System.Drawing.Brushes]::White, $tileRect)
    $g.DrawImage($Portrait, $tileRect)
    $tilePen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(240, 238, 231), [single](1.0 * $scale))
    $g.DrawRectangle($tilePen, $tileRect)

    $textX = $panelX + [int](190 * $scale)
    $titleFont = [System.Drawing.Font]::new('Microsoft YaHei UI', [single](20 * $scale), [System.Drawing.FontStyle]::Bold)
    $bodyFont = [System.Drawing.Font]::new('Microsoft YaHei UI', [single](13 * $scale), [System.Drawing.FontStyle]::Regular)
    $smallFont = [System.Drawing.Font]::new('Microsoft YaHei UI', [single](11 * $scale), [System.Drawing.FontStyle]::Regular)
    $buttonFont = [System.Drawing.Font]::new('Microsoft YaHei UI', [single](12 * $scale), [System.Drawing.FontStyle]::Bold)
    $titleBrush = [System.Drawing.SolidBrush]::new($accent)
    $bodyBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(238, 239, 236))
    $mutedBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(170, 183, 187))
    $g.DrawString($title, $titleFont, $titleBrush, $textX, $panelY + [int](37 * $scale))
    $g.DrawString($body, $bodyFont, $bodyBrush, $textX, $panelY + [int](76 * $scale))
    $g.DrawString($summary, $smallFont, $mutedBrush, $textX, $panelY + [int](107 * $scale))

    $buttonWidth = [int](128 * $scale)
    $buttonHeight = [int](34 * $scale)
    $buttonRect = [System.Drawing.Rectangle]::new($textX, $panelY + [int](158 * $scale), $buttonWidth, $buttonHeight)
    $buttonFill = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 19, 31, 37))
    $buttonPen = [System.Drawing.Pen]::new($accent, [single](1.3 * $scale))
    $g.FillRectangle($buttonFill, $buttonRect)
    $g.DrawRectangle($buttonPen, $buttonRect)
    $buttonSize = $g.MeasureString('再来一局', $buttonFont)
    $g.DrawString('再来一局', $buttonFont, $bodyBrush,
        $buttonRect.X + ($buttonRect.Width - $buttonSize.Width) / 2,
        $buttonRect.Y + ($buttonRect.Height - $buttonSize.Height) / 2)

    $labelFont = [System.Drawing.Font]::new('Microsoft YaHei UI', [single](10 * $scale), [System.Drawing.FontStyle]::Regular)
    $labelBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(220, 235, 238, 234))
    $label = "构图草案 · 图录原图 · ${Width}×${Height}"
    $g.DrawString($label, $labelFont, $labelBrush, $Width - [int](300 * $scale), [int](18 * $scale))

    $bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bitmap.Dispose(); $panelFill.Dispose(); $borderPen.Dispose()
    $tilePen.Dispose()
    $titleFont.Dispose(); $bodyFont.Dispose(); $smallFont.Dispose(); $buttonFont.Dispose(); $labelFont.Dispose()
    $titleBrush.Dispose(); $bodyBrush.Dispose(); $mutedBrush.Dispose(); $buttonFill.Dispose(); $buttonPen.Dispose(); $labelBrush.Dispose()
}

Draw-ResultScreen -Base $base -Portrait $winArt -State 'Win' -Width 1280 -Height 720 -OutputPath $winOut720
Draw-ResultScreen -Base $base -Portrait $lossArt -State 'Loss' -Width 1280 -Height 720 -OutputPath $lossOut720
Draw-ResultScreen -Base $base -Portrait $winArt -State 'Win' -Width 1920 -Height 1080 -OutputPath $winOut1080
Draw-ResultScreen -Base $base -Portrait $lossArt -State 'Loss' -Width 1920 -Height 1080 -OutputPath $lossOut1080

$board = [System.Drawing.Bitmap]::new(1600, 940)
$bg = [System.Drawing.Graphics]::FromImage($board)
$bg.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$bg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$bg.Clear([System.Drawing.Color]::FromArgb(242, 240, 232))
$headline = [System.Drawing.Font]::new('Microsoft YaHei UI', 28, [System.Drawing.FontStyle]::Bold)
$caption = [System.Drawing.Font]::new('Microsoft YaHei UI', 14, [System.Drawing.FontStyle]::Regular)
$ink = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(42, 48, 54))
$muted = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(94, 98, 98))
$bg.DrawString('P7 结算面板图录姿态构图草案', $headline, $ink, 48, 28)
$bg.DrawString('沿用当前灰盒结算面板尺寸；图录白底作为固定头像卡片，胜利/失守各展示一张。', $caption, $muted, 52, 76)
$screenWidth = 736
$screenHeight = 414
$winPreview = [System.Drawing.Image]::FromFile($winOut720)
$lossPreview = [System.Drawing.Image]::FromFile($lossOut720)
$bg.DrawImage($winPreview, 48, 126, $screenWidth, $screenHeight)
$bg.DrawImage($lossPreview, 816, 126, $screenWidth, $screenHeight)
$bg.DrawString('胜利 · 图录 121 滑跪庆祝', $caption, $ink, 52, 550)
$bg.DrawString('失守 · 图录 106 失落蹲坐', $caption, $ink, 820, 550)
$bg.DrawString('来源页裁切图保留白底，不作为透明 Sprite；两张构图均未导入 UE。人物与结算标题共用事件语义。', $caption, $muted, 52, 594)
$bg.DrawString('用 1280×720 灰盒截图作底，结算窗口按现有 HUD 480×228 逻辑尺寸绘制；1080p 版本按 HUDScale 1.5 放大。', $caption, $muted, 52, 620)
$bg.DrawString('屏幕样片仅用于构图评审，不是 UE 实机截图；结算数字为版式示意。', $caption, $muted, 52, 646)
$board.Save($boardOut, [System.Drawing.Imaging.ImageFormat]::Png)

$winPreview.Dispose(); $lossPreview.Dispose(); $bg.Dispose(); $board.Dispose(); $base.Dispose(); $winArt.Dispose(); $lossArt.Dispose()
$headline.Dispose(); $caption.Dispose(); $ink.Dispose(); $muted.Dispose()
Write-Output $boardOut
Write-Output $winOut720
Write-Output $lossOut720
Write-Output $winOut1080
Write-Output $lossOut1080
