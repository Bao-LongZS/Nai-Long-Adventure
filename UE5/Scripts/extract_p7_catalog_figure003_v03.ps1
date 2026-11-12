param()

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$ueRoot = Split-Path $PSScriptRoot -Parent
$projectRoot = Split-Path $ueRoot -Parent
$sourcePdf = 'C:\Users\31314\Desktop\奶龙大冒险素材库\奶蛙学 必修第一册.pdf'
$pdfHashExpected = '1b3d6198eb393e735d34c0ac0ccd6fbc8f1cfc5fb7e91847a7028e25057c9361'
$sourceRoot = Join-Path $ueRoot 'SourceAssets\P7\Characters\A1\CatalogDirect'
$tempRoot = Join-Path $projectRoot 'tmp\P7_CatalogFigure003_v03'
$renderRoot = Join-Path $tempRoot 'SourcePages300dpi'
$pagePath = Join-Path $renderRoot 'page-107.png'
$outputPath = Join-Path $sourceRoot 'P7_A1_CharacterHUD_Laugh_Figure003_v03.png'

if (-not (Test-Path -LiteralPath $sourcePdf)) { throw "Missing source PDF: $sourcePdf" }
$pdfHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourcePdf).Hash.ToLowerInvariant()
if ($pdfHash -ne $pdfHashExpected) { throw "Source PDF hash mismatch: $pdfHash" }
if (Test-Path -LiteralPath $outputPath) { throw "Refusing to overwrite existing asset: $outputPath" }
New-Item -ItemType Directory -Force -Path $renderRoot | Out-Null

if (-not (Test-Path -LiteralPath $pagePath)) {
    $poppler = Get-Command pdftoppm -ErrorAction Stop
    & $poppler.Source -f 107 -l 107 -r 300 -png $sourcePdf (Join-Path $renderRoot 'page')
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $pagePath)) {
        throw 'Could not render PDF page 107 with pdftoppm.'
    }
}

# Figure 003 is the standing “捧腹大笑” pose on PDF page 107 (printed book page 99).
# Crop a 145 pt square around the complete figure, before its printed caption.
$ratio = 300.0 / 72.0
$x = [int][Math]::Round(377.0 * $ratio)
$y = [int][Math]::Round(88.0 * $ratio)
$side = [int][Math]::Round(145.0 * $ratio)
$source = [System.Drawing.Bitmap]::new($pagePath)
try {
    $rect = [System.Drawing.Rectangle]::new($x, $y, $side, $side)
    if ($rect.Right -gt $source.Width -or $rect.Bottom -gt $source.Height) {
        throw "Crop rectangle exceeds rendered PDF page: $rect on $($source.Width)x$($source.Height)"
    }
    $crop = $source.Clone($rect, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    try { $crop.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png) }
    finally { $crop.Dispose() }
}
finally { $source.Dispose() }

$result = [System.Drawing.Bitmap]::new($outputPath)
try {
    if ($result.Width -ne 604 -or $result.Height -ne 604) {
        throw "Unexpected crop size: $($result.Width)x$($result.Height)"
    }
}
finally { $result.Dispose() }

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $outputPath).Hash.ToLowerInvariant()
Write-Output "$outputPath`n604x604 RGB PNG`nSHA256=$hash`nSource PDF SHA256=$pdfHash"
