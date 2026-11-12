param(
	[string]$RepositoryRoot = ''
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
	$RepositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
} else {
	$RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
}

$dataDirectory = Join-Path $RepositoryRoot 'UE5/SourceAssets/P4/Data/Beta6'
$themeCsvPath = Join-Path $dataDirectory 'P4Beta6_Themes.csv'
$chapterCsvPath = Join-Path $dataDirectory 'P4Beta6_Chapters.csv'
$betaArtManifestPath = Join-Path $RepositoryRoot 'UE5/SourceAssets/P4/Manifests/P4_Beta6_ArtDirection_manifest_v02.json'
$alphaArenaManifestPath = Join-Path $RepositoryRoot 'UE5/SourceAssets/P4/Manifests/P4_A2_ArenaScene_manifest_v02.json'
$scenePreviewReportPath = Join-Path $RepositoryRoot 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_ScenePreview_build_report_v01.json'
$sceneReviewApprovalPath = Join-Path $RepositoryRoot 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_SceneReview_Approval_v01.json'
$environmentArtManifestPath = Join-Path $RepositoryRoot 'UE5/SourceAssets/P4/Environment/Beta6/P4_Beta6_EnvironmentArt_manifest_v01.json'
$environmentArtImportReportPath = Join-Path $RepositoryRoot 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_EnvironmentArt_import_report_v01.json'
$beta6DataTableImportReportPath = Join-Path $RepositoryRoot 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_DataTableImport.json'
$environmentReviewMapsReportPath = Join-Path $RepositoryRoot 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_EnvironmentReviewMaps_build_report_v01.json'
$outputCsvPath = Join-Path $dataDirectory 'P4Beta6_ThemeBindingAudit.csv'
$outputJsonPath = Join-Path $dataDirectory 'P4Beta6_ThemeBindingAudit.json'

foreach ($path in @($themeCsvPath, $chapterCsvPath, $betaArtManifestPath, $alphaArenaManifestPath, $environmentArtManifestPath, $environmentArtImportReportPath)) {
	if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required theme audit input was not found: $path" }
}

