[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$AssetRoot = Join-Path $RepositoryRoot 'UE5\SourceAssets\P7\UI\A1'
$Manifest = Join-Path $RepositoryRoot 'UE5\SourceAssets\P7\Manifests\A1\P7_A1_HUDIconAssets_v02.csv'
$AtlasPath = Join-Path $AssetRoot 'P7_A1_HUDIconAtlas_v02.png'
$OutputPath = Join-Path $AssetRoot 'Review\P7_A1_HUDIconReadability_48px_v02.png'

foreach ($path in @($Manifest, $AtlasPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required review input is missing: $path" }
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null

$RendererSource = @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;

public static class P7HudIconReadabilityRenderer
{
    static readonly Color Navy = ColorTranslator.FromHtml("#14264A");
    static readonly Color Muted = ColorTranslator.FromHtml("#667086");
    static readonly Color Border = ColorTranslator.FromHtml("#DFE3E8");

    static Bitmap Gray(Bitmap source)
    {
        Bitmap result = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb);
        for (int y = 0; y < source.Height; ++y)
        for (int x = 0; x < source.Width; ++x)
        {
            Color p = source.GetPixel(x, y);
            int gray = (int)Math.Round(0.299 * p.R + 0.587 * p.G + 0.114 * p.B);
            result.SetPixel(x, y, Color.FromArgb(p.A, gray, gray, gray));
        }
        return result;
    }

    static Bitmap At48(Bitmap atlas, Rectangle source, bool grayscale)
    {
        using (Bitmap crop = atlas.Clone(source, PixelFormat.Format32bppArgb))
        using (Bitmap colorized = grayscale ? Gray(crop) : (Bitmap)crop.Clone())
        using (Bitmap small = new Bitmap(48, 48, PixelFormat.Format32bppArgb))
        using (Graphics g = Graphics.FromImage(small))
        {
            g.Clear(Color.Transparent);
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.CompositingQuality = CompositingQuality.HighQuality;
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.DrawImage(colorized, new Rectangle(0, 0, 48, 48));
            Bitmap enlarged = new Bitmap(96, 96, PixelFormat.Format32bppArgb);
            using (Graphics display = Graphics.FromImage(enlarged))
            {
                display.Clear(Color.Transparent);
                display.SmoothingMode = SmoothingMode.None;
                display.InterpolationMode = InterpolationMode.NearestNeighbor;
                display.PixelOffsetMode = PixelOffsetMode.Half;
                display.DrawImage(small, new Rectangle(0, 0, 96, 96));
            }
            return enlarged;
        }
    }

    public static void Render(string atlasPath, string outputPath, string[] ids, string[] uses, int[] xs, int[] ys)
    {
        if (ids.Length != 16 || uses.Length != 16 || xs.Length != 16 || ys.Length != 16)
            throw new ArgumentException("The P7 HUD v02 preview expects exactly 16 manifest rows.");
        using (Bitmap atlas = new Bitmap(atlasPath))
        using (Bitmap board = new Bitmap(1600, 1380, PixelFormat.Format32bppArgb))
        using (Graphics g = Graphics.FromImage(board))
        using (Font title = new Font("Microsoft YaHei UI", 33, FontStyle.Bold, GraphicsUnit.Pixel))
        using (Font subtitle = new Font("Microsoft YaHei UI", 17, FontStyle.Regular, GraphicsUnit.Pixel))
        using (Font idFont = new Font("Microsoft YaHei UI", 14, FontStyle.Bold, GraphicsUnit.Pixel))
        using (Font useFont = new Font("Microsoft YaHei UI", 15, FontStyle.Regular, GraphicsUnit.Pixel))
        using (Font noteFont = new Font("Microsoft YaHei UI", 14, FontStyle.Bold, GraphicsUnit.Pixel))
        using (SolidBrush background = new SolidBrush(ColorTranslator.FromHtml("#F4F2EC")))
        using (SolidBrush card = new SolidBrush(Color.White))
        using (SolidBrush navy = new SolidBrush(Navy))
        using (SolidBrush muted = new SolidBrush(Muted))
        using (Pen separator = new Pen(Border, 2))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.Clear(background.Color);
            g.DrawString("P7 / A1 HUD 图标可读性 · 48px v02", title, navy, 64, 36);
            g.DrawString("彩色与灰度各按 48px 缩小，再放大 2× 显示；用于查看小尺寸符号辨认，不替代 UE 实机与按钮 hitbox 验收。", subtitle, muted, 66, 82);

            for (int i = 0; i < 16; ++i)
            {
                int column = i % 4;
                int row = i / 4;
                int x = 64 + column * 368;
                int y = 132 + row * 300;
                using (GraphicsPath path = RoundedCard(x, y, 352, 278, 22)) g.FillPath(card, path);
                g.DrawString(ids[i], idFont, navy, x + 16, y + 13);
                g.DrawString(uses[i], useFont, muted, x + 16, y + 39);

                using (Bitmap color = At48(atlas, new Rectangle(xs[i], ys[i], 256, 256), false))
                    g.DrawImage(color, x + 20, y + 79, 96, 96);
                using (Bitmap gray = At48(atlas, new Rectangle(xs[i], ys[i], 256, 256), true))
                    g.DrawImage(gray, x + 150, y + 79, 96, 96);

                g.DrawString("48px 彩色（2×）", noteFont, muted, x + 18, y + 190);
                g.DrawString("48px 灰度（2×）", noteFont, muted, x + 146, y + 190);
                g.DrawLine(separator, x + 16, y + 226, x + 336, y + 226);
                g.DrawString("切片 256×256 · 24px 外边距/格间距", subtitle, muted, x + 16, y + 238);
            }

            board.Save(outputPath, ImageFormat.Png);
        }
    }

    static GraphicsPath RoundedCard(float x, float y, float width, float height, float radius)
    {
        GraphicsPath path = new GraphicsPath();
        float diameter = radius * 2;
        path.AddArc(x, y, diameter, diameter, 180, 90);
        path.AddArc(x + width - diameter, y, diameter, diameter, 270, 90);
        path.AddArc(x + width - diameter, y + height - diameter, diameter, diameter, 0, 90);
        path.AddArc(x, y + height - diameter, diameter, diameter, 90, 90);
        path.CloseFigure();
        return path;
    }
}
'@

Add-Type -AssemblyName System.Drawing
$DrawingAssemblies = @(
    [System.Drawing.Bitmap].Assembly.Location
    [System.Drawing.Color].Assembly.Location
)
foreach ($assemblyName in [System.Drawing.Bitmap].Assembly.GetReferencedAssemblies() |
    Where-Object { $_.Name -like 'System.Private.Windows.*' }) {
    $DrawingAssemblies += [System.Reflection.Assembly]::Load($assemblyName).Location
}
Add-Type -TypeDefinition $RendererSource -ReferencedAssemblies $DrawingAssemblies

$rows = @(Import-Csv -LiteralPath $Manifest)
if ($rows.Count -ne 16) { throw "Expected 16 HUD manifest rows; found $($rows.Count)" }
$ids = [string[]]@($rows | ForEach-Object AssetId)
$uses = [string[]]@($rows | ForEach-Object IntendedUse)
$xs = [int[]]@($rows | ForEach-Object { [int]$_.AtlasX })
$ys = [int[]]@($rows | ForEach-Object { [int]$_.AtlasY })
[P7HudIconReadabilityRenderer]::Render($AtlasPath, $OutputPath, $ids, $uses, $xs, $ys)

Write-Output "Generated the 48px color/grayscale HUD icon review sheet: $OutputPath"
