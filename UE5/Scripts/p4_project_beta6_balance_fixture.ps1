param(
	[string]$RepositoryRoot = ''
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
	$RepositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
} else {
	$RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
}

$designDirectory = Join-Path $RepositoryRoot 'UE5/Records/p4_alpha/design_reference'
$designFiles = @(Get-ChildItem -LiteralPath $designDirectory -File -Filter '*.json' | Where-Object { $_.BaseName -match '^P4_.+_v1$' })
if ($designFiles.Count -ne 1) { throw "Expected exactly one P4 versioned JSON design source; found $($designFiles.Count)." }
$designPath = $designFiles[0].FullName
$alphaEnemyPath = Join-Path $RepositoryRoot 'UE5/SourceAssets/P4/Data/P4EnemyAlpha.csv'
$alphaWavePath = Join-Path $RepositoryRoot 'UE5/SourceAssets/P4/Data/P4WaveAlpha.csv'
$outputDirectory = Join-Path $RepositoryRoot 'UE5/SourceAssets/P4/Data/Beta6'
$csvPath = Join-Path $outputDirectory 'P4Beta6_BalanceFixture.csv'
$waveCsvPath = Join-Path $outputDirectory 'P4Beta6_BalanceFixture_Waves.csv'
$jsonPath = Join-Path $outputDirectory 'P4Beta6_BalanceFixture.json'

$design = Get-Content -LiteralPath $designPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($design.schema -ne 'naiwa.p4.arena.chapters/2' -or $design.version -ne '2.0') {
	throw "Unexpected source schema/version: $($design.schema) / $($design.version)"
}
$alphaEnemies = @(Import-Csv -LiteralPath $alphaEnemyPath)
$alphaWaves = @(Import-Csv -LiteralPath $alphaWavePath)
$alphaById = @{}
foreach ($enemy in $alphaEnemies) { $alphaById[[string]$enemy.RowName] = $enemy }
$chapters = @{}
foreach ($chapter in $design.chapters) { $chapters[[int]$chapter.chapter] = $chapter }

$chapterRows = [System.Collections.Generic.List[object]]::new()
$waveRows = [System.Collections.Generic.List[object]]::new()
$cumulativeOrdinaryKills = 0
$playerLevel = 1
$playerExperience = 0
$experienceForNextLevel = 4
$cumulativeLevelUps = 0
$baseProjectileDps = 12.0 / 0.59