function Resolve-ProjectContentAssetPath([string]$GameAssetPath, [string]$Extension) {
	if (-not $GameAssetPath.StartsWith('/Game/')) { throw "Expected a /Game asset path, got: $GameAssetPath" }
	$relativePath = $GameAssetPath.Substring('/Game/'.Length).Replace('/', '\') + $Extension
	return Join-Path $RepositoryRoot (Join-Path 'UE5/NaiwaP0/Content' $relativePath)
}

$themes = @(Import-Csv -LiteralPath $themeCsvPath)
$chapters = @(Import-Csv -LiteralPath $chapterCsvPath)
$themeCsvSha256 = (Get-FileHash -LiteralPath $themeCsvPath -Algorithm SHA256).Hash
$betaArtManifest = Get-Content -LiteralPath $betaArtManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$alphaArenaManifest = Get-Content -LiteralPath $alphaArenaManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$environmentArtManifest = Get-Content -LiteralPath $environmentArtManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$environmentArtImportReport = Get-Content -LiteralPath $environmentArtImportReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($themes.Count -ne 6 -or $chapters.Count -ne 6) { throw 'Expected six chapter and six theme projection rows.' }
$nativeThemeDataTableImportReport = $null
$nativeThemeTextureReadbackById = @{}
$nativeThemeTextureReadbackStatus = 'pending_native_datatable_import'
if (Test-Path -LiteralPath $beta6DataTableImportReportPath -PathType Leaf) {
	$nativeThemeDataTableImportReport = Get-Content -LiteralPath $beta6DataTableImportReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
	if ($nativeThemeDataTableImportReport.status -eq 'failed') {
		$nativeThemeTextureReadbackStatus = 'native_datatable_import_failed'
	} elseif ($nativeThemeDataTableImportReport.status -eq 'passed') {
		$importedThemes = $nativeThemeDataTableImportReport.imported_tables.themes
		if (-not $importedThemes -or $importedThemes.csv_sha256 -ne $themeCsvSha256) {
			$nativeThemeTextureReadbackStatus = 'stale_native_datatable_import_report'
		} else {
			$readbackRows = $importedThemes.object_ref_readback
			if (-not $readbackRows -or $readbackRows.PSObject.Properties.Count -ne 6) {
				throw 'Successful Beta6 import report is missing one or more theme texture readback rows.'
			}
			foreach ($theme in $themes) {
				$sourceObjectPath = ([string]$theme.BackdropTexture).Replace("Texture2D'", '').TrimEnd("'")
				$expectedPackagePath = $sourceObjectPath.Substring(0, $sourceObjectPath.LastIndexOf('.'))
				$readbackProperty = $readbackRows.PSObject.Properties[[string]$theme.ThemeId]
				$readbackPackagePath = if ($readbackProperty) { [string]$readbackProperty.Value.BackdropTexture } else { '' }
				if ($readbackPackagePath -ne $expectedPackagePath) {
					throw "Native theme DataTable texture readback differs for $($theme.ThemeId): '$readbackPackagePath' != '$expectedPackagePath'."
				}
				$nativeThemeTextureReadbackById[[string]$theme.ThemeId] = $readbackPackagePath
			}
			$nativeThemeTextureReadbackStatus = 'six_native_theme_texture_references_read_back'
		}
	} else {
		$nativeThemeTextureReadbackStatus = 'invalid_native_datatable_import_report'
	}
}
if ($betaArtManifest.schema -ne 'naiwa.p4.beta6_art_direction_preview/2' -or
	$betaArtManifest.owner_direction.chapter_2_to_6 -ne 'Separate environments; no shared arena architecture or color-only variants') {
	throw 'Unexpected latest Beta6 scene direction manifest or owner direction.'
}
if ($environmentArtManifest.schema -ne 'naiwa.p4.beta6.environment_art_manifest/1' -or
	$environmentArtImportReport.schema -ne 'naiwa.p4.beta6.environment_art_import/1' -or
	$environmentArtImportReport.status -ne 'textures_imported_pending_engineering_theme_binding_and_readability_review' -or
	@($environmentArtManifest.assets).Count -ne 5 -or @($environmentArtImportReport.assets).Count -ne 5) {
	throw 'Beta6 production environment manifest/import report does not contain the expected five imported textures.'
}

$betaSceneAssets = @{}
foreach ($asset in $betaArtManifest.assets | Where-Object { $_.asset_id -match '^P4_BETA6_CH0[2-6]_SCENE$' }) {
	$betaSceneAssets[[string]$asset.chapter_id] = $asset
}
$environmentArtByChapter = @{}
foreach ($asset in $environmentArtManifest.assets) {
	$environmentArtByChapter[[string]$asset.chapter_id] = $asset
}
$environmentImportByChapter = @{}
foreach ($imported in $environmentArtImportReport.assets) {
	$chapterId = [string]$imported.chapter_id
	$asset = $environmentArtByChapter[$chapterId]
	if ($chapterId -notmatch '^chapter_[2-6]$' -or -not $asset -or
		$imported.texture_asset -ne $asset.ue_texture_asset -or
		$imported.source_sha256.ToUpperInvariant() -ne $asset.sha256.ToUpperInvariant() -or
		$imported.status -ne 'texture_imported_pending_engineering_theme_binding_and_readability_review') {
		throw "Beta6 production environment import row does not match its manifest entry: $chapterId"
	}
	$textureFile = Resolve-ProjectContentAssetPath ([string]$imported.texture_asset) '.uasset'
	$sourceFile = Join-Path $RepositoryRoot ([string]$asset.source_path.Replace('/', '\'))
	foreach ($file in @($textureFile, $sourceFile)) {
		if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Beta6 production environment input is missing: $file" }
	}
	$sourceHash = (Get-FileHash -LiteralPath $sourceFile -Algorithm SHA256).Hash
	if ($sourceHash -ne $asset.sha256.ToUpperInvariant()) { throw "Beta6 production environment source hash differs from manifest: $sourceFile" }
	$environmentImportByChapter[$chapterId] = $imported
}
$environmentReviewByChapter = @{}
$environmentReviewMapsReport = $null
if (Test-Path -LiteralPath $environmentReviewMapsReportPath -PathType Leaf) {
	$environmentReviewMapsReport = Get-Content -LiteralPath $environmentReviewMapsReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
	if ($environmentReviewMapsReport.schema -ne 'naiwa.p4.beta6.environment_review_maps/1' -or
		$environmentReviewMapsReport.status -ne 'review_maps_saved_pending_fixed_camera_readability_review' -or
		[int]$environmentReviewMapsReport.map_count -ne 5 -or $environmentReviewMapsReport.runtime_played) {
		throw 'Beta6 environment review-map report has an unexpected status or map count.'
	}
	foreach ($review in $environmentReviewMapsReport.items) {
		$chapterId = [string]$review.chapter_id
		if ($chapterId -notmatch '^chapter_[2-6]$' -or $review.runtime_bound -or
			$review.material_asset -notmatch '^/Game/' -or $review.review_map -notmatch '^/Game/') {
			throw "Beta6 environment review-map row is invalid for $chapterId."
		}
		$materialFile = Resolve-ProjectContentAssetPath ([string]$review.material_asset) '.uasset'
		$mapFile = Resolve-ProjectContentAssetPath ([string]$review.review_map) '.umap'
		foreach ($file in @($materialFile, $mapFile)) {
			if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Beta6 environment review asset is missing: $file" }
		}
		$environmentReviewByChapter[$chapterId] = $review
	}
}

$scenePreviewByChapter = @{}
$scenePreviewReport = $null
$sceneReviewApproval = $null
$sceneVisualReviewApproved = $false
if (Test-Path -LiteralPath $scenePreviewReportPath -PathType Leaf) {
	$scenePreviewReport = Get-Content -LiteralPath $scenePreviewReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
	if ($scenePreviewReport.schema -ne 'naiwa.p4.beta6_scene_preview_build/1' -or
		$scenePreviewReport.status -ne 'maps_saved_pending_visual_review' -or
		@($scenePreviewReport.maps).Count -ne 5 -or $scenePreviewReport.runtime_played) {
		throw 'Beta6 scene preview report is not the expected import-only five-map visual-review stage.'
	}
	foreach ($preview in $scenePreviewReport.maps) {
		$chapterId = [string]$preview.chapter_id
		if ($chapterId -notmatch '^chapter_[2-6]$' -or $preview.build_status -ne 'map_saved_pending_visual_review' -or
			$preview.texture_asset -notmatch '^/Game/' -or $preview.material_asset -notmatch '^/Game/' -or
			$preview.review_map -notmatch '^/Game/') {
			throw "Beta6 preview map row is invalid or claims an unexpected review state for $chapterId."
		}
		$textureFile = Resolve-ProjectContentAssetPath ([string]$preview.texture_asset) '.uasset'
		$materialFile = Resolve-ProjectContentAssetPath ([string]$preview.material_asset) '.uasset'
		$mapFile = Resolve-ProjectContentAssetPath ([string]$preview.review_map) '.umap'
		foreach ($assetFile in @($textureFile, $materialFile, $mapFile)) {
			if (-not (Test-Path -LiteralPath $assetFile -PathType Leaf)) { throw "Imported Beta6 preview asset is missing: $assetFile" }
		}
		$scenePreviewByChapter[$chapterId] = $preview
	}
}
if (Test-Path -LiteralPath $sceneReviewApprovalPath -PathType Leaf) {
	$sceneReviewApproval = Get-Content -LiteralPath $sceneReviewApprovalPath -Raw -Encoding UTF8 | ConvertFrom-Json
	if ($sceneReviewApproval.schema -ne 'naiwa.p4.beta6_scene_review_approval/1' -or
		$sceneReviewApproval.scope -ne 'chapter_2_to_6_scene_preview_review' -or
		$sceneReviewApproval.status -ne 'owner_visual_review_approved' -or
		$scenePreviewByChapter.Count -ne 5) {
		throw 'Beta6 owner visual review approval does not match the five imported Chapter 2-6 scene previews.'
	}
	$sceneVisualReviewApproved = $true
}

$rows = [System.Collections.Generic.List[object]]::new()
foreach ($chapter in ($chapters | Sort-Object { [int]$_.ChapterIndex })) {
	$chapterIndex = [int]$chapter.ChapterIndex
	$theme = $themes | Where-Object { $_.ThemeId -eq $chapter.ThemeId } | Select-Object -First 1
	if (-not $theme) { throw "Chapter $chapterIndex references missing theme $($chapter.ThemeId)." }

	if ($chapterIndex -eq 1) {
		$previewPath = [string]$alphaArenaManifest.source_png
		$previewHash = ([string]$alphaArenaManifest.source_sha256).ToUpperInvariant()
		$previewStatus = 'accepted_alpha_chapter_baseline'
		$runtimeReference = [string]$alphaArenaManifest.ue_texture_asset
		$runtimeStatus = 'accepted_alpha_runtime_asset_reference'
		$directionAssetId = [string]$alphaArenaManifest.asset_id
		$sceneDistinctnessStatus = 'frozen_alpha_baseline'
		$previewTextureAsset = ''
		$previewMaterialAsset = ''
		$previewMapAsset = ''
		$previewMapStatus = 'accepted_alpha_scene_baseline'
		$productionEnvironmentTextureAsset = ''
		$productionEnvironmentTextureStatus = 'accepted_alpha_scene_baseline'
		$expectedThemeBackdropTextureAsset = [string]$alphaArenaManifest.ue_texture_asset
		$environmentReviewMaterialAsset = ''
		$environmentReviewMapAsset = ''
		$environmentReviewMapStatus = 'accepted_alpha_scene_baseline'
	} else {
		$chapterId = "chapter_$chapterIndex"
		$asset = $betaSceneAssets[$chapterId]
		if (-not $asset) { throw "Latest art direction manifest has no individual scene concept for Chapter $chapterIndex." }
		$previewPath = [string]$asset.source_path
		$previewHash = ([string]$asset.sha256).ToUpperInvariant()
		$previewStatus = if ($sceneVisualReviewApproved) { 'owner_visual_review_approved' } else { [string]$asset.status }
		$runtimeReference = if ($sceneVisualReviewApproved) { 'Owner scene review approved; production environment texture imported; review material/map, prop audit, and runtime theme binding remain pending' } else { [string]$asset.runtime_reference }
		$previewMap = $scenePreviewByChapter[$chapterId]
		$previewTextureAsset = if ($previewMap) { [string]$previewMap.texture_asset } else { '' }
		$previewMaterialAsset = if ($previewMap) { [string]$previewMap.material_asset } else { '' }
		$previewMapAsset = if ($previewMap) { [string]$previewMap.review_map } else { '' }
		$previewMapStatus = if ($previewMap -and $sceneVisualReviewApproved) { 'map_saved_owner_visual_review_approved' } elseif ($previewMap) { [string]$previewMap.build_status } else { 'pending_scene_preview_import' }
		$runtimeStatus = if ($previewMap -and $sceneVisualReviewApproved) { 'preview_scene_visual_review_approved_pending_art_and_runtime_binding' } elseif ($previewMap) { 'preview_scene_imported_pending_visual_review' } else { 'pending_art_production_and_ue_preview' }
		$directionAssetId = [string]$asset.asset_id
		$sceneDistinctnessStatus = if ($previewMap -and $sceneVisualReviewApproved) { 'distinct_environment_preview_visual_review_approved' } elseif ($previewMap) { 'distinct_environment_preview_imported_visual_review_pending' } else { 'distinct_environment_concept_approved_art_tasks_pending' }
		$productionEnvironmentTextureAsset = [string]$environmentImportByChapter[$chapterId].texture_asset
		$productionEnvironmentTextureStatus = [string]$environmentImportByChapter[$chapterId].status
		$expectedThemeBackdropTextureAsset = $productionEnvironmentTextureAsset
		$environmentReview = $environmentReviewByChapter[$chapterId]
		$environmentReviewMaterialAsset = if ($environmentReview) { [string]$environmentReview.material_asset } else { '' }
		$environmentReviewMapAsset = if ($environmentReview) { [string]$environmentReview.review_map } else { '' }
		$environmentReviewMapStatus = if ($environmentReview) { [string]$environmentReview.build_status } else { 'pending_environment_review_map_generation' }
		$runtimeStatus = if ($environmentReview -and $sceneVisualReviewApproved) { 'environment_review_map_saved_pending_readability_and_runtime_binding' } else { 'production_environment_texture_imported_pending_review_map_theme_binding_and_readability_review' }
	}
	$expectedTextureObjectName = ($expectedThemeBackdropTextureAsset -split '/')[-1]
	$expectedThemeBackdropTextureReference = "Texture2D'$expectedThemeBackdropTextureAsset.$expectedTextureObjectName'"
	if ([string]$theme.BackdropTexture -ne $expectedThemeBackdropTextureReference) {
		throw "Chapter $chapterIndex theme CSV BackdropTexture does not match the approved/imported scene texture reference."
	}

	$resolvedPreviewPath = Join-Path $RepositoryRoot ($previewPath.Replace('/', '\'))
	if (-not (Test-Path -LiteralPath $resolvedPreviewPath -PathType Leaf)) { throw "Missing theme preview source: $previewPath" }
	$actualHash = (Get-FileHash -LiteralPath $resolvedPreviewPath -Algorithm SHA256).Hash
	if ($actualHash -ne $previewHash) { throw "Theme preview hash differs from manifest: $previewPath" }

	$rows.Add([ordered]@{
		RowName = ('chapter_{0:D2}' -f $chapterIndex)
		ChapterIndex = $chapterIndex
		ThemeId = [string]$chapter.ThemeId
		SceneBase = [string]$theme.SceneBase
		SceneTint = [string]$theme.SceneTint
		SceneLight = [string]$theme.SceneLight
		DesignPropIds = [string]$theme.DesignPropIds
		DirectionAssetId = $directionAssetId
		ConceptPreviewPath = $previewPath
		ConceptPreviewSha256 = $actualHash
		ConceptStatus = $previewStatus
		SceneDistinctnessStatus = $sceneDistinctnessStatus
		RuntimeSceneReference = $runtimeReference
		RuntimeBindingStatus = $runtimeStatus
		PreviewTextureAsset = $previewTextureAsset
		PreviewMaterialAsset = $previewMaterialAsset
		PreviewMapAsset = $previewMapAsset
		PreviewMapStatus = $previewMapStatus
		ProductionEnvironmentTextureAsset = $productionEnvironmentTextureAsset
		ProductionEnvironmentTextureStatus = $productionEnvironmentTextureStatus
		ThemeBackdropTextureAsset = $expectedThemeBackdropTextureAsset
		ThemeBackdropTextureStatus = if ($chapterIndex -eq 1) { 'accepted_alpha_reference_projected_pending_datatable_import' } else { 'production_texture_reference_projected_pending_datatable_import_review_and_runtime_gate' }
		NativeThemeTextureReadbackStatus = if ($nativeThemeTextureReadbackById.ContainsKey([string]$theme.ThemeId)) { 'read_back_from_native_datatable' } else { $nativeThemeTextureReadbackStatus }
		EnvironmentReviewMaterialAsset = $environmentReviewMaterialAsset
		EnvironmentReviewMapAsset = $environmentReviewMapAsset
		EnvironmentReviewMapStatus = $environmentReviewMapStatus
		PropBindingStatus = if ($chapterIndex -eq 1) { 'accepted_alpha_scene; no Beta prop changes' } else { 'design tokens only; source asset audit required' }
		bRuntimeThemeConfigured = ($chapterIndex -eq 1).ToString().ToLowerInvariant()
	})
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
	$columns = @($Rows[0].Keys)
	$lines = [System.Collections.Generic.List[string]]::new()
	$lines.Add((($columns | ForEach-Object { ConvertTo-CsvField $_ }) -join ','))
	foreach ($row in $Rows) {
		$fields = foreach ($column in $columns) { ConvertTo-CsvField $row[$column] }
		$lines.Add(($fields -join ','))
	}
	return (($lines -join "`r`n") + "`r`n")
}
function Get-Sha256([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }

$encoding = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($outputCsvPath, (ConvertTo-CsvContent $rows), $encoding)
$manifest = [ordered]@{
	artifact = 'P4 Beta6 Theme Binding Audit'
	auditVersion = '0.5'
	generatedAtLocalDate = (Get-Date).ToString('yyyy-MM-dd')
	sources = [ordered]@{
		chapterProjectionPath = 'UE5/SourceAssets/P4/Data/Beta6/P4Beta6_Chapters.csv'
		chapterProjectionSha256 = Get-Sha256 $chapterCsvPath
		themeProjectionPath = 'UE5/SourceAssets/P4/Data/Beta6/P4Beta6_Themes.csv'
		themeProjectionSha256 = Get-Sha256 $themeCsvPath
		betaArtDirectionPath = 'UE5/SourceAssets/P4/Manifests/P4_Beta6_ArtDirection_manifest_v02.json'
		betaArtDirectionSha256 = Get-Sha256 $betaArtManifestPath
		alphaArenaManifestPath = 'UE5/SourceAssets/P4/Manifests/P4_A2_ArenaScene_manifest_v02.json'
		alphaArenaManifestSha256 = Get-Sha256 $alphaArenaManifestPath
		scenePreviewReportPath = if ($scenePreviewReport) { 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_ScenePreview_build_report_v01.json' } else { '' }
		scenePreviewReportSha256 = if ($scenePreviewReport) { Get-Sha256 $scenePreviewReportPath } else { '' }
		sceneReviewApprovalPath = if ($sceneReviewApproval) { 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_SceneReview_Approval_v01.json' } else { '' }
		sceneReviewApprovalSha256 = if ($sceneReviewApproval) { Get-Sha256 $sceneReviewApprovalPath } else { '' }
		environmentArtManifestPath = 'UE5/SourceAssets/P4/Environment/Beta6/P4_Beta6_EnvironmentArt_manifest_v01.json'
		environmentArtManifestSha256 = Get-Sha256 $environmentArtManifestPath
		environmentArtImportReportPath = 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_EnvironmentArt_import_report_v01.json'
		environmentArtImportReportSha256 = Get-Sha256 $environmentArtImportReportPath
		environmentReviewMapsReportPath = if ($environmentReviewMapsReport) { 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_EnvironmentReviewMaps_build_report_v01.json' } else { '' }
		environmentReviewMapsReportSha256 = if ($environmentReviewMapsReport) { Get-Sha256 $environmentReviewMapsReportPath } else { '' }
		beta6DataTableImportReportPath = if ($nativeThemeDataTableImportReport) { 'UE5/Records/p4_alpha/evidence/Beta6/P4_Beta6_DataTableImport.json' } else { '' }
		beta6DataTableImportReportSha256 = if ($nativeThemeDataTableImportReport) { Get-Sha256 $beta6DataTableImportReportPath } else { '' }
	}
	beta6ThemeDataTableReadback = [ordered]@{
		status = $nativeThemeTextureReadbackStatus
		verifiedThemeCount = $nativeThemeTextureReadbackById.Count
		projectionCsvSha256 = $themeCsvSha256
		reportStatus = if ($nativeThemeDataTableImportReport) { [string]$nativeThemeDataTableImportReport.status } else { 'pending' }
	}
	rows = @($rows.ToArray())
	unresolved = @(
		$(if ($sceneVisualReviewApproved) { 'Chapter 2-6 scene review is owner-approved and five production environment textures are imported; production art completion, review materials/maps, prop audit, and readability review remain pending.' } else { 'Chapter 2-6 scene concepts are owner-approved and their preview textures, materials, and maps are imported; fixed-camera visual review and production cleanup remain pending.' }),
		'Backdrop texture soft references are present in the CSV projection and Beta6 test presenter path; the compiled DataTable import and readability approval are still pending, so Chapters 2-6 stay blocked by the runtime catalog.',
		'Chapter 2-6 environmental prop IDs are design tokens; source asset paths, ownership/reuse audit, and UE component bindings remain pending.',
		'Five production environment textures are verified as imported and projected into chapter theme rows; dedicated review materials/maps and DataTable import readback have not been recorded.',
		$(if ($sceneVisualReviewApproved) { 'The owner approval is recorded in P4_Beta6_SceneReview_Approval_v01.json; the requested 1280x720 Chapter 2 screenshot was not found and is not attached.' } else { 'No owner visual-review approval record is available yet.' }),
		'The v01 five-panel color study is superseded by the owner-directed v02 separate-environment concepts.'
	)
	output = [ordered]@{
		path = 'UE5/SourceAssets/P4/Data/Beta6/P4Beta6_ThemeBindingAudit.csv'
		rowCount = $rows.Count
		sha256 = Get-Sha256 $outputCsvPath
	}
}
[System.IO.File]::WriteAllText($outputJsonPath, (($manifest | ConvertTo-Json -Depth 20) + "`r`n"), $encoding)
Write-Output "Generated theme binding audit: rows=$($rows.Count); Beta scene concepts approved=$($rows.Count - 1); owner visual review=$(if ($sceneVisualReviewApproved) { 'approved' } else { 'pending' }); production textures=$($environmentImportByChapter.Count); theme texture refs=$($rows.Count); native readbacks=$($nativeThemeTextureReadbackById.Count); review maps=$($environmentReviewByChapter.Count); runtime-ready themes=1"
Write-Output "Imported Beta6 preview maps=$($scenePreviewByChapter.Count); runtime-ready themes remain limited to the accepted Alpha baseline."
Write-Output "Theme binding audit CSV SHA256=$(Get-Sha256 $outputCsvPath)"
