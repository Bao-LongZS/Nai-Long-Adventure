[CmdletBinding()]
param(
    [string]$RepositoryRoot = '',
    [string]$RuntimeScreenshot = '',
    [string]$RuntimeViewport = '',
    [string]$OutputPath = ''
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $scriptDirectory = Split-Path -Parent $PSCommandPath
    $RepositoryRoot = (Resolve-Path (Join-Path $scriptDirectory '../..')).Path
}
if ($PSVersionTable.PSEdition -eq 'Core') {
    $windowsPowerShell = Join-Path $env:WINDIR 'System32/WindowsPowerShell/v1.0/powershell.exe'
    if (-not (Test-Path -LiteralPath $windowsPowerShell -PathType Leaf)) { throw 'Windows PowerShell 5.1 is required for System.Drawing pixel access.' }
    $legacyArguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath, '-RepositoryRoot', $RepositoryRoot)
    if (-not [string]::IsNullOrWhiteSpace($RuntimeScreenshot)) { $legacyArguments += @('-RuntimeScreenshot', $RuntimeScreenshot) }
    if (-not [string]::IsNullOrWhiteSpace($RuntimeViewport)) { $legacyArguments += @('-RuntimeViewport', $RuntimeViewport) }
    if (-not [string]::IsNullOrWhiteSpace($OutputPath)) { $legacyArguments += @('-OutputPath', $OutputPath) }
    & $windowsPowerShell @legacyArguments
    exit $LASTEXITCODE
}
Add-Type -AssemblyName System.Drawing

