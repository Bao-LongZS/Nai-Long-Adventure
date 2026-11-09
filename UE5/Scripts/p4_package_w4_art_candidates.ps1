param(
    [string]$BossSource = 'UE5/SourceAssets/P4/Characters/Alpha/W4_Boss/RuntimeWIP/SourceFrames',
    [string]$BossOutput = 'UE5/SourceAssets/P4/Characters/Alpha/W4_Boss/RuntimeWIP/Frames512',
    [string]$FxSource = 'UE5/SourceAssets/P4/Effects/Alpha/W4/RuntimeWIP/SourceFrames',
    [string]$FxOutput = 'UE5/SourceAssets/P4/Effects/Alpha/W4/RuntimeWIP/Frames512',
    [int]$CanvasSize = 512,
    [int]$BossFootAnchorY = 466,
    [int]$AlphaThreshold = 8
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function Get-VisibleBounds {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [int]$Threshold
    )

    $left = $Bitmap.Width
    $top = $Bitmap.Height
    $right = -1
    $bottom = -1

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

    if ($right -lt $left -or $bottom -lt $top) {
        throw 'Input image contains no visible pixels above the alpha threshold.'
    }

    return [System.Drawing.Rectangle]::new($left, $top, $right - $left + 1, $bottom - $top + 1)
}

function Export-CanvasCandidate {
    param(
        [string]$InputPath,
        [string]$OutputPath,
        [ValidateSet('boss', 'effect')][string]$Kind,
        [int]$CanvasSide,
        [int]$FootY,
        [int]$Threshold
    )

    $source = [System.Drawing.Bitmap]::FromFile((Resolve-Path -LiteralPath $InputPath).Path)
    $canvasBitmap = [System.Drawing.Bitmap]::new($CanvasSide, $CanvasSide, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($canvasBitmap)
    try {
        $bounds = Get-VisibleBounds -Bitmap $source -Threshold $Threshold
        if ($Kind -eq 'boss') {
            $maxWidth = 440.0
            $maxHeight = [double]($FootY - 24)
            $scale = [Math]::Min($maxWidth / $bounds.Width, $maxHeight / $bounds.Height)
            $targetWidth = [Math]::Max(1, [int][Math]::Round($bounds.Width * $scale))
            $targetHeight = [Math]::Max(1, [int][Math]::Round($bounds.Height * $scale))
            $destinationX = [int][Math]::Round(($CanvasSide - $targetWidth) / 2.0)
            $destinationY = $FootY - $targetHeight
        }
        else {
            $maxExtent = 440.0
            $scale = [Math]::Min($maxExtent / $bounds.Width, $maxExtent / $bounds.Height)
            $targetWidth = [Math]::Max(1, [int][Math]::Round($bounds.Width * $scale))
            $targetHeight = [Math]::Max(1, [int][Math]::Round($bounds.Height * $scale))
            $destinationX = [int][Math]::Round(($CanvasSide - $targetWidth) / 2.0)
            $destinationY = [int][Math]::Round(($CanvasSide - $targetHeight) / 2.0)
        }

        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $sourceRectangle = [System.Drawing.Rectangle]::new($bounds.X, $bounds.Y, $bounds.Width, $bounds.Height)
        $destinationRectangle = [System.Drawing.Rectangle]::new($destinationX, $destinationY, $targetWidth, $targetHeight)
        $graphics.DrawImage($source, $destinationRectangle, $sourceRectangle, [System.Drawing.GraphicsUnit]::Pixel)

        $parent = Split-Path -Parent $OutputPath
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
        $canvasBitmap.Save((Join-Path (Get-Location) $OutputPath), [System.Drawing.Imaging.ImageFormat]::Png)

        $workingRoot = (Get-Location).Path
        $inputRelativePath = $InputPath.Substring($workingRoot.Length).TrimStart([char[]]@('\', '/')) -replace '\\', '/'
        $outputRelativePath = $OutputPath -replace '\\', '/'
        [pscustomobject]@{
            Input = $inputRelativePath
            Output = $outputRelativePath
            Kind = $Kind
            SourceSize = "$($source.Width)x$($source.Height)"
            VisibleBounds = "$($bounds.X),$($bounds.Y),$($bounds.Width),$($bounds.Height)"
            CandidateSize = "$CanvasSide`x$CanvasSide"
            Anchor = if ($Kind -eq 'boss') { "256,$FootY" } else { '256,256' }
        }
    }
    finally {
        $graphics.Dispose()
        $canvasBitmap.Dispose()
        $source.Dispose()
    }
}

$bossSourcePath = (Resolve-Path -LiteralPath $BossSource).Path
$fxSourcePath = (Resolve-Path -LiteralPath $FxSource).Path
$results = [System.Collections.Generic.List[object]]::new()

Get-ChildItem -LiteralPath $bossSourcePath -File -Filter '*.png' | Sort-Object Name | ForEach-Object {
    $relativeName = $_.Name -replace '_source(?=\.png$)', ''
    $outputPath = Join-Path $BossOutput $relativeName
    $results.Add((Export-CanvasCandidate -InputPath $_.FullName -OutputPath $outputPath -Kind boss -CanvasSide $CanvasSize -FootY $BossFootAnchorY -Threshold $AlphaThreshold))
}

Get-ChildItem -LiteralPath $fxSourcePath -File -Filter '*.png' | Sort-Object Name | ForEach-Object {
    $relativeName = $_.Name -replace '_source(?=\.png$)', ''
    $outputPath = Join-Path $FxOutput $relativeName
    $results.Add((Export-CanvasCandidate -InputPath $_.FullName -OutputPath $outputPath -Kind effect -CanvasSide $CanvasSize -FootY $BossFootAnchorY -Threshold $AlphaThreshold))
}

$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath 'UE5/SourceAssets/P4/Manifests/P4_W4_CandidatePackaging_report_v01.json' -Encoding utf8
$results | Format-Table -AutoSize
