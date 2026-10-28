[CmdletBinding()]
param(
    [string]$Source,
    [string]$Output,
    [string]$Preview,
    [switch]$MatchAlphaSeam
)

$ErrorActionPreference = 'Stop'
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $scriptRoot '..\..'))
if ([string]::IsNullOrWhiteSpace($Source)) {
    $Source = Join-Path $projectRoot 'UE5\SourceAssets\P5\Backgrounds\Source\BG_Ridge_Near_Source_v04_WIP.png'
}
if ([string]::IsNullOrWhiteSpace($Output)) {
    $Output = Join-Path $projectRoot 'UE5\SourceAssets\P5\Backgrounds\Exports\BG_Ridge_Near_v02_WIP.png'
}
if ([string]::IsNullOrWhiteSpace($Preview)) {
    $Preview = Join-Path $projectRoot 'UE5\Records\p5_p6_plan\evidence\P5_A1_Backgrounds\P5_A1_RidgeTiling_v08.png'
}
$Source = [System.IO.Path]::GetFullPath($Source)
$Output = [System.IO.Path]::GetFullPath($Output)
$Preview = [System.IO.Path]::GetFullPath($Preview)
foreach ($path in @($Output, $Preview)) {
    if (Test-Path -LiteralPath $path) { throw "Refusing to overwrite existing candidate: $path" }
}
if (-not (Test-Path -LiteralPath $Source)) { throw "A1 source not found: $Source" }
New-Item -ItemType Directory -Path (Split-Path -Parent $Output) -Force | Out-Null
New-Item -ItemType Directory -Path (Split-Path -Parent $Preview) -Force | Out-Null
Add-Type -AssemblyName System.Drawing