if (-not ('NaiwaP4PixelProbeNative' -as [type])) {
    $pixelProbeSource = @'
using System;
using System.Drawing;
using System.Collections.Generic;

public sealed class NaiwaP4PixelStats
{
    public int Width;
    public int Height;
    public int VisiblePixelsAlpha8;
    public int VisiblePixelsAlpha32;
    public int BorderPixelsAlpha8;
    public int BoundsLeftAlpha8;
    public int BoundsTopAlpha8;
    public int BoundsRightAlpha8;
    public int BoundsBottomAlpha8;
    public int BoundsBottomAlpha32;
    public int CornerAlphaAbove8;
    public double AlphaCoverage;
    public double MeanLuma;
    public double StdDevLuma;
    public int MinLuma;
    public int MaxLuma;
    public byte[] Pixels;
}

public sealed class NaiwaP4PixelDelta
{
    public int ChangedForegroundPixels;
    public int AlphaMaskChangedPixels;
    public int ForegroundUnionPixels;
    public double ChangedForegroundRatio;
    public double AlphaMaskChangedRatio;
    public double MeanAbsAlphaDelta;
}

public sealed class NaiwaP4OrientationScore
{
    public string Orientation;
    public double Correlation;
    public double MeanAbsoluteError;
    public long ChannelSamples;
}

public static class NaiwaP4PixelProbeNative
{
    public static NaiwaP4OrientationScore[] CompareBackdropOrientations(
        string sourcePath, string screenshotPath, int viewportX, int viewportY, int viewportWidth, int viewportHeight)
    {
        using (var source = new Bitmap(sourcePath))
        using (var screenshot = new Bitmap(screenshotPath))
        {
            if (viewportX < 0 || viewportY < 0 || viewportWidth <= 0 || viewportHeight <= 0 ||
                viewportX + viewportWidth > screenshot.Width || viewportY + viewportHeight > screenshot.Height)
                throw new ArgumentOutOfRangeException("Runtime viewport is outside the screenshot bounds.");

            var names = new[] { "normal", "flip_y", "flip_x", "flip_xy" };
            var reports = new NaiwaP4OrientationScore[names.Length];
            int step = Math.Max(2, Math.Min(viewportWidth, viewportHeight) / 360);
            for (int mode = 0; mode < names.Length; ++mode)
            {
                double sumA = 0.0, sumB = 0.0, sumAA = 0.0, sumBB = 0.0, sumAB = 0.0, mae = 0.0;
                long n = 0;
                for (int y = 0; y < viewportHeight; y += step)
                {
                    for (int x = 0; x < viewportWidth; x += step)
                    {
                        double nx = (double)x / viewportWidth;
                        double ny = (double)y / viewportHeight;
                        bool leftHud = ny < 0.265 && nx < 0.24;
                        bool rightHud = ny < 0.12 && nx > 0.75;
                        bool combatOverlay = ny >= 0.25 && ny < 0.62 && nx >= 0.55 && nx < 0.86;
                        if (leftHud || rightHud || combatOverlay) continue;

                        int sx = Math.Min(source.Width - 1, (int)((x + 0.5) * source.Width / viewportWidth));
                        int sy = Math.Min(source.Height - 1, (int)((y + 0.5) * source.Height / viewportHeight));
                        if ((mode & 1) != 0) sy = source.Height - 1 - sy;
                        if ((mode & 2) != 0) sx = source.Width - 1 - sx;
                        Color a = source.GetPixel(sx, sy);
                        Color b = screenshot.GetPixel(viewportX + x, viewportY + y);
                        byte[] ar = { a.R, a.G, a.B };
                        byte[] br = { b.R, b.G, b.B };
                        for (int c = 0; c < 3; ++c)
                        {
                            double av = ar[c], bv = br[c];
                            sumA += av; sumB += bv; sumAA += av * av; sumBB += bv * bv; sumAB += av * bv;
                            mae += Math.Abs(av - bv);
                            ++n;
                        }
                    }
                }
                double covariance = n * sumAB - sumA * sumB;
                double varianceProduct = (n * sumAA - sumA * sumA) * (n * sumBB - sumB * sumB);
                reports[mode] = new NaiwaP4OrientationScore {
                    Orientation = names[mode],
                    Correlation = varianceProduct <= 0.0 ? 0.0 : covariance / Math.Sqrt(varianceProduct),
                    MeanAbsoluteError = n == 0 ? 0.0 : mae / n,
                    ChannelSamples = n
                };
            }
            return reports;
        }
    }

    public static NaiwaP4PixelStats Inspect(string path)
    {
        using (var image = new Bitmap(path))
        {
            var stats = new NaiwaP4PixelStats();
            stats.Width = image.Width;
            stats.Height = image.Height;
            stats.BoundsLeftAlpha8 = image.Width;
            stats.BoundsTopAlpha8 = image.Height;
            stats.BoundsRightAlpha8 = -1;
            stats.BoundsBottomAlpha8 = -1;
            stats.BoundsBottomAlpha32 = -1;
            stats.MinLuma = 255;
            stats.MaxLuma = 0;
            stats.Pixels = new byte[image.Width * image.Height * 4];

            long alphaVisible = 0;
            long lumaSum = 0;
            double lumaSqSum = 0.0;
            int pixelIndex = 0;
            for (int y = 0; y < image.Height; ++y)
            {
                for (int x = 0; x < image.Width; ++x)
                {
                    Color c = image.GetPixel(x, y);
                    stats.Pixels[pixelIndex++] = c.R;
                    stats.Pixels[pixelIndex++] = c.G;
                    stats.Pixels[pixelIndex++] = c.B;
                    stats.Pixels[pixelIndex++] = c.A;
                    if (c.A > 8)
                    {
                        ++stats.VisiblePixelsAlpha8;
                        if (x < stats.BoundsLeftAlpha8) stats.BoundsLeftAlpha8 = x;
                        if (x > stats.BoundsRightAlpha8) stats.BoundsRightAlpha8 = x;
                        if (y < stats.BoundsTopAlpha8) stats.BoundsTopAlpha8 = y;
                        if (y > stats.BoundsBottomAlpha8) stats.BoundsBottomAlpha8 = y;
                        if (x == 0 || y == 0 || x == image.Width - 1 || y == image.Height - 1)
                            ++stats.BorderPixelsAlpha8;
                    }
                    if (c.A > 32)
                    {
                        ++stats.VisiblePixelsAlpha32;
                        if (y > stats.BoundsBottomAlpha32) stats.BoundsBottomAlpha32 = y;
                    }
                    if (c.A > 8 && ((x == 0 && y == 0) || (x == image.Width - 1 && y == 0) ||
                                    (x == 0 && y == image.Height - 1) || (x == image.Width - 1 && y == image.Height - 1)))
                        ++stats.CornerAlphaAbove8;

                    int luma = (299 * c.R + 587 * c.G + 114 * c.B + 500) / 1000;
                    if (luma < stats.MinLuma) stats.MinLuma = luma;
                    if (luma > stats.MaxLuma) stats.MaxLuma = luma;
                    lumaSum += luma;
                    lumaSqSum += (double)luma * luma;
                    if (c.A > 8) ++alphaVisible;
                }
            }

            long total = (long)image.Width * image.Height;
            stats.AlphaCoverage = total == 0 ? 0.0 : (double)alphaVisible / total;
            stats.MeanLuma = total == 0 ? 0.0 : (double)lumaSum / total;
            double variance = total == 0 ? 0.0 : (lumaSqSum / total) - stats.MeanLuma * stats.MeanLuma;
            stats.StdDevLuma = Math.Sqrt(Math.Max(0.0, variance));
            return stats;
        }
    }

    public static NaiwaP4PixelDelta Compare(NaiwaP4PixelStats a, NaiwaP4PixelStats b)
    {
        if (a.Width != b.Width || a.Height != b.Height)
            throw new ArgumentException("Sequence frames have different canvas dimensions.");
        var result = new NaiwaP4PixelDelta();
        long alphaDelta = 0;
        int count = a.Width * a.Height;
        for (int i = 0, p = 0; i < count; ++i, p += 4)
        {
            int ar = a.Pixels[p], ag = a.Pixels[p + 1], ab = a.Pixels[p + 2], aa = a.Pixels[p + 3];
            int br = b.Pixels[p], bg = b.Pixels[p + 1], bb = b.Pixels[p + 2], ba = b.Pixels[p + 3];
            int da = Math.Abs(aa - ba);
            alphaDelta += da;
            if (aa > 8 || ba > 8)
            {
                ++result.ForegroundUnionPixels;
                if (da > 8 || Math.Abs(ar - br) > 8 || Math.Abs(ag - bg) > 8 || Math.Abs(ab - bb) > 8)
                    ++result.ChangedForegroundPixels;
            }
            if ((aa > 8) != (ba > 8)) ++result.AlphaMaskChangedPixels;
        }
        result.ChangedForegroundRatio = result.ForegroundUnionPixels == 0 ? 0.0 :
            (double)result.ChangedForegroundPixels / result.ForegroundUnionPixels;
        result.AlphaMaskChangedRatio = count == 0 ? 0.0 : (double)result.AlphaMaskChangedPixels / count;
        result.MeanAbsAlphaDelta = count == 0 ? 0.0 : (double)alphaDelta / count;
        return result;
    }
}
'@
    Add-Type -TypeDefinition $pixelProbeSource -ReferencedAssemblies @('System.Drawing.dll')
}

