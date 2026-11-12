[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$AssetRoot = Join-Path $RepositoryRoot 'UE5\SourceAssets\P7\UI\A1'
$ReviewRoot = Join-Path $AssetRoot 'Review'
$AtlasSvg = Join-Path $AssetRoot 'P7_A1_ResultBadgeAtlas_v02.svg'
$AtlasPng = Join-Path $AssetRoot 'P7_A1_ResultBadgeAtlas_v02.png'
$ReviewPng = Join-Path $ReviewRoot 'P7_A1_ResultBadgeReview_v02.png'
$Manifest = Join-Path $RepositoryRoot 'UE5\SourceAssets\P7\Manifests\A1\P7_A1_ResultBadgeAssets_v02.csv'

foreach ($path in @($AssetRoot, $ReviewRoot, (Split-Path -Parent $Manifest))) {
    New-Item -ItemType Directory -Force -Path $path | Out-Null
}
if (-not (Test-Path -LiteralPath $AtlasSvg -PathType Leaf)) {
    throw "Result badge SVG source is missing: $AtlasSvg"
}

Add-Type -AssemblyName System.Drawing
$RendererSource = @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;

public static class P7ResultBadgeRenderer
{
    static readonly Color Navy = ColorTranslator.FromHtml("#14264A");
    static readonly Color Cream = ColorTranslator.FromHtml("#FFF3D7");
    static readonly Color Gold = ColorTranslator.FromHtml("#F4C84A");
    static readonly Color Ochre = ColorTranslator.FromHtml("#9A5A2E");
    static readonly Color Mint = ColorTranslator.FromHtml("#55B97C");
    static readonly Color PaleMint = ColorTranslator.FromHtml("#E6F3E5");
    static readonly Color Coral = ColorTranslator.FromHtml("#EF6256");
    static readonly Color PaleCoral = ColorTranslator.FromHtml("#FBE7DE");

    static Pen MakePen(Color color, float width)
    {
        Pen pen = new Pen(color, width);
        pen.LineJoin = LineJoin.Round;
        pen.StartCap = LineCap.Round;
        pen.EndCap = LineCap.Round;
        return pen;
    }

    static GraphicsPath ShieldPath(float left, float top, float width, float height)
    {
        float right = left + width;
        float center = left + width * 0.5f;
        float shoulder = top + height * 0.14f;
        float bottom = top + height;
        GraphicsPath path = new GraphicsPath();
        path.StartFigure();
        path.AddBezier(center, top, left + width * 0.39f, top, left + width * 0.23f, top + height * 0.04f, left + width * 0.09f, shoulder);
        path.AddLine(left + width * 0.09f, shoulder, left + width * 0.09f, top + height * 0.55f);
        path.AddBezier(left + width * 0.09f, top + height * 0.55f, left + width * 0.09f, top + height * 0.76f, left + width * 0.32f, top + height * 0.91f, center, bottom);
        path.AddBezier(center, bottom, right - width * 0.32f, top + height * 0.91f, right - width * 0.09f, top + height * 0.76f, right - width * 0.09f, top + height * 0.55f);
        path.AddLine(right - width * 0.09f, top + height * 0.55f, right - width * 0.09f, shoulder);
        path.AddBezier(right - width * 0.09f, shoulder, right - width * 0.23f, top + height * 0.04f, right - width * 0.39f, top, center, top);
        path.CloseFigure();
        return path;
    }

    static GraphicsPath OuterLossPath()
    {
        GraphicsPath path = new GraphicsPath();
        path.StartFigure();
        path.AddBezier(192, 25, 256, 25, 315, 50, 342, 98);
        path.AddLine(342, 98, 342, 217);
        path.AddBezier(342, 217, 342, 278, 283, 330, 192, 359);
        path.AddBezier(192, 359, 101, 330, 42, 278, 42, 217);
        path.AddLine(42, 217, 42, 98);
        path.AddBezier(42, 98, 69, 50, 128, 25, 192, 25);
        path.CloseFigure();
        return path;
    }

    static GraphicsPath StarPath(float cx, float cy, float outer, float inner, int points)
    {
        PointF[] vertices = new PointF[points * 2];
        for (int i = 0; i < vertices.Length; ++i)
        {
            double angle = -Math.PI * 0.5 + Math.PI * i / points;
            float radius = (i % 2 == 0) ? outer : inner;
            vertices[i] = new PointF(cx + (float)Math.Cos(angle) * radius, cy + (float)Math.Sin(angle) * radius);
        }
        GraphicsPath path = new GraphicsPath();
        path.AddPolygon(vertices);
        return path;
    }

    static void DrawLeaf(Graphics g, float cx, float cy, float angle)
    {
        GraphicsState state = g.Save();
        g.TranslateTransform(cx, cy);
        g.RotateTransform(angle);
        using (SolidBrush brush = new SolidBrush(Mint))
            g.FillEllipse(brush, -8, -16, 16, 32);
        g.Restore(state);
    }

    static void DrawWin(Graphics g)
    {
        using (SolidBrush cream = new SolidBrush(Cream)) g.FillEllipse(cream, 32, 32, 320, 320);
        using (Pen navy = MakePen(Navy, 14)) g.DrawEllipse(navy, 32, 32, 320, 320);
        using (Pen gold = MakePen(Gold, 9)) g.DrawEllipse(gold, 49, 49, 286, 286);

        using (Pen branch = MakePen(Mint, 8))
        {
            g.DrawBezier(branch, 93, 269, 70, 230, 70, 179, 91, 144);
            g.DrawBezier(branch, 291, 269, 314, 230, 314, 179, 293, 144);
        }
        DrawLeaf(g, 83, 232, -44); DrawLeaf(g, 79, 196, -18); DrawLeaf(g, 84, 160, 15);
        DrawLeaf(g, 301, 232, 44); DrawLeaf(g, 305, 196, 18); DrawLeaf(g, 300, 160, -15);

        using (GraphicsPath shield = ShieldPath(103, 91, 178, 215))
        using (SolidBrush fill = new SolidBrush(PaleMint))
        using (Pen outline = MakePen(Navy, 11))
        {
            g.FillPath(fill, shield);
            g.DrawPath(outline, shield);
        }

        using (SolidBrush egg = new SolidBrush(Gold)) g.FillEllipse(egg, 145, 120, 94, 147);
        using (Pen eggOutline = MakePen(Navy, 9)) g.DrawEllipse(eggOutline, 145, 120, 94, 147);
        using (Pen highlight = MakePen(ColorTranslator.FromHtml("#FFF8E8"), 8))
            g.DrawBezier(highlight, 159, 148, 166, 132, 179, 124, 192, 124);
        using (SolidBrush eye = new SolidBrush(Navy))
        {
            g.FillEllipse(eye, 172.5f, 182.5f, 11, 11);
            g.FillEllipse(eye, 200.5f, 182.5f, 11, 11);
        }
        using (Pen smile = MakePen(Ochre, 6)) g.DrawBezier(smile, 180, 207, 184, 216, 200, 216, 204, 207);
        using (Pen guard = MakePen(Mint, 7)) g.DrawBezier(guard, 168, 229, 179, 241, 205, 241, 216, 229);

        using (GraphicsPath star = StarPath(192, 74, 29, 13, 5))
        using (SolidBrush fill = new SolidBrush(Gold))
        using (Pen outline = MakePen(Ochre, 5))
        {
            g.FillPath(fill, star);
            g.DrawPath(outline, star);
        }
        using (Pen ray = MakePen(Gold, 6))
        {
            g.DrawLine(ray, 139, 82, 144, 93);
            g.DrawLine(ray, 245, 82, 240, 93);
        }
    }

    static void DrawLoss(Graphics g)
    {
        using (GraphicsPath outer = OuterLossPath())
        using (SolidBrush fill = new SolidBrush(PaleCoral))
        using (Pen outline = MakePen(Navy, 14))
        {
            g.FillPath(fill, outer);
            g.DrawPath(outline, outer);
        }
        using (GraphicsPath inner = OuterLossPath())
        using (Pen coral = MakePen(Coral, 9))
        {
            Matrix m = new Matrix();
            m.Translate(12, 12);
            m.Scale(0.93f, 0.93f, MatrixOrder.Append);
            inner.Transform(m);
            g.DrawPath(coral, inner);
            m.Dispose();
        }

        using (GraphicsPath shield = ShieldPath(103, 101, 178, 194))
        using (SolidBrush fill = new SolidBrush(Cream))
        using (Pen outline = MakePen(Navy, 10))
        {
            g.FillPath(fill, shield);
            g.DrawPath(outline, shield);
        }
        using (SolidBrush egg = new SolidBrush(Gold)) g.FillEllipse(egg, 145, 132, 94, 134);
        using (Pen eggOutline = MakePen(Navy, 9)) g.DrawEllipse(eggOutline, 145, 132, 94, 134);
        using (Pen highlight = MakePen(ColorTranslator.FromHtml("#FFF8E8"), 8))
            g.DrawBezier(highlight, 159, 158, 167, 143, 180, 136, 192, 136);
        using (Pen sadEye = MakePen(Navy, 6))
        {
            g.DrawBezier(sadEye, 168, 193, 174, 186, 181, 186, 187, 193);
            g.DrawBezier(sadEye, 198, 193, 204, 186, 211, 186, 217, 193);
        }
        using (Pen sadMouth = MakePen(Ochre, 6)) g.DrawBezier(sadMouth, 180, 223, 184, 214, 200, 214, 204, 223);
        using (Pen split = MakePen(Coral, 9))
        {
            g.DrawLines(split, new PointF[] { new PointF(114, 115), new PointF(99, 97), new PointF(105, 79) });
            g.DrawLines(split, new PointF[] { new PointF(270, 118), new PointF(285, 100), new PointF(280, 82) });
        }
        using (GraphicsPath shard = new GraphicsPath())
        using (SolidBrush fill = new SolidBrush(Coral))
        using (Pen outline = MakePen(Navy, 5))
        {
            shard.AddPolygon(new PointF[] { new PointF(243, 273), new PointF(259, 287), new PointF(248, 303), new PointF(233, 289) });
            g.FillPath(fill, shard); g.DrawPath(outline, shard);
        }
        using (GraphicsPath shard = new GraphicsPath())
        using (SolidBrush fill = new SolidBrush(Gold))
        using (Pen outline = MakePen(Navy, 5))
        {
            shard.AddPolygon(new PointF[] { new PointF(275, 260), new PointF(288, 272), new PointF(280, 286), new PointF(267, 275) });
            g.FillPath(fill, shard); g.DrawPath(outline, shard);
        }
    }

    static void Configure(Graphics g)
    {
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.CompositingQuality = CompositingQuality.HighQuality;
        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
    }

    public static void RenderAtlas(string path)
    {
        using (Bitmap atlas = new Bitmap(832, 416, PixelFormat.Format32bppArgb))
        using (Graphics g = Graphics.FromImage(atlas))
        {
            Configure(g);
            g.Clear(Color.Transparent);
            GraphicsState state = g.Save();
            g.TranslateTransform(16, 16); DrawWin(g); g.Restore(state);
            state = g.Save();
            g.TranslateTransform(432, 16); DrawLoss(g); g.Restore(state);
            atlas.Save(path, ImageFormat.Png);
        }
    }

    static GraphicsPath RoundedRect(float x, float y, float width, float height, float radius)
    {
        GraphicsPath p = new GraphicsPath();
        float d = radius * 2;
        p.AddArc(x, y, d, d, 180, 90); p.AddArc(x + width - d, y, d, d, 270, 90);
        p.AddArc(x + width - d, y + height - d, d, d, 0, 90); p.AddArc(x, y + height - d, d, d, 90, 90);
        p.CloseFigure(); return p;
    }

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

    static void DrawCrop(Graphics g, Bitmap atlas, Rectangle source, RectangleF target, bool gray)
    {
        using (Bitmap crop = atlas.Clone(source, PixelFormat.Format32bppArgb))
        using (Bitmap display = gray ? Gray(crop) : (Bitmap)crop.Clone())
        {
            Configure(g);
            g.DrawImage(display, target, new Rectangle(0, 0, display.Width, display.Height), GraphicsUnit.Pixel);
        }
    }

    public static void RenderReview(string atlasPath, string outputPath)
    {
        using (Bitmap atlas = new Bitmap(atlasPath))
        using (Bitmap board = new Bitmap(1600, 1060, PixelFormat.Format32bppArgb))
        using (Graphics g = Graphics.FromImage(board))
        using (Font title = new Font("Microsoft YaHei UI", 36, FontStyle.Bold, GraphicsUnit.Pixel))
        using (Font label = new Font("Microsoft YaHei UI", 25, FontStyle.Bold, GraphicsUnit.Pixel))
        using (Font body = new Font("Microsoft YaHei UI", 17, FontStyle.Regular, GraphicsUnit.Pixel))
        using (Font small = new Font("Microsoft YaHei UI", 15, FontStyle.Bold, GraphicsUnit.Pixel))
        using (SolidBrush navy = new SolidBrush(Navy))
        using (SolidBrush muted = new SolidBrush(ColorTranslator.FromHtml("#667086")))
        using (SolidBrush cardBrush = new SolidBrush(Color.White))
        using (Pen separator = MakePen(ColorTranslator.FromHtml("#E3E5E8"), 2))
        {
            Configure(g);
            g.Clear(ColorTranslator.FromHtml("#F4F2EC"));
            g.DrawString("P7 / α 结算徽章 · v02", title, navy, 72, 45);
            g.DrawString("原创 UI 候选 · 透明图集不含文字 · 48px 与灰度预览", body, muted, 74, 104);

            using (GraphicsPath left = RoundedRect(64, 190, 704, 790, 28)) g.FillPath(cardBrush, left);
            using (GraphicsPath right = RoundedRect(832, 190, 704, 790, 28)) g.FillPath(cardBrush, right);
            g.DrawString("守护成功", label, navy, 104, 218);
            g.DrawString("完整护盾 · 奶蛋微笑 · 金星与枝叶向上", body, muted, 104, 257);
            g.DrawString("奶蛋失守", label, navy, 872, 218);
            g.DrawString("盾形外廓 · 奶蛋低垂 · 下坠碎片", body, muted, 872, 257);

            DrawCrop(g, atlas, new Rectangle(16, 16, 384, 384), new RectangleF(232, 292, 368, 368), false);
            DrawCrop(g, atlas, new Rectangle(432, 16, 384, 384), new RectangleF(1000, 292, 368, 368), false);
            g.DrawLine(separator, 102, 700, 730, 700);
            g.DrawLine(separator, 870, 700, 1498, 700);
            g.DrawString("48px 彩色（2×）", small, muted, 104, 720);
            g.DrawString("灰度", small, muted, 252, 720);
            g.DrawString("圆形徽章 + 完整护盾", body, navy, 354, 736);
            g.DrawString("用于通关、奶蛋未失守结算", body, muted, 354, 767);
            g.DrawString("48px 彩色（2×）", small, muted, 872, 720);
            g.DrawString("灰度", small, muted, 1020, 720);
            g.DrawString("盾形徽章 + 下坠碎片", body, navy, 1122, 736);
            g.DrawString("用于奶蛋耐久归零的结算", body, muted, 1122, 767);

            DrawCrop(g, atlas, new Rectangle(16, 16, 384, 384), new RectangleF(120, 760, 96, 96), false);
            DrawCrop(g, atlas, new Rectangle(16, 16, 384, 384), new RectangleF(252, 776, 64, 64), true);
            DrawCrop(g, atlas, new Rectangle(432, 16, 384, 384), new RectangleF(888, 760, 96, 96), false);
            DrawCrop(g, atlas, new Rectangle(432, 16, 384, 384), new RectangleF(1020, 776, 64, 64), true);

            using (Pen footer = MakePen(ColorTranslator.FromHtml("#D9D9D1"), 2)) g.DrawLine(footer, 64, 1002, 1536, 1002);
            g.DrawString("P7 海军蓝描边、奶油白底、金色奶蛋、薄荷绿守护态与珊瑚红失守态。颜色之外以圆/盾轮廓、星标上扬/碎片下坠编码状态。", small, muted, 72, 1012);
            board.Save(outputPath, ImageFormat.Png);
        }
    }
}
'@

$DrawingAssemblies = @(
    [System.Drawing.Bitmap].Assembly.Location
    [System.Drawing.Color].Assembly.Location
)
foreach ($assemblyName in [System.Drawing.Bitmap].Assembly.GetReferencedAssemblies() |
    Where-Object { $_.Name -like 'System.Private.Windows.*' }) {
    $DrawingAssemblies += [System.Reflection.Assembly]::Load($assemblyName).Location
}
Add-Type -TypeDefinition $RendererSource -ReferencedAssemblies $DrawingAssemblies
[P7ResultBadgeRenderer]::RenderAtlas($AtlasPng)
[P7ResultBadgeRenderer]::RenderReview($AtlasPng, $ReviewPng)

$SourceHash = (Get-FileHash -LiteralPath $AtlasSvg -Algorithm SHA256).Hash.ToLowerInvariant()
$ExportHash = (Get-FileHash -LiteralPath $AtlasPng -Algorithm SHA256).Hash.ToLowerInvariant()
$Rows = @(
    [pscustomobject]@{
        AssetId = 'P7-UI-A1-RESULT-WIN'; Owner = 'P7'; SourcePath = 'UI/A1/P7_A1_ResultBadgeAtlas_v02.svg'; SourceHash = $SourceHash
        ExportPath = 'UI/A1/P7_A1_ResultBadgeAtlas_v02.png'; ExportHash = $ExportHash; Version = 'v02'
        IntendedUse = 'Settlement victory: protected milk-egg guardian'; Width = 384; Height = 384; FrameCount = 2; FrameIndex = 0
        AlphaMode = 'Straight RGBA with transparent background'; ColorSpace = 'sRGB'; ExportPreset = '8-bit transparent PNG; 2-frame horizontal atlas with 16px outer margins and gutter'
        UnrealPath = '/Game/Naiwa/P7/UI/A1/T_P7_A1_ResultBadgeAtlas_v02'; AtlasX = 16; AtlasY = 16; AtlasW = 384; AtlasH = 384
        ReviewStatus = 'A1 candidate v02; user visual review pending; not imported'; ReuseStatus = 'P7-exclusive; Common review not started'
        EvidencePath = 'UI/A1/Review/P7_A1_ResultBadgeReview_v02.png'; Notes = 'Circular seal, intact mint shield, smiling egg, rising star and leaves; labels are review-only.'
    }
    [pscustomobject]@{
        AssetId = 'P7-UI-A1-RESULT-LOSS'; Owner = 'P7'; SourcePath = 'UI/A1/P7_A1_ResultBadgeAtlas_v02.svg'; SourceHash = $SourceHash
        ExportPath = 'UI/A1/P7_A1_ResultBadgeAtlas_v02.png'; ExportHash = $ExportHash; Version = 'v02'
        IntendedUse = 'Settlement defeat: milk-egg guardian lost'; Width = 384; Height = 384; FrameCount = 2; FrameIndex = 1
        AlphaMode = 'Straight RGBA with transparent background'; ColorSpace = 'sRGB'; ExportPreset = '8-bit transparent PNG; 2-frame horizontal atlas with 16px outer margins and gutter'
        UnrealPath = '/Game/Naiwa/P7/UI/A1/T_P7_A1_ResultBadgeAtlas_v02'; AtlasX = 432; AtlasY = 16; AtlasW = 384; AtlasH = 384
        ReviewStatus = 'A1 candidate v02; user visual review pending; not imported'; ReuseStatus = 'P7-exclusive; Common review not started'
        EvidencePath = 'UI/A1/Review/P7_A1_ResultBadgeReview_v02.png'; Notes = 'Shield-shaped seal, sad egg with an intact center, coral failure accents and falling shards; labels are review-only.'
    }
)
$Rows | Export-Csv -LiteralPath $Manifest -NoTypeInformation -Encoding utf8

$Atlas = [System.Drawing.Bitmap]::FromFile($AtlasPng)
try {
    if ($Atlas.Width -ne 832 -or $Atlas.Height -ne 416 -or $Atlas.GetPixel(0, 0).A -ne 0) {
        throw "Unexpected result badge atlas output: $($Atlas.Width)x$($Atlas.Height), alpha=$($Atlas.GetPixel(0, 0).A)"
    }
} finally {
    $Atlas.Dispose()
}
Write-Output "Generated two P7 result badge candidates. Atlas: $AtlasPng; review: $ReviewPng; manifest: $Manifest"

