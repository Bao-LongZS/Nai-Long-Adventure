param(
    [string]$SourcePdfPath = 'C:\Users\31314\Desktop\奶龙大冒险素材库\奶蛙学 必修第一册.pdf',
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$ueRoot = Split-Path $PSScriptRoot -Parent
$projectRoot = Split-Path $ueRoot -Parent
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    $OutputDirectory = Join-Path $projectRoot "tmp\P7_CatalogFigureRebuild_v02_$stamp"
}
if (-not (Test-Path -LiteralPath $SourcePdfPath)) { throw "Missing source PDF: $SourcePdfPath" }
$expectedPdfHash = '1b3d6198eb393e735d34c0ac0ccd6fbc8f1cfc5fb7e91847a7028e25057c9361'
$sourcePdfHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $SourcePdfPath).Hash.ToLowerInvariant()
if ($sourcePdfHash -ne $expectedPdfHash) { throw "Source PDF hash mismatch: $sourcePdfHash" }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$renderRoot = Join-Path $OutputDirectory 'SourcePages300dpi'
New-Item -ItemType Directory -Force -Path $renderRoot | Out-Null
$poppler = Get-Command pdftoppm -ErrorAction Stop

$items = @(
    @{ File = 'P7_A1_CharacterHUD_Laugh_Figure079_v02.png'; Page = 113; Box = @(55,415,200,560); Hash = '4f408dc01a5259479f237404875a8d54d1d5ef3f70f77c6f8da92e9fe1f43a3c' },
    @{ File = 'P7_A1_CharacterHUD_Distress_Figure106_v02.png'; Page = 115; Box = @(55,575,200,720); Hash = '1e78beb45e2a105e12541b4b4b27f2ab434cdcc07034f84d86e1eac67b5b1102' },
    @{ File = 'P7_A1_CharacterHUD_Victory_Figure121_v02.png'; Page = 117; Box = @(55,75,200,220); Hash = '220189369ab41ae78ab4cf168c2523ad4a2e1f3aa043bb0c2c67058425ed1fae' },
    @{ File = 'P7_A1_CharacterHUD_Idle_Figure139_v02.png'; Page = 118; Box = @(60,415,200,555); Hash = 'a6a0d8b28330da41fead19e5e074737de1e524547977048cab23e4b575c5fef9' }
)

$ratio = 300.0 / 72.0
foreach ($item in $items) {
    $outputPath = Join-Path $OutputDirectory $item.File
    if (Test-Path -LiteralPath $outputPath) { throw "Refusing to overwrite existing output: $outputPath" }
    $pagePath = Join-Path $renderRoot "page-$($item.Page).png"
    if (-not (Test-Path -LiteralPath $pagePath)) {
        $prefix = Join-Path $renderRoot 'page'
        & $poppler.Source -f $item.Page -l $item.Page -r 300 -png $SourcePdfPath $prefix
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $pagePath)) {
            throw "Could not render PDF page $($item.Page) with pdftoppm."
        }
    }

    $box = $item.Box
    $x = [int][Math]::Round($box[0] * $ratio)
    $y = [int][Math]::Round($box[1] * $ratio)
    $width = [int][Math]::Round(($box[2] - $box[0]) * $ratio)
    $height = [int][Math]::Round(($box[3] - $box[1]) * $ratio)
    $source = [System.Drawing.Bitmap]::new($pagePath)
    try {
        $rect = [System.Drawing.Rectangle]::new($x, $y, $width, $height)
        $crop = $source.Clone($rect, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
        try {
            $crop.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
        } finally {
            $crop.Dispose()
        }
    } finally {
        $source.Dispose()
    }

    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $outputPath).Hash.ToLowerInvariant()
    if ($actualHash -ne $item.Hash) { throw "Hash mismatch for $($item.File): $actualHash" }
    Write-Output "$($item.File) $($width)x$($height) SHA256=$actualHash"
}

Write-Output "Rebuilt image copies at: $OutputDirectory"