$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$manifestPath = Join-Path $root 'UE5/SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v06.json'
$a2ReportPath = Join-Path $root 'UE5/Records/p4_alpha/evidence/A2/P4_A2_RefinedArena_build_report.json'
$widgetSourcePath = Join-Path $root 'UE5/NaiwaP0/Source/NaiwaRuntime/P4/Presentation/NaiwaP4ArtIntegrationWidget.cpp'
$arenaSourcePath = Join-Path $root 'UE5/NaiwaP0/Source/NaiwaRuntime/P4/Core/NaiwaP4ArenaSubsystem.cpp'
$outputDirectory = Join-Path $root 'UE5/Records/p4_alpha/evidence/PixelProbe'
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $reportPath = Join-Path $outputDirectory 'P4_source_pixel_probe_20261101_v03.json'
} elseif ([System.IO.Path]::IsPathRooted($OutputPath)) {
    $reportPath = [System.IO.Path]::GetFullPath($OutputPath)
} else {
    $reportPath = [System.IO.Path]::GetFullPath((Join-Path $root $OutputPath))
}
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $reportPath) | Out-Null

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$checks = [System.Collections.Generic.List[object]]::new()
$assetStats = @{}
$assetEntries = @($manifest.assets | Where-Object {
    $_.type -in @('boss_single_keyframe_candidate', 'boss_idle_transform_frame_candidate',
                   'boss_action_sequence_frame_candidate', 'isolated_vfx_still_candidate',
                   'transform_animated_vfx_frame_candidate')
})

function Add-Check {
    param([string]$Id, [bool]$Pass, [object]$Actual, [object]$Expected, [string]$Detail = '')
    $checks.Add([pscustomobject]@{
        id = $Id
        pass = $Pass
        actual = $Actual
        expected = $Expected
        detail = $Detail
    })
}