for ($chapterIndex = 1; $chapterIndex -le 6; $chapterIndex++) {
	$chapter = $chapters[$chapterIndex]
	if (-not $chapter) { throw "Chapter $chapterIndex is missing from source JSON." }
	$bossId = if ($null -eq $chapter.boss_id) { '' } else { [string]$chapter.boss_id }
	$bossProperty = if ($bossId) { $design.bosses.PSObject.Properties[$bossId] } else { $null }
	$bossHp = if ($bossProperty) { [int]$bossProperty.Value.hp } else { 0 }
	$targetSeconds = [int]$chapter.duration_target_s
	$ordinaryCount = 0
	$ordinaryHp = 0
	$incomingDamage = 0
	$chapterLevelUps = 0

	if ($chapterIndex -eq 1) {
		foreach ($group in ($alphaWaves | Group-Object { [int]$_.WaveIndex } | Sort-Object { [int]$_.Name })) {
			$waveIndex = [int]$group.Name
			$waveCount = 0
			$waveHp = 0
			$waveDamage = 0
			foreach ($spawn in $group.Group) {
				$enemyId = [string]$spawn.EnemyTypeId
				$count = [int]$spawn.Count
				$enemy = $alphaById[$enemyId]
				if (-not $enemy) { throw "Accepted Alpha wave references an enemy without an Alpha row: $enemyId" }
				$designEnemy = $design.enemies.PSObject.Properties[$enemyId].Value
				if ([int]$enemy.MaxHP -ne [int]$designEnemy.hp -or [int]$enemy.Damage -ne [int]$designEnemy.damage) {
					throw "Frozen Alpha combat stats disagree with source enemy design for $enemyId."
				}
				$waveCount += $count
				$waveHp += $count * [int]$enemy.MaxHP
				$waveDamage += $count * [int]$enemy.Damage
			}
			$ordinaryCount += $waveCount
			$ordinaryHp += $waveHp
			$incomingDamage += $waveDamage
			$waveRows.Add([ordered]@{
				RowName = ('ch{0:D2}_w{1:D2}' -f $chapterIndex, $waveIndex)
				ChapterIndex = $chapterIndex
				WaveIndex = $waveIndex
				OrdinaryEnemyCount = $waveCount
				OrdinaryHpBudget = $waveHp
				NominalIncomingDamageBudget = $waveDamage
				SourceKind = 'accepted_alpha_wave_and_enemy_csv'
			})
		}
	} else {
		foreach ($sourceWave in $chapter.waves) {
			$waveIndex = [int]$sourceWave.wave
			$waveCount = 0
			$waveHp = 0
			$waveDamage = 0
			foreach ($spawn in $sourceWave.spawns.PSObject.Properties) {
				$enemyId = [string]$spawn.Name
				$count = [int]$spawn.Value
				$enemyProperty = $design.enemies.PSObject.Properties[$enemyId]
				if (-not $enemyProperty) { throw "Chapter $chapterIndex references undefined enemy $enemyId." }
				$enemy = $enemyProperty.Value
				$waveCount += $count
				$waveHp += $count * [int]$enemy.hp
				$waveDamage += $count * [int]$enemy.damage
			}
			$ordinaryCount += $waveCount
			$ordinaryHp += $waveHp
			$incomingDamage += $waveDamage
			$waveRows.Add([ordered]@{
				RowName = ('ch{0:D2}_w{1:D2}' -f $chapterIndex, $waveIndex)
				ChapterIndex = $chapterIndex
				WaveIndex = $waveIndex
				OrdinaryEnemyCount = $waveCount
				OrdinaryHpBudget = $waveHp
				NominalIncomingDamageBudget = $waveDamage
				SourceKind = 'chapter_json_v2'
			})
		}
	}

	$expectedCount = switch ($chapterIndex) { 1 { 20 } 2 { 30 } 3 { 42 } 4 { 9 } 5 { 56 } 6 { 12 } }
	if ($ordinaryCount -ne $expectedCount) { throw "Chapter $chapterIndex projects $ordinaryCount enemies; expected $expectedCount." }
	$cumulativeOrdinaryKills += $ordinaryCount
	while ($ordinaryCount -gt 0) {
		# The current deterministic P4 runtime awards one XP per ordinary kill.
		$ordinaryCount--
		$playerExperience++
		while ($playerExperience -ge $experienceForNextLevel) {
			$playerExperience -= $experienceForNextLevel
			$playerLevel++
			$chapterLevelUps++
			$cumulativeLevelUps++
			$experienceForNextLevel = 4 + [Math]::Max(0, $playerLevel - 1) * 2
		}
	}
	$ordinaryCount = $expectedCount
	$combinedHp = $ordinaryHp + $bossHp
	$chapterRows.Add([ordered]@{
		RowName = ('chapter_{0:D2}' -f $chapterIndex)
		ChapterIndex = $chapterIndex
		WaveCount = [int]$chapter.waves.Count
		OrdinaryEnemyCount = $ordinaryCount
		OrdinaryHpBudget = $ordinaryHp
		BossId = $bossId
		BossHp = $bossHp
		CombinedHealthBudget = $combinedHp
		DurationTargetSeconds = $targetSeconds
		PerfectHitBaseProjectileDps = [Math]::Round($baseProjectileDps, 3)
		PerfectHitTtkLowerBoundSeconds = [Math]::Round($combinedHp / $baseProjectileDps, 2)
		TargetSustainedDpsLowerBound = [Math]::Round($combinedHp / $targetSeconds, 2)
		NominalIncomingDamageBudget = $incomingDamage
		OrdinaryKillExperience = $ordinaryCount
		CurrentRuntimeLevelUpsThisChapter = $chapterLevelUps
		CumulativeRuntimeLevelUps = $cumulativeLevelUps
		RuntimePlayerLevelAtChapterEnd = $playerLevel
		RuntimeExperienceTowardNextLevel = $playerExperience
		SourceKind = if ($chapterIndex -eq 1) { 'accepted_alpha_wave_csv_plus_source_enemy_stats' } else { 'chapter_json_v2' }
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
		$lines.Add(((@($columns | ForEach-Object { ConvertTo-CsvField $row[$_] })) -join ','))
	}
	return (($lines -join "`r`n") + "`r`n")
}
function Get-Sha256([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$encoding = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($csvPath, (ConvertTo-CsvContent $chapterRows), $encoding)
$jsonManifest = [ordered]@{
	artifact = 'P4 Beta6 Chapter Balance Fixture'
	fixtureVersion = '0.1'
	generatedAtLocalDate = (Get-Date).ToString('yyyy-MM-dd')
	source = [ordered]@{
		chapterDataPath = $designPath.Substring($RepositoryRoot.Length).TrimStart('\').Replace('\', '/')
		chapterDataSchema = [string]$design.schema
		chapterDataVersion = [string]$design.version
		chapterDataSha256 = Get-Sha256 $designPath
		acceptedAlphaEnemyPath = 'UE5/SourceAssets/P4/Data/P4EnemyAlpha.csv'
		acceptedAlphaEnemySha256 = Get-Sha256 $alphaEnemyPath
		acceptedAlphaWavePath = 'UE5/SourceAssets/P4/Data/P4WaveAlpha.csv'
		acceptedAlphaWaveSha256 = Get-Sha256 $alphaWavePath
		combatTuningPath = 'UE5/NaiwaP0/Source/NaiwaRuntime/P4/Core/NaiwaP4ArenaSubsystem.cpp'
		combatTuningScope = 'Current frozen runtime reference: 12 damage per projectile, 0.59 second fire interval, one XP per ordinary kill, and next-level thresholds increasing by 2 XP per level (4, 6, 8, ...).'
	}
	method = [ordered]@{
		perfectHitBaseProjectileDps = [Math]::Round($baseProjectileDps, 3)
		perfectHitTtkLowerBound = 'Combined ordinary-enemy and Boss HP divided by base projectile DPS; assumes every shot hits, continuous target availability, and zero movement/attack downtime.'
		targetSustainedDpsLowerBound = 'Combined health budget divided by chapter target duration; a lower bound, not a predicted playtest DPS.'
		nominalIncomingDamageBudget = 'Sum of enemy damage multiplied by spawn count; does not model attack frequency, behavior, block, misses, or invulnerability.'
		xpAndLevelCurve = 'Existing P4 runtime awards one XP per ordinary kill; Boss XP is excluded because no Beta rule is frozen.'
	}
	scope = [ordered]@{
		chapterRows = @($chapterRows.ToArray())
		waveRows = @($waveRows.ToArray())
		totalWaves = $waveRows.Count
		totalOrdinaryEnemies = 0
		totalOrdinaryHpBudget = 0
		totalBossHp = 0
		cumulativeOrdinaryKillExperience = $cumulativeOrdinaryKills
	}
	unresolved = @(
		'Chapter 2-6 spawn intervals are not present in the source JSON.',
		'Ten Beta enemies have no accepted runtime movement/range/cooldown/collision tuning.',
		'Boss per-attack timings, hitboxes, damage, summons and transition details are not frozen.',
		'Boss XP, upgrade offer count, retry XP reset behavior and cross-chapter run metrics need Beta progression freeze.',
		'Perfect-hit DPS/TTK values are analytical lower bounds and do not replace deterministic simulation or PIE/package measurements.'
	)
	outputs = @(
		[ordered]@{ path = 'UE5/SourceAssets/P4/Data/Beta6/P4Beta6_BalanceFixture.csv'; rowCount = $chapterRows.Count; sha256 = Get-Sha256 $csvPath },
		[ordered]@{ path = 'UE5/SourceAssets/P4/Data/Beta6/P4Beta6_BalanceFixture_Waves.csv'; rowCount = $waveRows.Count; sha256 = '' }
	)
}
$waveCsvContent = ConvertTo-CsvContent $waveRows
[System.IO.File]::WriteAllText($waveCsvPath, $waveCsvContent, $encoding)
$jsonManifest.outputs[1].sha256 = Get-Sha256 $waveCsvPath
$totalOrdinaryEnemyCount = 0
$totalOrdinaryHpBudget = 0
$totalBossHp = 0
foreach ($row in $chapterRows) {
	$totalOrdinaryEnemyCount += [int]$row['OrdinaryEnemyCount']
	$totalOrdinaryHpBudget += [int]$row['OrdinaryHpBudget']
	$totalBossHp += [int]$row['BossHp']
}
$jsonManifest.scope.totalOrdinaryEnemies = $totalOrdinaryEnemyCount
$jsonManifest.scope.totalOrdinaryHpBudget = $totalOrdinaryHpBudget
$jsonManifest.scope.totalBossHp = $totalBossHp
$jsonManifest.scope.cumulativeOrdinaryKillExperience = $totalOrdinaryEnemyCount
[System.IO.File]::WriteAllText($jsonPath, (($jsonManifest | ConvertTo-Json -Depth 24) + "`r`n"), $encoding)
Write-Output "Generated Beta6 balance fixture: chapters=$($chapterRows.Count); waves=$($waveRows.Count); ordinary enemies=$cumulativeOrdinaryKills"
Write-Output "Fixture CSV SHA256=$(Get-Sha256 $csvPath)"
Write-Output "Combined ordinary HP=$($jsonManifest.scope.totalOrdinaryHpBudget); Boss HP=$($jsonManifest.scope.totalBossHp)"
