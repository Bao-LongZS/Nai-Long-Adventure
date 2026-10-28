[CmdletBinding()]
param(
    [string]$Source,
    [string]$Output,
    [int]$TargetSize = 1024,
    [int]$AlphaThreshold = 24
)

$ErrorActionPreference = 'Stop'
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($Source)) {
    $Source = Join-Path $scriptRoot '..\SourceAssets\P5\Environment\Source\C_Kangaroo_Ride_ApprovedBase_Source_20261028.png'
}
if ([string]::IsNullOrWhiteSpace($Output)) {
    $Output = Join-Path $scriptRoot '..\SourceAssets\P5\Environment\Exports\C_Kangaroo_Ride_v04_WIP.png'
}
$Source = [System.IO.Path]::GetFullPath($Source)
$Output = [System.IO.Path]::GetFullPath($Output)
if (-not (Test-Path -LiteralPath $Source)) { throw "A5 approved source not found: $Source" }
if (Test-Path -LiteralPath $Output) { throw "Refusing to overwrite existing A5 candidate: $Output" }
New-Item -ItemType Directory -Path (Split-Path -Parent $Output) -Force | Out-Null
Add-Type -AssemblyName System.Drawing

$typeName = 'P5KangarooAtlasExporter'
if (-not ($typeName -as [type])) {
    $code = @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Linq;

public static class P5KangarooAtlasExporter
{
    private sealed class Component
    {
        public readonly List<int> Pixels = new List<int>();
        public int MinX = Int32.MaxValue, MinY = Int32.MaxValue;
        public int MaxX = Int32.MinValue, MaxY = Int32.MinValue;
        public int BluePixels;
    }

    public static string Export(string sourcePath, string outputPath, int targetSize, int alphaThreshold)
    {
        if (targetSize <= 0) throw new ArgumentOutOfRangeException("targetSize");
        using (Bitmap source = new Bitmap(sourcePath))
        using (Bitmap cleaned = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb))
        {
            if (source.Width % 3 != 0 || source.Height % 2 != 0)
                throw new InvalidOperationException("Approved A5 base must divide evenly into a 3x2 atlas.");
            using (Graphics copy = Graphics.FromImage(cleaned))
            {
                copy.CompositingMode = CompositingMode.SourceCopy;
                copy.DrawImageUnscaled(source, 0, 0);
            }

            int cellWidth = source.Width / 3;
            int cellHeight = source.Height / 2;
            var summaries = new List<string>();
            for (int row = 0; row < 2; row++)
            for (int col = 0; col < 3; col++)
            {
                int index = row * 3 + col;
                int x0 = col * cellWidth;
                int y0 = row * cellHeight;
                bool[] alphaMask = BuildAlphaMask(cleaned, x0, y0, cellWidth, cellHeight, alphaThreshold);
                bool[] openedMask = MorphologicalOpen(alphaMask, cellWidth, cellHeight);
                List<Component> components = FindComponents(cleaned, openedMask, x0, y0, cellWidth, cellHeight, alphaThreshold);
                if (components.Count == 0) throw new InvalidOperationException("No visible art found in A5 cell " + index);
                components = components.OrderByDescending(c => c.Pixels.Count).ToList();
                Component main = components[0];
                int removedComponents = 0;
                int removedPixels = 0;
                int retainedBlueComponents = 0;
                bool[] keepMask = new bool[cellWidth * cellHeight];
                for (int c = 1; c < components.Count; c++)
                {
                    Component other = components[c];
                    bool keepActionMark = index == 4 && other.BluePixels >= 8;
                    if (keepActionMark)
                    {
                        retainedBlueComponents++;
                        foreach (int localIndex in other.Pixels) keepMask[localIndex] = true;
                        continue;
                    }
                    removedComponents++;
                    removedPixels += other.Pixels.Count;
                }
                foreach (int localIndex in main.Pixels) keepMask[localIndex] = true;

                // Retain a two-pixel soft-alpha fringe around the kept character/action art.
                bool[] fringe = (bool[])keepMask.Clone();
                for (int localIndex = 0; localIndex < keepMask.Length; localIndex++)
                {
                    if (!keepMask[localIndex]) continue;
                    int x = localIndex % cellWidth, y = localIndex / cellWidth;
                    for (int dy = -2; dy <= 2; dy++)
                    for (int dx = -2; dx <= 2; dx++)
                    {
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && nx < cellWidth && ny >= 0 && ny < cellHeight)
                            fringe[ny * cellWidth + nx] = true;
                    }
                }
                for (int localIndex = 0; localIndex < fringe.Length; localIndex++)
                {
                    int px = x0 + localIndex % cellWidth;
                    int py = y0 + localIndex / cellWidth;
                    if (!fringe[localIndex] && cleaned.GetPixel(px, py).A > 0)
                    {
                        cleaned.SetPixel(px, py, Color.Transparent);
                        removedPixels++;
                    }
                }
                summaries.Add(String.Format(
                    "frame={0}; components={1}; main_bbox=[{2},{3},{4},{5}]; removed_components={6}; removed_pixels={7}; retained_blue_action_components={8}",
                    index, components.Count, main.MinX, main.MinY, main.MaxX, main.MaxY,
                    removedComponents, removedPixels, retainedBlueComponents));
            }

            using (Bitmap resized = new Bitmap(targetSize, targetSize, PixelFormat.Format32bppArgb))
            {
                using (Graphics g = Graphics.FromImage(resized))
                {
                    g.CompositingMode = CompositingMode.SourceCopy;
                    g.CompositingQuality = CompositingQuality.HighQuality;
                    g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                    g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                    g.SmoothingMode = SmoothingMode.HighQuality;
                    g.DrawImage(cleaned, new Rectangle(0, 0, targetSize, targetSize),
                        0, 0, cleaned.Width, cleaned.Height, GraphicsUnit.Pixel);
                }
                resized.Save(outputPath, ImageFormat.Png);
            }
            return String.Join(Environment.NewLine, summaries.ToArray());
        }
    }

    private static bool[] BuildAlphaMask(Bitmap image, int x0, int y0, int width, int height, int alphaThreshold)
    {
        var mask = new bool[width * height];
        for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            mask[y * width + x] = image.GetPixel(x0 + x, y0 + y).A >= alphaThreshold;
        return mask;
    }

    private static bool[] MorphologicalOpen(bool[] mask, int width, int height)
    {
        var eroded = new bool[mask.Length];
        for (int y = 1; y < height - 1; y++)
        for (int x = 1; x < width - 1; x++)
        {
            bool keep = true;
            for (int dy = -1; dy <= 1 && keep; dy++)
            for (int dx = -1; dx <= 1; dx++)
                if (!mask[(y + dy) * width + (x + dx)]) { keep = false; break; }
            eroded[y * width + x] = keep;
        }
        var opened = new bool[mask.Length];
        for (int y = 1; y < height - 1; y++)
        for (int x = 1; x < width - 1; x++)
        {
            if (!eroded[y * width + x]) continue;
            for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) opened[(y + dy) * width + (x + dx)] = true;
        }
        return opened;
    }

    private static List<Component> FindComponents(Bitmap image, bool[] mask, int x0, int y0, int width, int height, int alphaThreshold)
    {
        int area = width * height;
        bool[] visited = new bool[area];
        int[] queue = new int[area];
        var found = new List<Component>();
        for (int start = 0; start < area; start++)
        {
            if (visited[start]) continue;
            int sx = start % width, sy = start / width;
            if (!mask[start]) { visited[start] = true; continue; }
            var component = new Component();
            int head = 0, tail = 0;
            queue[tail++] = start;
            visited[start] = true;
            while (head < tail)
            {
                int current = queue[head++];
                int x = current % width, y = current / width;
                Color color = image.GetPixel(x0 + x, y0 + y);
                component.Pixels.Add(current);
                if (x < component.MinX) component.MinX = x;
                if (y < component.MinY) component.MinY = y;
                if (x > component.MaxX) component.MaxX = x;
                if (y > component.MaxY) component.MaxY = y;
                if (color.A >= alphaThreshold && color.B > color.R * 1.35 && color.B > color.G * 1.08)
                    component.BluePixels++;
                Enqueue(current - 1, x > 0, mask, visited, queue, ref tail);
                Enqueue(current + 1, x + 1 < width, mask, visited, queue, ref tail);
                Enqueue(current - width, y > 0, mask, visited, queue, ref tail);
                Enqueue(current + width, y + 1 < height, mask, visited, queue, ref tail);
            }
            found.Add(component);
        }
        return found;
    }

    private static void Enqueue(int index, bool inBounds, bool[] mask,
        bool[] visited, int[] queue, ref int tail)
    {
        if (!inBounds || index < 0 || index >= mask.Length || visited[index] || !mask[index]) return;
        visited[index] = true;
        queue[tail++] = index;
    }
}
'@
    Add-Type -TypeDefinition $code -ReferencedAssemblies 'System.Drawing'
}

$summary = [P5KangarooAtlasExporter]::Export($Source, $Output, $TargetSize, $AlphaThreshold)
Write-Output "A5 atlas candidate exported: $Output"
Write-Output "Source: $Source"
Write-Output $summary