foreach ($asset in $assetEntries) {
    $filePath = Join-Path $root ($asset.file -replace '/', '\')
    if (-not (Test-Path -LiteralPath $filePath -PathType Leaf)) {
        Add-Check "asset.$($asset.asset_id).exists" $false $false $true $asset.file
        continue
    }

    $hash = (Get-FileHash -LiteralPath $filePath -Algorithm SHA256).Hash
    $stats = [NaiwaP4PixelProbeNative]::Inspect($filePath)
    $assetStats[$asset.file] = $stats
    $expectedDimensions = @($asset.candidate_dimensions_px)
    if ($expectedDimensions.Count -ne 2) { $expectedDimensions = @(512, 512) }

    Add-Check "asset.$($asset.asset_id).sha256" ($hash -ieq [string]$asset.sha256) $hash ([string]$asset.sha256) $asset.file
    Add-Check "asset.$($asset.asset_id).dimensions" ($stats.Width -eq [int]$expectedDimensions[0] -and $stats.Height -eq [int]$expectedDimensions[1]) @($stats.Width, $stats.Height) $expectedDimensions $asset.type
    Add-Check "asset.$($asset.asset_id).visible" ($stats.VisiblePixelsAlpha8 -gt 0) $stats.VisiblePixelsAlpha8 '> 0' 'alpha > 8'
    Add-Check "asset.$($asset.asset_id).transparent_border" ($stats.BorderPixelsAlpha8 -eq 0 -and $stats.CornerAlphaAbove8 -eq 0) ([pscustomobject]@{ border = $stats.BorderPixelsAlpha8; corners = $stats.CornerAlphaAbove8 }) @{ border = 0; corners = 0 } 'alpha > 8'

    if ($asset.type -in @('boss_single_keyframe_candidate', 'boss_idle_transform_frame_candidate', 'boss_action_sequence_frame_candidate')) {
        Add-Check "asset.$($asset.asset_id).foot_anchor" ([Math]::Abs($stats.BoundsBottomAlpha32 - 465) -le 1) $stats.BoundsBottomAlpha32 '464..466' 'visible bottom at alpha > 32'
    }
}

$widgetSource = Get-Content -LiteralPath $widgetSourcePath -Raw
$arenaSource = Get-Content -LiteralPath $arenaSourcePath -Raw
$artIntegrationSource = Get-Content -LiteralPath (Join-Path $root 'UE5/NaiwaP0/Source/NaiwaRuntime/P4/Presentation/NaiwaP4ArtIntegrationPawn.cpp') -Raw
$artIntegrationHeader = Get-Content -LiteralPath (Join-Path $root 'UE5/NaiwaP0/Source/NaiwaRuntime/P4/Presentation/NaiwaP4ArtIntegrationPawn.h') -Raw
$dataTypesSource = Get-Content -LiteralPath (Join-Path $root 'UE5/NaiwaP0/Source/NaiwaRuntime/P4/Data/NaiwaP4DataTypes.h') -Raw
$bossImportSource = Get-Content -LiteralPath (Join-Path $root 'UE5/Scripts/p4_import_w4_boss_runtime_preview.py') -Raw
$bossRuntimeCsvPath = Join-Path $root 'UE5/SourceAssets/P4/Data/P4_W4_Boss_RuntimePreviewFrames.csv'
$bossRuntimeImportReportPath = Join-Path $root 'UE5/Records/p4_alpha/evidence/W4/P4_W4_Boss_RuntimePreview_import_report_v01.json'
$determinismSource = Get-Content -LiteralPath (Join-Path $root 'UE5/NaiwaP0/Source/NaiwaRuntime/P4/Core/NaiwaP4Determinism.cpp') -Raw
Add-Check 'cards.widget_reads_current_rank' ($widgetSource -match 'GetUpgradeCardRank\(UpgradeCardIds\[Index\]\)' -and $widgetSource -match 'BuildUpgradeCardDescription\(\*Row,\s*Rank,\s*Player\)') $true $true 'Presentation uses the current rank and live player state when rebuilding card details.'
Add-Check 'cards.dash_description_uses_live_capacity' ($widgetSource -match 'CurrentCapacity\s*=\s*Player\s*\?\s*Player->DashChargesMax\s*:\s*BaseCapacity\s*\+\s*Rank' -and $widgetSource -match 'NextCapacity\s*=\s*CurrentCapacity\s*\+\s*1') 'rank 1 shows current capacity 3 and next capacity 4 when base capacity is 2' 'computed from current live capacity' 'Source-path check; no gameplay session was run.'
Add-Check 'cards.repeat_selection_increments_dash_rank' ($arenaSource -match 'NewRank\s*=\s*GetUpgradeCardRank\(CardId\)\s*\+\s*1' -and $arenaSource -match 'DashChargesMax\s*=\s*BaseDashCapacity\s*\+\s*NewRank') 'each repeat selection increments cap by one' 'NewRank = prior rank + 1' 'Source-path check; no gameplay session was run.'
Add-Check 'arena.backdrop.faces_camera' ($artIntegrationSource -match 'MakeFromXZ\(CameraRight,\s*-CameraForward\)') $true $true 'Background plane front normal points back toward the camera.'
Add-Check 'combat.melee_range_and_cleave' ($arenaSource -match 'PlayerMeleeRange\s*=\s*160\.0' -and $arenaSource -match 'HitCount\s*=\s*0' -and $arenaSource -match 'QueueDamage\(Player\.EntityId,\s*Target\.EntityId,\s*Player\.MeleeDamage') '160 uu range; every target in the forward arc is hit' 'range constant plus per-target damage loop' 'Source-path assertion; runtime combat still requires a fresh gameplay capture.'
Add-Check 'combat.action_input_buffer' ($arenaSource -match 'PlayerActionInputBufferSeconds\s*=\s*0\.28' -and $arenaSource -match 'MeleeInputBufferTicksRemaining' -and $arenaSource -match 'DashInputBufferTicksRemaining' -and $arenaSource -match 'UltimateInputBufferTicksRemaining') 'melee, dash, ultimate edges buffer briefly' '0.28 second deterministic per-action buffers' ''
Add-Check 'combat.hit_feedback_and_damage_number_scale' ($artIntegrationSource -match 'ImpactPresentationFreezeSecondsRemaining,\s*0\.045f' -and $artIntegrationSource -match 'DamageNumberWorldSize\s*=\s*62\.0f') '45 ms presentation freeze; damage number size 62' 'impact pause and smaller damage number' ''
Add-Check 'boss.charge_is_continuous_and_hashed' ($arenaSource -match 'BossChargeTicksRemaining\s*>\s*0' -and $arenaSource -match 'boss_charge_started' -and $determinismSource -match 'AddInt32\(Entity\.BossChargeTicksRemaining\)') 'charge advances over fixed ticks and is represented in the state hash' 'fixed-step movement, start event, V7 state hash' ''
Add-Check 'combat.enemy_health_bars_track_hp' ($widgetSource.Contains('void UNaiwaP4ArtIntegrationWidget::UpdateEnemyHealthBars') -and $widgetSource.Contains('Enemy.HP') -and $widgetSource.Contains('ProjectWorldLocationToWidgetPosition') -and $widgetSource.Contains('EnemyHealthBarSlots')) 'projected overhead bars follow current enemy HP' 'UMG progress bars projected above enemy sprites' 'Source-path assertion; runtime positioning still requires a fresh gameplay capture.'
Add-Check 'combat.melee_swing_facing_matches_screen' ($artIntegrationSource.Contains('TEXT("fx_melee_swing")') -and $artIntegrationSource.Contains('Player && Player->bFacingRight')) 'swing flips for logical screen-right direction' 'corrected mirrored VFX orientation' 'Source-path assertion; visual direction still requires a fresh gameplay capture.'
$bossSequenceEntries = @($manifest.boss_sequence_batch.sequences) + @($manifest.boss_action_sequence_batch.sequences)
$bossFrameTotal = ($bossSequenceEntries | Measure-Object -Property frame_count -Sum).Sum
$bossRejectedChargeAttackIncluded = @($bossSequenceEntries | Where-Object { $_.state -eq 'charge_attack' }).Count -gt 0
Add-Check 'boss.w4_preview_batch_coverage' ($bossSequenceEntries.Count -eq 10 -and $bossFrameTotal -eq 26 -and -not $bossRejectedChargeAttackIncluded) ([pscustomobject]@{ sequence_count = $bossSequenceEntries.Count; frame_count = $bossFrameTotal; rejected_charge_attack_included = $bossRejectedChargeAttackIncluded }) '3 idle views + 7 accepted WIP action sequences; 26 frames; charge_attack excluded' 'Candidate package and art direction coverage check.'
Add-Check 'boss.w4_preview_import_and_state_binding' ($bossImportSource.Contains('DT_P4_W4_Boss_RuntimePreviewFrames') -and $bossImportSource.Contains('boss_action_sequence_batch') -and $artIntegrationHeader.Contains('FNaiwaP4RuntimeVfxSequence> BossSequences') -and $dataTypesSource.Contains('struct FNaiwaP4BossFrameRow') -and $artIntegrationSource.Contains('ResolveBossSequenceId') -and $artIntegrationSource.Contains('ApplyBossArt') -and $artIntegrationSource.Contains('boss_killed') -and $artIntegrationSource.Contains('StateId = TEXT("death")')) $true $true 'Source-path assertion for preview import, simulation-state poses, and the short Boss death pose.'
$bossCsvRows = @()
$bossTexturePackagesPresent = 0
$bossTableRowCount = 0
if ((Test-Path -LiteralPath $bossRuntimeCsvPath -PathType Leaf) -and (Test-Path -LiteralPath $bossRuntimeImportReportPath -PathType Leaf)) {
    $bossCsvRows = @(Import-Csv -LiteralPath $bossRuntimeCsvPath)
    $bossRuntimeImportReport = Get-Content -LiteralPath $bossRuntimeImportReportPath -Raw | ConvertFrom-Json
    $bossTableRowCount = [int]$bossRuntimeImportReport.frame_table_result.row_count
    foreach ($row in $bossCsvRows) {
        $assetRef = [string]$row.FrameTexture
        if ($assetRef -notmatch "^Texture2D'.+'$") { continue }
        $objectPath = $assetRef.Substring($assetRef.IndexOf("'") + 1).TrimEnd([char]39)
        $packagePath = $objectPath.Substring(0, $objectPath.LastIndexOf('.'))
        $contentRelative = ($packagePath -replace '^/Game/', '').Replace('/', '\')
        $textureAssetPath = Join-Path $root (Join-Path 'UE5/NaiwaP0/Content' ($contentRelative + '.uasset'))
        if (Test-Path -LiteralPath $textureAssetPath -PathType Leaf) { $bossTexturePackagesPresent++ }
    }
}
Add-Check 'boss.w4_content_texture_packages_and_table_rows' ($bossCsvRows.Count -eq 26 -and $bossTableRowCount -eq 26 -and $bossTexturePackagesPresent -eq 26) ([pscustomobject]@{ csv_rows = $bossCsvRows.Count; datatable_rows = $bossTableRowCount; texture_packages_present = $bossTexturePackagesPresent }) '26 CSV rows, 26 imported DataTable rows, and all 26 referenced Texture2D packages present in project Content' 'PixelProbe cross-checks imported row references against saved UE asset packages.'

$sequences = @()
if ($manifest.boss_sequence_batch -and $manifest.boss_sequence_batch.sequences) { $sequences += @($manifest.boss_sequence_batch.sequences) }
if ($manifest.boss_action_sequence_batch -and $manifest.boss_action_sequence_batch.sequences) { $sequences += @($manifest.boss_action_sequence_batch.sequences) }
if ($manifest.vfx_sequence_batch -and $manifest.vfx_sequence_batch.sequences) { $sequences += @($manifest.vfx_sequence_batch.sequences) }
$sequenceReports = [System.Collections.Generic.List[object]]::new()

foreach ($sequence in $sequences) {
    $frameFiles = @($sequence.frame_files)
    $expectedCount = [int]$sequence.frame_count
    $durations = @()
    if ($sequence.frame_durations_seconds) { $durations = @($sequence.frame_durations_seconds) }
    elseif ($sequence.frame_duration_seconds) { $durations = @(1..$expectedCount | ForEach-Object { [double]$sequence.frame_duration_seconds }) }

    Add-Check "sequence.$($sequence.sequence_id).frame_count" ($frameFiles.Count -eq $expectedCount) $frameFiles.Count $expectedCount ''
    Add-Check "sequence.$($sequence.sequence_id).timing" ($durations.Count -eq $expectedCount -and @($durations | Where-Object { [double]$_ -le 0 }).Count -eq 0) $durations 'one positive duration per frame' ''

    $deltas = [System.Collections.Generic.List[object]]::new()
    for ($i = 1; $i -lt $frameFiles.Count; $i++) {
        $previous = $assetStats[$frameFiles[$i - 1]]
        $current = $assetStats[$frameFiles[$i]]
        if ($null -eq $previous -or $null -eq $current) {
            Add-Check "sequence.$($sequence.sequence_id).frame_pair_$($i - 1)_$i" $false 'missing pixel buffer' 'both frames present in manifest asset list' ''
            continue
        }
        $delta = [NaiwaP4PixelProbeNative]::Compare($previous, $current)
        $pass = $delta.ChangedForegroundPixels -ge 100 -and $delta.ChangedForegroundRatio -ge 0.001
        Add-Check "sequence.$($sequence.sequence_id).frame_pair_$($i - 1)_$i" $pass ([pscustomobject]@{
            changed_foreground_pixels = $delta.ChangedForegroundPixels
            changed_foreground_ratio = [Math]::Round($delta.ChangedForegroundRatio, 5)
            alpha_mask_changed_ratio = [Math]::Round($delta.AlphaMaskChangedRatio, 5)
            mean_abs_alpha_delta = [Math]::Round($delta.MeanAbsAlphaDelta, 5)
        }) 'at least 100 changed visible pixels and >=0.001 foreground change ratio' ''
        $deltas.Add([pscustomobject]@{
            from = $frameFiles[$i - 1]
            to = $frameFiles[$i]
            changed_foreground_pixels = $delta.ChangedForegroundPixels
            changed_foreground_ratio = [Math]::Round($delta.ChangedForegroundRatio, 5)
            alpha_mask_changed_ratio = [Math]::Round($delta.AlphaMaskChangedRatio, 5)
            mean_abs_alpha_delta = [Math]::Round($delta.MeanAbsAlphaDelta, 5)
        })
    }

    $sequenceReports.Add([pscustomobject]@{
        sequence_id = $sequence.sequence_id
        state = $sequence.state
        effect_id = $sequence.effect_id
        frame_count = $frameFiles.Count
        playback_mode = $sequence.playback_mode
        frame_durations_seconds = $durations
        adjacent_frame_pixel_deltas = @($deltas)
        runtime_ready = [bool]$sequence.runtime_ready
    })
}

$a2Report = Get-Content -LiteralPath $a2ReportPath -Raw | ConvertFrom-Json
$a2Path = [string]$a2Report.source
if (Test-Path -LiteralPath $a2Path -PathType Leaf) {
    $a2Hash = (Get-FileHash -LiteralPath $a2Path -Algorithm SHA256).Hash
    $a2Stats = [NaiwaP4PixelProbeNative]::Inspect($a2Path)
    Add-Check 'a2_scene.source_hash' ($a2Hash -ieq [string]$a2Report.source_sha256) $a2Hash ([string]$a2Report.source_sha256) $a2Path
    Add-Check 'a2_scene.dimensions' ($a2Stats.Width -eq [int]$a2Report.source_dimensions_px[0] -and $a2Stats.Height -eq [int]$a2Report.source_dimensions_px[1]) @($a2Stats.Width, $a2Stats.Height) @($a2Report.source_dimensions_px) ''
    Add-Check 'a2_scene.pixel_variation' ($a2Stats.StdDevLuma -gt 10 -and $a2Stats.MaxLuma - $a2Stats.MinLuma -gt 60) ([pscustomobject]@{ mean_luma = [Math]::Round($a2Stats.MeanLuma, 2); stddev_luma = [Math]::Round($a2Stats.StdDevLuma, 2); min_luma = $a2Stats.MinLuma; max_luma = $a2Stats.MaxLuma }) 'stddev > 10 and luma range > 60' 'Detects blank/flat export; does not judge composition.'
} else {
    Add-Check 'a2_scene.source_exists' $false $a2Path 'existing source image' ''
}

$runtimeScreenshotReport = $null
if (-not [string]::IsNullOrWhiteSpace($RuntimeScreenshot)) {
    $shotPath = (Resolve-Path -LiteralPath $RuntimeScreenshot).Path
    $shotStats = [NaiwaP4PixelProbeNative]::Inspect($shotPath)
    $viewportRect = @()
    if ([string]::IsNullOrWhiteSpace($RuntimeViewport)) {
        $viewportRect = @(0, 0, $shotStats.Width, $shotStats.Height)
    } else {
        $viewportRect = @($RuntimeViewport -split '[,;\s]+' | ForEach-Object { [int]::Parse($_) })
    }
    if ($viewportRect.Count -ne 4) { throw 'RuntimeViewport must be four integers: X,Y,Width,Height.' }
    $orientationScores = @([NaiwaP4PixelProbeNative]::CompareBackdropOrientations(
        $a2Path, $shotPath, [int]$viewportRect[0], [int]$viewportRect[1],
        [int]$viewportRect[2], [int]$viewportRect[3]))
    $normalScore = @($orientationScores | Where-Object { $_.Orientation -eq 'normal' })[0]
    $bestScore = @($orientationScores | Sort-Object Correlation -Descending)[0]
    $orientationPass = $normalScore.Correlation -ge 0.20 -and $normalScore.Correlation -ge ($bestScore.Correlation - 0.03)
    Add-Check 'runtime_background.orientation' $orientationPass ([pscustomobject]@{
        best_matching_orientation = $bestScore.Orientation
        normal_correlation = [Math]::Round($normalScore.Correlation, 5)
        best_correlation = [Math]::Round($bestScore.Correlation, 5)
        score_margin = [Math]::Round($normalScore.Correlation - $bestScore.Correlation, 5)
    }) 'normal orientation matches the A2 source within 0.03 correlation, with correlation >= 0.20' 'Tests four orientations against A2 after masking HUD and combat sprites.'
    $runtimeScreenshotReport = [pscustomobject]@{
        status = 'captured_for_pixel_analysis'
        path = $shotPath
        dimensions_px = @($shotStats.Width, $shotStats.Height)
        nonblack_luma_stddev = [Math]::Round($shotStats.StdDevLuma, 2)
        luma_min = $shotStats.MinLuma
        luma_max = $shotStats.MaxLuma
        viewport = [pscustomobject]@{ x = $viewportRect[0]; y = $viewportRect[1]; width = $viewportRect[2]; height = $viewportRect[3] }
        background_orientation_scores = @($orientationScores | ForEach-Object { [pscustomobject]@{
            orientation = $_.Orientation
            correlation = [Math]::Round($_.Correlation, 5)
            mean_absolute_error = [Math]::Round($_.MeanAbsoluteError, 2)
            channel_samples = $_.ChannelSamples
        } })
        note = 'Background orientation is compared with the A2 source after masking HUD and combat sprites.'
    }
    Add-Check 'runtime_screenshot.nonblank' ($shotStats.StdDevLuma -gt 8 -and $shotStats.MaxLuma -gt 50) ([pscustomobject]@{ stddev = [Math]::Round($shotStats.StdDevLuma, 2); max = $shotStats.MaxLuma }) 'stddev > 8 and max luma > 50' $shotPath
}

$failed = @($checks | Where-Object { -not $_.pass })
$report = [pscustomobject]@{
    schema = 'naiwa.p4.source_pixel_probe/5'
    generated_utc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
    manifest = [pscustomobject]@{
        file = 'UE5/SourceAssets/P4/Manifests/P4_W4_BossAndVFX_manifest_v06.json'
        package_id = $manifest.package_id
        version = $manifest.version
        status = $manifest.status
        candidate_asset_count = $assetEntries.Count
        sequence_count = $sequences.Count
    }
    verdict = if ($failed.Count -eq 0) { 'PASS_PIXEL_PROBE' } else { 'FAIL_PIXEL_PROBE' }
    checks_passed = $checks.Count - $failed.Count
    checks_failed = $failed.Count
    checks = @($checks)
    sequence_pixel_report = @($sequenceReports)
    runtime_scene_capture = if ($runtimeScreenshotReport) { $runtimeScreenshotReport } else {
        [pscustomobject]@{ status = 'not_run'; reason = 'No fresh P4 runtime screenshot was supplied; this run only probes source pixels and sequence timing.' }
    }
    known_limitations = @(
        'W4 animation and VFX frames are RuntimeWIP candidates; pixel QA does not promote them to RuntimeReady.',
        'Card-rank assertions are source-path checks, not runtime gameplay tests.',
        'Enemy health bars and melee swing orientation assertions inspect source paths, not a live gameplay frame.',
        'W4 and card runtime behavior still require current gameplay observation; the supplied screenshot only validates scene orientation and nonblank output.'
    )
}

$report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $reportPath -Encoding utf8
Write-Output "P4 pixel probe verdict: $($report.verdict) ($($report.checks_passed)/$($checks.Count)); report: $reportPath"
if ($failed.Count -gt 0) {
    $failed | ForEach-Object { Write-Error ("FAIL {0}: actual={1} expected={2} {3}" -f $_.id, ($_.actual | ConvertTo-Json -Compress -Depth 5), ($_.expected | ConvertTo-Json -Compress -Depth 5), $_.detail) }
    exit 1
}
