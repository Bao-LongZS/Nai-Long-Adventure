param(
    [string]$RepositoryRoot = ''
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
} else {
    $RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
}

$alphaWaveRelative = 'UE5/SourceAssets/P4/Data/P4WaveAlpha.csv'
$outputRelative = 'UE5/SourceAssets/P4/Data/Beta6'
$outputDirectory = Join-Path $RepositoryRoot $outputRelative
$alphaWavePath = Join-Path $RepositoryRoot ($alphaWaveRelative.Replace('/', '\'))
$designReferenceDirectory = Join-Path $RepositoryRoot 'UE5/Records/p4_alpha/design_reference'
$chapterJsonFiles = @(Get-ChildItem -LiteralPath $designReferenceDirectory -File -Filter '*.json' | Where-Object { $_.BaseName -match '^P4_.+_v1$' })
if ($chapterJsonFiles.Count -ne 1) {
    throw "Expected exactly one P4 versioned JSON design source in $designReferenceDirectory; found $($chapterJsonFiles.Count)."
}
$sourceJsonPath = $chapterJsonFiles[0].FullName
$sourceJsonRelative = $sourceJsonPath.Substring($RepositoryRoot.Length).TrimStart('\').Replace('\', '/')
$scenePreviewReportPath = Join-Path $RepositoryRoot 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_ScenePreview_build_report_v01.json'
$sceneReviewApprovalPath = Join-Path $RepositoryRoot 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_SceneReview_Approval_v01.json'
$alphaArenaManifestPath = Join-Path $RepositoryRoot 'UE5/SourceAssets/P4/Manifests/P4_A2_ArenaScene_manifest_v02.json'
$runtimeTuningPath = Join-Path $RepositoryRoot 'UE5/SourceAssets/P4/Data/Beta6/P4Beta6_RuntimeTuning_v01.json'
$environmentArtImportReportPath = Join-Path $RepositoryRoot 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_EnvironmentArt_import_report_v01.json'
$scenePreviewByChapter = @{}
$environmentArtByChapter = @{}
$scenePreviewReport = $null
$sceneReviewApproval = $null
$alphaArenaManifest = $null
$environmentArtImportReport = $null
$sceneVisualReviewApproved = $false
if (Test-Path -LiteralPath $scenePreviewReportPath -PathType Leaf) {
    $scenePreviewReport = Get-Content -LiteralPath $scenePreviewReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($scenePreviewReport.schema -ne 'naiwa.p4.beta6_scene_preview_build/1' -or
        $scenePreviewReport.status -ne 'maps_saved_pending_visual_review' -or
        @($scenePreviewReport.maps).Count -ne 5) {
        throw 'Beta6 scene preview report does not contain the expected five saved maps pending visual review.'
    }
    foreach ($preview in $scenePreviewReport.maps) {
        if ($preview.chapter_id -notmatch '^chapter_0?[2-6]$' -or $preview.texture_asset -notmatch '^/Game/' -or
            $preview.material_asset -notmatch '^/Game/' -or $preview.review_map -notmatch '^/Game/') {
            throw "Beta6 scene preview report contains an invalid asset row for $($preview.chapter_id)."
        }
        $scenePreviewByChapter[[string]$preview.chapter_id] = $preview
    }
}
if (Test-Path -LiteralPath $sceneReviewApprovalPath -PathType Leaf) {
    $sceneReviewApproval = Get-Content -LiteralPath $sceneReviewApprovalPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($sceneReviewApproval.schema -ne 'naiwa.p4.beta6_scene_review_approval/1' -or
        $sceneReviewApproval.scope -ne 'chapter_2_to_6_scene_preview_review' -or
        $sceneReviewApproval.status -ne 'owner_visual_review_approved' -or
        $scenePreviewByChapter.Count -ne 5) {
        throw 'Beta6 scene review approval does not match the five imported Chapter 2-6 scene previews.'
    }
    $sceneVisualReviewApproved = $true
}
if (Test-Path -LiteralPath $environmentArtImportReportPath -PathType Leaf) {
    $environmentArtImportReport = Get-Content -LiteralPath $environmentArtImportReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($environmentArtImportReport.schema -ne 'naiwa.p4.beta6.environment_art_import/1' -or
        $environmentArtImportReport.status -ne 'textures_imported_pending_engineering_theme_binding_and_readability_review' -or
        [int]$environmentArtImportReport.asset_count -ne 5 -or
        @($environmentArtImportReport.assets).Count -ne 5) {
        throw 'Beta6 environment art import report does not contain the expected five imported production textures.'
    }
    foreach ($environment in $environmentArtImportReport.assets) {
        $chapterId = [string]$environment.chapter_id
        if ($chapterId -notmatch '^chapter_[2-6]$' -or $environment.texture_asset -notmatch '^/Game/' -or
            $environment.status -ne 'texture_imported_pending_engineering_theme_binding_and_readability_review') {
            throw "Beta6 environment art import report has an invalid asset row for $chapterId."
        }
        $relativeAsset = ([string]$environment.texture_asset).Substring('/Game/'.Length).Replace('/', '\') + '.uasset'
        $assetFile = Join-Path $RepositoryRoot (Join-Path 'UE5/NaiwaP0/Content' $relativeAsset)
        if (-not (Test-Path -LiteralPath $assetFile -PathType Leaf)) { throw "Imported Beta6 environment texture is missing: $assetFile" }
        $environmentArtByChapter[$chapterId] = $environment
    }
}
if (-not (Test-Path -LiteralPath $alphaArenaManifestPath -PathType Leaf)) {
    throw "Missing accepted Alpha arena scene manifest: $alphaArenaManifestPath"
}
if (Test-Path -LiteralPath $alphaArenaManifestPath -PathType Leaf) {
    $alphaArenaManifest = Get-Content -LiteralPath $alphaArenaManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($alphaArenaManifest.schema -ne 'naiwa.p4.a2_arena_scene/1' -or
        $alphaArenaManifest.ue_texture_asset -notmatch '^/Game/') {
        throw 'Accepted Alpha arena manifest does not contain a valid backdrop texture reference.'
    }
    $relativeAsset = ([string]$alphaArenaManifest.ue_texture_asset).Substring('/Game/'.Length).Replace('/', '\') + '.uasset'
    $assetFile = Join-Path $RepositoryRoot (Join-Path 'UE5/NaiwaP0/Content' $relativeAsset)
    if (-not (Test-Path -LiteralPath $assetFile -PathType Leaf)) { throw "Accepted Alpha backdrop texture is missing: $assetFile" }
}

if (-not (Test-Path -LiteralPath $sourceJsonPath -PathType Leaf)) {
    throw "Missing chapter design source: $sourceJsonPath"
}
if (-not (Test-Path -LiteralPath $alphaWavePath -PathType Leaf)) {
    throw "Missing accepted Alpha wave source: $alphaWavePath"
}
if (-not (Test-Path -LiteralPath $runtimeTuningPath -PathType Leaf)) {
    throw "Missing Beta6 runtime tuning source: $runtimeTuningPath"
}

$chapterData = Get-Content -LiteralPath $sourceJsonPath -Raw -Encoding UTF8 | ConvertFrom-Json
$runtimeTuning = Get-Content -LiteralPath $runtimeTuningPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($chapterData.schema -ne 'naiwa.p4.arena.chapters/2' -or $chapterData.version -ne '2.0') {
    throw "Unexpected chapter source schema/version: $($chapterData.schema) / $($chapterData.version)"
}
if ($runtimeTuning.schema -ne 'naiwa.p4.beta6.runtime_tuning/1' -or
    $runtimeTuning.status -ne 'engineering_baseline_for_beta6_runtime') {
    throw 'Beta6 runtime tuning source has an unexpected schema or status.'
}

$expectedTotals = @{
    '1' = 20
    '2' = 30
    '3' = 42
    '4' = 9
    '5' = 56
    '6' = 12
}
$chapterByNumber = @{}
foreach ($chapter in $chapterData.chapters) {
    $chapterByNumber[[string][int]$chapter.chapter] = $chapter
}

$alphaWaveRows = @(Import-Csv -LiteralPath $alphaWavePath)
if ($alphaWaveRows.Count -eq 0) {
    throw 'The accepted Alpha wave CSV has no rows.'
}

$chapterRows = [System.Collections.Generic.List[object]]::new()
$themeRows = [System.Collections.Generic.List[object]]::new()
$waveRows = [System.Collections.Generic.List[object]]::new()
$chapter1JsonWaveTotals = [System.Collections.Generic.List[int]]::new()
$chapter1AcceptedWaveTotals = [System.Collections.Generic.List[int]]::new()

for ($chapterNumber = 1; $chapterNumber -le 6; $chapterNumber++) {
    $chapter = $chapterByNumber[[string]$chapterNumber]
    if (-not $chapter) {
        throw "Chapter $chapterNumber is missing from the source JSON."
    }

    $expectedTotal = [int]$expectedTotals[[string]$chapterNumber]
    $dataSource = if ($chapterNumber -eq 1) { 'accepted_alpha_wave_csv' } else { 'chapter_json_v2' }
    $derivedWaveCount = 0
    $derivedEnemyTotal = 0

    if ($chapterNumber -eq 1) {
        foreach ($sourceWave in $chapter.waves) {
            $jsonWaveTotal = 0
            foreach ($spawn in $sourceWave.spawns.PSObject.Properties) {
                $jsonWaveTotal += [int]$spawn.Value
            }
            $chapter1JsonWaveTotals.Add($jsonWaveTotal)
        }

        $groupedAcceptedWaves = $alphaWaveRows | Group-Object { [int]$_.WaveIndex } | Sort-Object { [int]$_.Name }
        foreach ($waveGroup in $groupedAcceptedWaves) {
            $waveIndex = [int]$waveGroup.Name
            $waveTotal = 0
            foreach ($spawnRow in ($waveGroup.Group | Sort-Object { [int]$_.SpawnOrder })) {
                $spawnOrder = [int]$spawnRow.SpawnOrder
                $count = [int]$spawnRow.Count
                $waveTotal += $count
                $waveRows.Add([ordered]@{
                    RowName = ('ch01_w{0:D2}_s{1:D2}_{2}' -f $waveIndex, $spawnOrder, ([string]$spawnRow.EnemyTypeId).ToLowerInvariant())
                    ChapterIndex = 1
                    WaveIndex = $waveIndex
                    SpawnOrder = $spawnOrder
                    EnemyTypeId = ([string]$spawnRow.EnemyTypeId).ToLowerInvariant()
                    Count = $count
                    SpawnIntervalSeconds = ([string]$spawnRow.SpawnIntervalSeconds)
                    bSpawnIntervalConfigured = 'true'
                    SourceKind = 'accepted_alpha_wave_csv'
                })
            }
            $chapter1AcceptedWaveTotals.Add($waveTotal)
            $derivedWaveCount++
            $derivedEnemyTotal += $waveTotal
        }
    } else {
        $intervalProperty = $runtimeTuning.chapterSpawnIntervals.PSObject.Properties[[string]$chapterNumber]
        if (-not $intervalProperty -or [double]$intervalProperty.Value -le 0.0) {
            throw "Chapter $chapterNumber is missing a positive authored Beta6 spawn interval."
        }
        $chapterSpawnInterval = [double]$intervalProperty.Value
        foreach ($sourceWave in $chapter.waves) {
            $waveIndex = [int]$sourceWave.wave
            $spawnOrder = 0
            foreach ($spawn in $sourceWave.spawns.PSObject.Properties) {
                $enemyTypeId = ([string]$spawn.Name).ToLowerInvariant()
                $count = [int]$spawn.Value
                $waveRows.Add([ordered]@{
                    RowName = ('ch{0:D2}_w{1:D2}_s{2:D2}_{3}' -f $chapterNumber, $waveIndex, $spawnOrder, $enemyTypeId)
                    ChapterIndex = $chapterNumber
                    WaveIndex = $waveIndex
                    SpawnOrder = $spawnOrder
                    EnemyTypeId = $enemyTypeId
                    Count = $count
                    SpawnIntervalSeconds = $chapterSpawnInterval.ToString([Globalization.CultureInfo]::InvariantCulture)
                    bSpawnIntervalConfigured = 'true'
                    SourceKind = 'chapter_json_v2_plus_engineering_runtime_tuning_v1'
                })
                $spawnOrder++
                $derivedEnemyTotal += $count
            }
            $derivedWaveCount++
        }
    }

    if ($derivedEnemyTotal -ne $expectedTotal) {
        throw "Chapter $chapterNumber projects $derivedEnemyTotal ordinary enemies; handoff baseline requires $expectedTotal."
    }
    if ($derivedWaveCount -ne [int]$chapter.waves.Count) {
        throw "Chapter $chapterNumber wave count differs from its source baseline."
    }

    $themeId = ('theme_{0}_{1}' -f ([string]$chapter.scene.base).ToLowerInvariant(), ([string]$chapter.scene.tint).ToLowerInvariant())
    $themeProps = @($chapter.scene.props | ForEach-Object { [string]$_ })
    $chapterScenePreviewId = "chapter_$chapterNumber"
    $backdropTextureAsset = if ($chapterNumber -eq 1) {
        [string]$alphaArenaManifest.ue_texture_asset
    } elseif ($environmentArtByChapter.ContainsKey($chapterScenePreviewId)) {
        [string]$environmentArtByChapter[$chapterScenePreviewId].texture_asset
    } else {
        ''
    }
    $backdropTextureReference = if ($backdropTextureAsset) {
        $textureObjectName = ($backdropTextureAsset -split '/')[-1]
        "Texture2D'$backdropTextureAsset.$textureObjectName'"
    } else {
        'None'
    }
    $runtimeAssetBindingsStatus = if ($chapterNumber -eq 1) {
        'accepted_alpha_theme_frozen'
    } elseif ($environmentArtByChapter.ContainsKey($chapterScenePreviewId) -and $sceneVisualReviewApproved) {
        'production_environment_texture_imported_pending_review_maps_theme_binding_and_readability_review'
    } elseif ($environmentArtByChapter.ContainsKey($chapterScenePreviewId)) {
        'production_environment_texture_imported_pending_scene_review_and_runtime_binding'
    } elseif ($scenePreviewByChapter.ContainsKey($chapterScenePreviewId)) {
        if ($sceneVisualReviewApproved) { 'preview_scene_visual_review_approved_pending_production_asset_and_runtime_binding' } else { 'preview_scene_imported_pending_visual_review' }
    } else {
        'pending_asset_audit_or_production'
    }
    $themeRows.Add([ordered]@{
        RowName = $themeId
        ThemeId = $themeId
        SceneBase = ([string]$chapter.scene.base)
        SceneTint = ([string]$chapter.scene.tint)
        SceneLight = ([string]$chapter.scene.light)
        DesignPropIds = ('(' + ($themeProps -join ',') + ')')
        BackdropTexture = $backdropTextureReference
        RuntimeAssetBindingsStatus = $runtimeAssetBindingsStatus
    })
    $chapterRows.Add([ordered]@{
        RowName = ('chapter_{0:D2}' -f $chapterNumber)
        ChapterIndex = $chapterNumber
        DisplayName = ([string]$chapter.name)
        ChapterType = ([string]$chapter.type)
        ThemeId = $themeId
        WaveCount = $derivedWaveCount
        OrdinaryEnemyCount = $derivedEnemyTotal
        BossId = if ($null -eq $chapter.boss_id) { '' } else { [string]$chapter.boss_id }
        DurationTargetSeconds = [int]$chapter.duration_target_s
        DataSource = $dataSource
    })
}

$sumOfEnemies = 0
$sumOfWaves = 0
foreach ($row in $chapterRows) {
    $sumOfEnemies += [int]$row.OrdinaryEnemyCount
    $sumOfWaves += [int]$row.WaveCount
}
if ($sumOfEnemies -ne 169 -or $sumOfWaves -ne 32) {
    throw "Six-chapter total differs from handoff baseline (enemies=$sumOfEnemies, waves=$sumOfWaves)."
}

function ConvertTo-CsvField([object]$Value) {
    if ($null -eq $Value) { return '' }
    $text = [string]$Value
    if ($text.Contains('"') -or $text.Contains(',') -or $text.Contains("`r") -or $text.Contains("`n")) {
        return '"' + $text.Replace('"', '""') + '"'
    }
    return $text
}

function ConvertTo-CsvContent([System.Collections.Generic.List[object]]$Rows) {
    if ($Rows.Count -eq 0) { return '' }
    $columns = @($Rows[0].Keys)
    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add((($columns | ForEach-Object { ConvertTo-CsvField $_ }) -join ','))
    foreach ($row in $Rows) {
        $fields = foreach ($column in $columns) { ConvertTo-CsvField $row[$column] }
        $lines.Add(($fields -join ','))
    }
    return (($lines -join "`r`n") + "`r`n")
}

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$encoding = [System.Text.UTF8Encoding]::new($false)
$chapterCsvPath = Join-Path $outputDirectory 'P4Beta6_Chapters.csv'
$themeCsvPath = Join-Path $outputDirectory 'P4Beta6_Themes.csv'
$waveCsvPath = Join-Path $outputDirectory 'P4Beta6_WaveSpawns.csv'
$manifestPath = Join-Path $outputDirectory 'P4Beta6_ScopeProjection.json'

[System.IO.File]::WriteAllText($chapterCsvPath, (ConvertTo-CsvContent $chapterRows), $encoding)
[System.IO.File]::WriteAllText($themeCsvPath, (ConvertTo-CsvContent $themeRows), $encoding)
[System.IO.File]::WriteAllText($waveCsvPath, (ConvertTo-CsvContent $waveRows), $encoding)

$jsonChapter1WaveTotals = @($chapter1JsonWaveTotals.ToArray())
$acceptedChapter1WaveTotals = @($chapter1AcceptedWaveTotals.ToArray())
$jsonWaveDataOverridden = ($jsonChapter1WaveTotals.Count -gt 0 -and ($jsonChapter1WaveTotals -join ',') -ne ($acceptedChapter1WaveTotals -join ','))
$missingIntervalChapters = @()
$manifest = [ordered]@{
    artifact = 'P4 Beta6 Scope Projection'
    projectionVersion = '0.5'
    generatedAtLocalDate = (Get-Date).ToString('yyyy-MM-dd')
    source = [ordered]@{
        chapterDataPath = $sourceJsonRelative
        chapterDataSchema = [string]$chapterData.schema
        chapterDataVersion = [string]$chapterData.version
        chapterDataSha256 = Get-Sha256 $sourceJsonPath
        runtimeTuningPath = 'UE5/SourceAssets/P4/Data/Beta6/P4Beta6_RuntimeTuning_v01.json'
        runtimeTuningSha256 = Get-Sha256 $runtimeTuningPath
        runtimeTuningVersion = [string]$runtimeTuning.version
        acceptedAlphaWavePath = $alphaWaveRelative
        acceptedAlphaWaveSha256 = Get-Sha256 $alphaWavePath
        chapterOneAuthority = 'Accepted Alpha wave CSV; JSON chapter 1 is preserved as historical design reference and does not override the accepted runtime wave composition.'
    }
    scope = [ordered]@{
        chapterIndices = @(1, 2, 3, 4, 5, 6)
        betaAddedChapterIndices = @(2, 3, 4, 5, 6)
        deferredChapterIndices = @(7, 8, 9, 10)
        themeIdRule = 'theme_<lowercase scene.base>_<lowercase scene.tint>; derived from source scene fields'
        themeBackdropRule = 'BackdropTexture projects the accepted Alpha Chapter 1 texture and imported Beta6 Chapter 2-6 production textures; non-Alpha chapter playability remains blocked until runtime binding and readability acceptance.'
        scenePropRule = 'DesignPropIds preserve source design tokens only; they are not approved UE asset bindings.'
        chapterRows = @($chapterRows.ToArray())
        themeRows = @($themeRows.ToArray())
        waveSpawnRows = @($waveRows.ToArray())
        totalChapters = $chapterRows.Count
        totalWaves = $sumOfWaves
        totalOrdinaryEnemies = $sumOfEnemies
        chapterOneOverride = [ordered]@{
            applied = $true
            jsonWaveTotals = $jsonChapter1WaveTotals
            acceptedAlphaWaveTotals = $acceptedChapter1WaveTotals
            jsonWaveCompositionDiffers = $jsonWaveDataOverridden
            totalStillMatches = (($jsonChapter1WaveTotals | Measure-Object -Sum).Sum -eq ($acceptedChapter1WaveTotals | Measure-Object -Sum).Sum)
        }
    }
    beta6ScenePreviewImport = [ordered]@{
        sourceReportPath = if ($scenePreviewReport) { 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_ScenePreview_build_report_v01.json' } else { '' }
        sourceReportSha256 = if ($scenePreviewReport) { Get-Sha256 $scenePreviewReportPath } else { '' }
        reviewApprovalPath = if ($sceneReviewApproval) { 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_SceneReview_Approval_v01.json' } else { '' }
        reviewApprovalSha256 = if ($sceneReviewApproval) { Get-Sha256 $sceneReviewApprovalPath } else { '' }
        status = if ($scenePreviewByChapter.Count -eq 5) { [string]$scenePreviewReport.status } else { 'pending_scene_preview_import' }
        visualReviewStatus = if ($sceneVisualReviewApproved) { 'owner_visual_review_approved' } else { 'pending_visual_review' }
        maps = if ($scenePreviewByChapter.Count -eq 5) { @($scenePreviewReport.maps) } else { @() }
        runtimeBindingStatus = if ($sceneVisualReviewApproved) { 'visual review approved; production art, prop audit, and runtime theme/component bindings remain pending' } else { 'preview imports are not production runtime theme bindings; visual review and final art/runtime acceptance remain pending' }
    }
    beta6ProductionEnvironmentImport = [ordered]@{
        sourceReportPath = if ($environmentArtImportReport) { 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_EnvironmentArt_import_report_v01.json' } else { '' }
        sourceReportSha256 = if ($environmentArtImportReport) { Get-Sha256 $environmentArtImportReportPath } else { '' }
        status = if ($environmentArtByChapter.Count -eq 5) { [string]$environmentArtImportReport.status } else { 'pending_production_environment_texture_import' }
        importedTextureCount = $environmentArtByChapter.Count
        assets = if ($environmentArtByChapter.Count -eq 5) { @($environmentArtImportReport.assets) } else { @() }
        reviewMapStatus = 'pending_environment_review_map_generation'
        runtimeBindingStatus = 'production textures are imported and projected as soft texture references; review materials/maps, actual DataTable import, runtime binding acceptance, and fixed-camera readability review remain pending'
    }
    outputs = @(
        [ordered]@{ path = "$outputRelative/P4Beta6_Chapters.csv"; rowCount = $chapterRows.Count; sha256 = Get-Sha256 $chapterCsvPath; plannedDataTable = '/Game/Naiwa/P4/Data/DT_P4_Beta6_Chapters' },
        [ordered]@{ path = "$outputRelative/P4Beta6_Themes.csv"; rowCount = $themeRows.Count; sha256 = Get-Sha256 $themeCsvPath; plannedDataTable = '/Game/Naiwa/P4/Data/DT_P4_Beta6_Themes' },
        [ordered]@{ path = "$outputRelative/P4Beta6_WaveSpawns.csv"; rowCount = $waveRows.Count; sha256 = Get-Sha256 $waveCsvPath; plannedDataTable = '/Game/Naiwa/P4/Data/DT_P4_Beta6_WaveSpawns' }
    )
    ueDataTableImport = [ordered]@{
        status = 'pending_row_struct_and_native_asset_import'
        importedAssets = @()
        rowCounts = $null
        note = 'CSV projections are authoritative handoff artifacts only; runtime DataTables are not claimed until row structs compile and UE import readback is recorded.'
    }
    unresolvedSourceFields = @(
        [ordered]@{
            field = 'RuntimeAssetBindings'
            affectedChapters = @(2, 3, 4, 5, 6)
            sourceStatus = if ($environmentArtByChapter.Count -eq 5 -and $sceneVisualReviewApproved) { 'five production environment textures are imported and owner scene review is approved; review materials/maps, production cleanup, prop audit, and Beta runtime bindings remain pending' } elseif ($scenePreviewByChapter.Count -eq 5 -and $sceneVisualReviewApproved) { 'five preview scenes are imported and owner visual review is approved; production art, prop audit, and Beta runtime bindings remain pending' } elseif ($scenePreviewByChapter.Count -eq 5) { 'preview textures/materials/maps are imported; visual review, production art, prop audit, and Beta runtime bindings remain pending' } else { 'scene props are design tokens; P2/P3 reuse candidates and P4-only props are not resolved to runtime assets' }
            action = 'Resolve each prop through the reuse/ownership ledger or create a P4 asset mapping, then complete readability acceptance before final visual sign-off.'
        }
    )
    sourceChapterOneReference = [ordered]@{
        chapter = 1
        originalJsonWaveTotals = $chapter1JsonWaveTotals
        acceptedAlphaWaveTotals = $chapter1AcceptedWaveTotals
        usedWaveSource = $alphaWaveRelative
        acceptedEnemyCount = [int]$chapterRows[0].OrdinaryEnemyCount
    }
}

$manifestJson = $manifest | ConvertTo-Json -Depth 30
[System.IO.File]::WriteAllText($manifestPath, ($manifestJson + "`r`n"), $encoding)

Write-Output "Generated six-chapter projection: $outputDirectory"
Write-Output "Chapters=$($chapterRows.Count); themes=$($themeRows.Count); wave-spawn rows=$($waveRows.Count); waves=$sumOfWaves; ordinary enemies=$sumOfEnemies"
Write-Output "Chapter CSV SHA256=$(Get-Sha256 $chapterCsvPath)"
Write-Output "Theme CSV SHA256=$(Get-Sha256 $themeCsvPath)"
Write-Output "Wave CSV SHA256=$(Get-Sha256 $waveCsvPath)"
Write-Output 'Native UE DataTable imports remain explicitly pending.'
Write-Output "Beta6 Chapter 2-6 scene visual review status=$(if ($sceneVisualReviewApproved) { 'owner_visual_review_approved' } else { 'pending_visual_review' }); art/runtime bindings remain pending."
Write-Output "Production environment textures imported=$($environmentArtByChapter.Count); review maps and runtime bindings remain pending."