$typeName = 'P5RidgeSeamCandidate'
if (-not ($typeName -as [type])) {
    $code = @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class P5RidgeSeamCandidate
{
    private static byte[] ReadPixels(Bitmap bitmap)
    {
        Rectangle rect = new Rectangle(0, 0, bitmap.Width, bitmap.Height);
        BitmapData data = bitmap.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        try
        {
            byte[] result = new byte[bitmap.Width * bitmap.Height * 4];
            byte[] row = new byte[Math.Abs(data.Stride)];
            for (int y = 0; y < bitmap.Height; y++)
            {
                IntPtr rowPtr = IntPtr.Add(data.Scan0, y * data.Stride);
                Marshal.Copy(rowPtr, row, 0, row.Length);
                Buffer.BlockCopy(row, 0, result, y * bitmap.Width * 4, bitmap.Width * 4);
            }
            return result;
        }
        finally { bitmap.UnlockBits(data); }
    }

    private static void WritePixels(Bitmap bitmap, byte[] pixels)
    {
        Rectangle rect = new Rectangle(0, 0, bitmap.Width, bitmap.Height);
        BitmapData data = bitmap.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        try
        {
            byte[] row = new byte[Math.Abs(data.Stride)];
            for (int y = 0; y < bitmap.Height; y++)
            {
                Array.Clear(row, 0, row.Length);
                Buffer.BlockCopy(pixels, y * bitmap.Width * 4, row, 0, bitmap.Width * 4);
                IntPtr rowPtr = IntPtr.Add(data.Scan0, y * data.Stride);
                Marshal.Copy(row, 0, rowPtr, row.Length);
            }
        }
        finally { bitmap.UnlockBits(data); }
    }

    private static byte Clamp(double value)
    {
        if (value <= 0) return 0;
        if (value >= 255) return 255;
        return (byte)Math.Round(value);
    }

    private static int[] LargestAlphaComponent(byte[] pixels, int width, int height, int threshold)
    {
        int count = width * height;
        bool[] visited = new bool[count];
        int[] queue = new int[count];
        int[] best = new int[0];
        for (int start = 0; start < count; start++)
        {
            if (visited[start] || pixels[start * 4 + 3] < threshold) continue;
            int head = 0, tail = 0;
            queue[tail++] = start;
            visited[start] = true;
            while (head < tail)
            {
                int index = queue[head++];
                int x = index % width;
                int y = index / width;
                for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                    int next = ny * width + nx;
                    if (!visited[next] && pixels[next * 4 + 3] >= threshold)
                    {
                        visited[next] = true;
                        queue[tail++] = next;
                    }
                }
            }
            if (tail > best.Length)
            {
                best = new int[tail];
                Array.Copy(queue, best, tail);
            }
        }
        return best;
    }

    private static bool[] MakeFringe(int[] component, int width, int height, int radius)
    {
        bool[] keep = new bool[width * height];
        foreach (int index in component)
        {
            int x = index % width, y = index / width;
            for (int dy = -radius; dy <= radius; dy++)
            for (int dx = -radius; dx <= radius; dx++)
            {
                int nx = x + dx, ny = y + dy;
                if (nx >= 0 && nx < width && ny >= 0 && ny < height)
                    keep[ny * width + nx] = true;
            }
        }
        return keep;
    }

    private static void CorrectPeriodicEdges(byte[] pixels, int width, int height, bool matchAlphaSeam)
    {
        const int band = 96;
        for (int y = 0; y < height; y++)
        {
            int left = (y * width) * 4;
            int right = (y * width + width - 1) * 4;
            double leftA = pixels[left + 3] / 255.0;
            double rightA = pixels[right + 3] / 255.0;
            if (matchAlphaSeam)
            {
                const int alphaBand = 16;
                double targetA = Math.Min(leftA, rightA);
                double[] targetPremul = new double[3];
                for (int channel = 0; channel < 3; channel++)
                {
                    double leftPremul = pixels[left + channel] * leftA;
                    double rightPremul = pixels[right + channel] * rightA;
                    targetPremul[channel] = leftA < rightA ? leftPremul :
                        (rightA < leftA ? rightPremul : (leftPremul + rightPremul) * 0.5);
                }
                for (int x = 0; x < width; x++)
                {
                    int edgeDistance = Math.Min(x, width - 1 - x);
                    if (edgeDistance >= alphaBand) continue;
                    double blend = 0.5 + 0.5 * Math.Cos(Math.PI * edgeDistance / alphaBand);
                    int p = (y * width + x) * 4;
                    double sourceA = pixels[p + 3] / 255.0;
                    double resultA = sourceA + (targetA - sourceA) * blend;
                    byte resultAlpha = Clamp(resultA * 255.0);
                    resultA = resultAlpha / 255.0;
                    pixels[p + 3] = resultAlpha;
                    for (int channel = 0; channel < 3; channel++)
                    {
                        double sourcePremul = pixels[p + channel] * sourceA;
                        double resultPremul = sourcePremul + (targetPremul[channel] - sourcePremul) * blend;
                        pixels[p + channel] = resultA <= 0.0001 ? (byte)0 : Clamp(resultPremul / resultA);
                    }
                }
                continue;
            }
            double[] delta = new double[4];
            for (int channel = 0; channel < 3; channel++)
                delta[channel] = pixels[right + channel] * rightA - pixels[left + channel] * leftA;
            for (int x = 0; x < width; x++)
            {
                double t = (double)x / (width - 1);
                double weight = 0.5 + 0.5 * Math.Cos(Math.PI * t);
                int edgeDistance = Math.Min(x, width - 1 - x);
                double edgeT = Math.Min(1.0, (double)edgeDistance / band);
                double edgeEnvelope = 0.5 + 0.5 * Math.Cos(Math.PI * edgeT);
                double correction = (weight - 0.5) * edgeEnvelope;
                int p = (y * width + x) * 4;
                double a = pixels[p + 3] / 255.0;
                double[] premul = new double[3];
                for (int channel = 0; channel < 3; channel++)
                    premul[channel] = pixels[p + channel] * (pixels[p + 3] / 255.0) + delta[channel] * correction;
                if (a <= 0.0001)
                {
                    pixels[p] = pixels[p + 1] = pixels[p + 2] = 0;
                }
                else
                {
                    for (int channel = 0; channel < 3; channel++)
                        pixels[p + channel] = Clamp(premul[channel] / a);
                }
            }
        }
    }

    public static string Export(string sourcePath, string outputPath, string previewPath, bool matchAlphaSeam)
    {
        using (Bitmap opened = new Bitmap(sourcePath))
        using (Bitmap source = new Bitmap(opened.Width, opened.Height, PixelFormat.Format32bppArgb))
        {
            using (Graphics g = Graphics.FromImage(source))
            {
                g.CompositingMode = CompositingMode.SourceCopy;
                g.DrawImageUnscaled(opened, 0, 0);
            }
            byte[] sourcePixels = ReadPixels(source);
            int[] component = LargestAlphaComponent(sourcePixels, source.Width, source.Height, 24);
            if (component.Length == 0) throw new InvalidOperationException("No connected ridge art found.");
            bool[] keep = MakeFringe(component, source.Width, source.Height, 2);
            for (int i = 0; i < keep.Length; i++)
            {
                if (!keep[i])
                {
                    int p = i * 4;
                    sourcePixels[p] = sourcePixels[p + 1] = sourcePixels[p + 2] = sourcePixels[p + 3] = 0;
                }
            }

            using (Bitmap cleaned = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb))
            using (Bitmap exported = new Bitmap(2048, 768, PixelFormat.Format32bppArgb))
            {
                WritePixels(cleaned, sourcePixels);
                using (Graphics g = Graphics.FromImage(exported))
                {
                    g.CompositingMode = CompositingMode.SourceCopy;
                    g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                    g.PixelOffsetMode = PixelOffsetMode.Half;
                    g.SmoothingMode = SmoothingMode.HighQuality;
                    g.DrawImage(cleaned, new Rectangle(0, 0, exported.Width, exported.Height), new Rectangle(0, 0, cleaned.Width, cleaned.Height), GraphicsUnit.Pixel);
                }
                byte[] result = ReadPixels(exported);
                CorrectPeriodicEdges(result, exported.Width, exported.Height, matchAlphaSeam);
                WritePixels(exported, result);
                exported.Save(outputPath, ImageFormat.Png);

                using (Bitmap preview = new Bitmap(4096, 768, PixelFormat.Format32bppArgb))
                using (Graphics g = Graphics.FromImage(preview))
                {
                    g.Clear(Color.FromArgb(255, 115, 137, 161));
                    g.CompositingMode = CompositingMode.SourceOver;
                    g.DrawImageUnscaled(exported, 0, 0);
                    g.DrawImageUnscaled(exported, 2048, 0);
                    preview.Save(previewPath, ImageFormat.Png);
                }

                double[] means = new double[4];
                int[] maxes = new int[4];
                for (int y = 0; y < exported.Height; y++)
                {
                    int l = (y * exported.Width) * 4;
                    int r = (y * exported.Width + exported.Width - 1) * 4;
                    for (int c = 0; c < 4; c++)
                    {
                        int diff = Math.Abs(result[l + c] - result[r + c]);
                        means[c] += diff;
                        if (diff > maxes[c]) maxes[c] = diff;
                    }
                }
                for (int c = 0; c < 4; c++) means[c] = Math.Round(means[c] / exported.Height, 3);
                return "largestComponentPixels=" + component.Length + "; removedDisconnectedComponents=thresholded outside major ridge; " +
                    "edgeMeanAbsDiffRGBA=" + String.Join(",", means) + "; edgeMaxAbsDiffRGBA=" + String.Join(",", maxes) +
                    "; alphaSeamMatched=" + matchAlphaSeam +
                    "; sourceDimensions=" + source.Width + "x" + source.Height + "; exportDimensions=2048x768";
            }
        }
    }
}
'@
    Add-Type -TypeDefinition $code -ReferencedAssemblies @('System.Drawing.dll')
}

$summary = [P5RidgeSeamCandidate]::Export($Source, $Output, $Preview, $MatchAlphaSeam.IsPresent)
Write-Output $summary
Write-Output "Source: $Source"
Write-Output "Output: $Output"
Write-Output "Preview: $Preview"
