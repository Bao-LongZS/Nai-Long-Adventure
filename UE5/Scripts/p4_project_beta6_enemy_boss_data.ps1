param(
    [string]$RepositoryRoot = ''
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
} else {
    $RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
}

$designReferenceDirectory = Join-Path $RepositoryRoot 'UE5/Records/p4_alpha/design_reference'
$chapterJsonFiles = @(Get-ChildItem -LiteralPath $designReferenceDirectory -File -Filter '*.json' | Where-Object { $_.BaseName -match '^P4_.+_v1$' })
if ($chapterJsonFiles.Count -ne 1) {
    throw "Expected exactly one P4 versioned JSON design source; found $($chapterJsonFiles.Count)."
}
$sourceJsonPath = $chapterJsonFiles[0].FullName
$sourceJsonRelative = $sourceJsonPath.Substring($RepositoryRoot.Length).TrimStart('\').Replace('\', '/')
$alphaEnemyPath = Join-Path $RepositoryRoot 'UE5/SourceAssets/P4/Data/P4EnemyAlpha.csv'
$alphaWavePath = Join-Path $RepositoryRoot 'UE5/SourceAssets/P4/Data/P4WaveAlpha.csv'
$runtimeTuningPath = Join-Path $RepositoryRoot 'UE5/SourceAssets/P4/Data/Beta6/P4Beta6_RuntimeTuning_v01.json'
$outputDirectory = Join-Path $RepositoryRoot 'UE5/SourceAssets/P4/Data/Beta6'
$enemyCsvPath = Join-Path $outputDirectory 'P4Beta6_Enemies.csv'
$bossCsvPath = Join-Path $outputDirectory 'P4Beta6_Bosses.csv'
$bossAttackCsvPath = Join-Path $outputDirectory 'P4Beta6_BossAttacks.csv'
$manifestPath = Join-Path $outputDirectory 'P4Beta6_ContentProjection.json'

$designData = Get-Content -LiteralPath $sourceJsonPath -Raw -Encoding UTF8 | ConvertFrom-Json
$runtimeTuning = Get-Content -LiteralPath $runtimeTuningPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($designData.schema -ne 'naiwa.p4.arena.chapters/2' -or $designData.version -ne '2.0') {
    throw "Unexpected source schema/version: $($designData.schema) / $($designData.version)"
}
if ($runtimeTuning.schema -ne 'naiwa.p4.beta6.runtime_tuning/1' -or
    $runtimeTuning.status -ne 'engineering_baseline_for_beta6_runtime') {
    throw 'Beta6 runtime tuning source has an unexpected schema or status.'
}
$alphaRows = @(Import-Csv -LiteralPath $alphaEnemyPath)
$alphaWaveRows = @(Import-Csv -LiteralPath $alphaWavePath)
$alphaById = @{}
foreach ($row in $alphaRows) { $alphaById[[string]$row.RowName] = $row }

$enemyIds = @('naiding','naizhou','naiping_lei','naitiao','naidun','nailing','naichui','naixiang','naishuang','chacha','haohao','dandan')
$bossIds = @('boss_nai_tong','boss_nai_jia')
$chapterEnemyIds = @(
    $designData.chapters | Where-Object { $_.chapter -ge 2 -and $_.chapter -le 6 } |
        ForEach-Object { $_.waves | ForEach-Object { $_.spawns.PSObject.Properties.Name } } |
        Sort-Object -Unique
)
$effectiveDebutById = @{}
foreach ($row in $alphaWaveRows) { $effectiveDebutById[[string]$row.EnemyTypeId] = 1 }
foreach ($chapter in ($designData.chapters | Where-Object { $_.chapter -ge 2 -and $_.chapter -le 6 } | Sort-Object { [int]$_.chapter })) {
    foreach ($wave in $chapter.waves) {
        foreach ($spawnId in $wave.spawns.PSObject.Properties.Name) {
            if (-not $effectiveDebutById.ContainsKey([string]$spawnId)) {
                $effectiveDebutById[[string]$spawnId] = [int]$chapter.chapter
            }
        }
    }
}
$missingEnemyIds = @($chapterEnemyIds | Where-Object { $_ -notin $enemyIds })
if ($missingEnemyIds.Count -gt 0) { throw "Chapter 2-6 source references IDs outside the handoff enemy list: $($missingEnemyIds -join ', ')" }

$enemyRows = [System.Collections.Generic.List[object]]::new()
foreach ($enemyId in $enemyIds) {
    $enemyProperty = $designData.enemies.PSObject.Properties[$enemyId]
    if (-not $enemyProperty) { throw "Missing enemy definition in source JSON: $enemyId" }
    $sourceEnemy = $enemyProperty.Value
    $alpha = $alphaById[$enemyId]
    $hasAlphaRuntime = $null -ne $alpha
    $runtime = $runtimeTuning.enemies.PSObject.Properties[$enemyId].Value
    if (-not $runtime) { throw "Missing Beta6 runtime tuning for enemy $enemyId" }
    if ($hasAlphaRuntime) {
        if ([int]$sourceEnemy.hp -ne [int]$alpha.MaxHP -or
            [int]$sourceEnemy.damage -ne [int]$alpha.Damage -or
            [double]$sourceEnemy.telegraph_s -ne [double]$alpha.TelegraphSeconds) {
            throw "Frozen Alpha stats disagree with source design fields for $enemyId; reconcile before projection."
        }
    }

    $enemyRows.Add([ordered]@{
        RowName = $enemyId
        EnemyId = $enemyId
        DisplayName = [string]$sourceEnemy.name
        MaxHP = [int]$sourceEnemy.hp
        Damage = [int]$sourceEnemy.damage
        TelegraphSeconds = [string]$sourceEnemy.telegraph_s
        BehaviorId = [string]$sourceEnemy.behavior
        ThreatLevel = [int]$sourceEnemy.threat
        DesignDebutChapter = [int]$sourceEnemy.debut_chapter
        DebutChapter = [int]$effectiveDebutById[$enemyId]
        DebutSource = if ($alphaWaveRows.EnemyTypeId -contains $enemyId) { 'accepted_alpha_wave_csv' } else { 'chapter_json_v2' }
        DesignSource = [string]$sourceEnemy.source
        SilhouetteNotes = [string]$sourceEnemy.silhouette
        MoveSpeed = if ($hasAlphaRuntime) { [string]$alpha.MoveSpeed } else { [string]$runtime.moveSpeed }
        AttackRange = if ($hasAlphaRuntime) { [string]$alpha.AttackRange } else { [string]$runtime.attackRange }
        AttackIntervalSeconds = if ($hasAlphaRuntime) { [string]$alpha.AttackIntervalSeconds } else { [string]$runtime.attackIntervalSeconds }
        CollisionRadius = if ($hasAlphaRuntime) { [string]$alpha.CollisionRadius } else { [string]$runtime.collisionRadius }
        bFrontShield75 = if ($hasAlphaRuntime) { ([string]$alpha.bFrontShield75).ToLowerInvariant() } else { 'false' }
        AbilityRadius = [string]$runtime.abilityRadius
        AbilityDurationSeconds = [string]$runtime.abilityDurationSeconds
        ProjectileSpeed = [string]$runtime.projectileSpeed
        SlowMultiplier = [string]$runtime.slowMultiplier
        SlowDurationSeconds = [string]$runtime.slowDurationSeconds
        SummonEnemyId = [string]$runtime.summonEnemyId
        SummonCount = [string]$runtime.summonCount
        bRuntimeTuningConfigured = 'true'
        RuntimeTuningStatus = if ($hasAlphaRuntime) { 'accepted_alpha_runtime_baseline' } else { 'engineering_runtime_tuning_v1' }
    })
}

$bossRows = [System.Collections.Generic.List[object]]::new()
$bossAttackRows = [System.Collections.Generic.List[object]]::new()
foreach ($bossId in $bossIds) {
    $bossProperty = $designData.bosses.PSObject.Properties[$bossId]
    if (-not $bossProperty) { throw "Missing Boss definition in source JSON: $bossId" }
    $boss = $bossProperty.Value
    $attackIds = @($boss.attacks | ForEach-Object { [string]$_ })
    $breakWindowMatch = [regex]::Match([string]$boss.break_window, '([0-9]+(?:\.[0-9]+)?)s')
    $damageModifierMatch = [regex]::Match([string]$boss.break_window, 'x\s*([0-9]+(?:\.[0-9]+)?)')
    if (-not $breakWindowMatch.Success -or -not $damageModifierMatch.Success) {
        throw "Could not parse the frozen break/vulnerability values for ${bossId}: $($boss.break_window)"
    }
    $breakWindowSeconds = [double]$breakWindowMatch.Groups[1].Value
    $sourceVulnerabilityMultiplier = [double]$damageModifierMatch.Groups[1].Value
    $breakDamageMultiplier = if ($attackIds -contains 'charge_wall_stun') { $sourceVulnerabilityMultiplier } else { 1.0 }
    $backDamageMultiplier = if ($attackIds -contains 'shield_wall_advance') { $sourceVulnerabilityMultiplier } else { 1.0 }
    $attackOrder = 0
    $bossRuntime = $runtimeTuning.bosses.PSObject.Properties[$bossId].Value
    if (-not $bossRuntime) { throw "Missing Beta6 runtime tuning for Boss $bossId" }
    foreach ($attackId in $attackIds) {
        $attackOrder++
        $attackRuntimeKey = '{0}_{1}' -f $bossId, $attackId
        $attackRuntime = $runtimeTuning.bossAttacks.PSObject.Properties[$attackRuntimeKey].Value
        if (-not $attackRuntime) { throw "Missing Beta6 runtime tuning for Boss attack $attackRuntimeKey" }
        $designActionKind = switch -Regex ($attackId) {
            '^charge' { 'charge'; break }
            'slam|slash' { 'melee_strike'; break }
            'ground_wave' { 'ground_projectile'; break }
            'shield_wall' { 'advancing_shield'; break }
            'spin' { 'rotating_area'; break }
            'summon' { 'summon'; break }
            default { 'requires_action_kind_review' }
        }
        $bossAttackRows.Add([ordered]@{
            RowName = ('{0}_{1}' -f $bossId, $attackId)
            BossId = $bossId
            AttackId = $attackId
            AttackOrder = $attackOrder
            PhaseId = if ($attackId -like 'phase2_*') { 'phase2' } else { 'phase1' }
            DesignActionKind = $designActionKind
            TelegraphSecondsMinimum = [string]$attackRuntime.telegraphSecondsMinimum
            Damage = [string]$attackRuntime.damage
            ActiveDurationSeconds = [string]$attackRuntime.activeDurationSeconds
            RecoverySeconds = [string]$attackRuntime.recoverySeconds
            HitboxGeometry = [string]$attackRuntime.hitboxGeometry
            HitboxExtentX = [string]$attackRuntime.hitboxExtentX
            HitboxExtentY = [string]$attackRuntime.hitboxExtentY
            HitCount = [string]$attackRuntime.hitCount
            HitIntervalSeconds = [string]$attackRuntime.hitIntervalSeconds
            DamageHit2 = [string]$attackRuntime.damageHit2
            DamageHit3 = [string]$attackRuntime.damageHit3
            PhaseTwoHitCount = [string]$attackRuntime.phaseTwoHitCount
            ProjectileSpeed = [string]$attackRuntime.projectileSpeed
            MoveSpeed = [string]$attackRuntime.moveSpeed
            ContactDamageIntervalSeconds = [string]$attackRuntime.contactDamageIntervalSeconds
            BreakWindowSeconds = if ($attackId -in @('charge_wall_stun','shield_wall_advance')) { [string]$breakWindowSeconds } else { '0' }
            BreakDamageMultiplier = if ($attackId -eq 'charge_wall_stun') { [string]$breakDamageMultiplier } else { '1' }
            BackDamageMultiplier = if ($attackId -eq 'shield_wall_advance') { [string]$backDamageMultiplier } else { '1' }
            bBreakWindowConfigured = ([string]($attackId -in @('charge_wall_stun','shield_wall_advance'))).ToLowerInvariant()
            bSummonsEnemies = ([string]($attackId -match 'summon')).ToLowerInvariant()
            SummonEnemyIds = if (@($attackRuntime.summonEnemyIds).Count -gt 0) { '(' + (@($attackRuntime.summonEnemyIds) -join ',') + ')' } else { '' }
            SummonCount = [string]$attackRuntime.summonCount
            bRuntimeTuningConfigured = 'true'
            RuntimeTuningStatus = 'engineering_runtime_tuning_v1'
        })
    }
    $bossRows.Add([ordered]@{
        RowName = $bossId
        BossId = $bossId
        DisplayName = [string]$boss.name
        ChapterIndex = [int]$boss.chapter
        MaxHP = [int]$boss.hp
        PhaseTwoAt = [string]$boss.phase2_at
        TelegraphSecondsMinimum = [string]$boss.telegraph_s_min
        AttackIds = ('(' + ($attackIds -join ',') + ')')
        BreakWindowDescription = [string]$boss.break_window
        BreakWindowSeconds = [string]$breakWindowSeconds
        BreakDamageMultiplier = [string]$breakDamageMultiplier
        BackDamageMultiplier = [string]$backDamageMultiplier
        IntroLine = [string]$boss.lines.intro
        PhaseTwoLine = [string]$boss.lines.phase2
        DeathLine = [string]$boss.lines.death
        EventLine = [string]$boss.lines.event
        GagDescription = [string]$boss.gag
        MoveSpeed = [string]$bossRuntime.moveSpeed
        AttackRange = [string]$bossRuntime.attackRange
        CollisionRadius = [string]$bossRuntime.collisionRadius
        PhaseTwoTransitionSeconds = [string]$bossRuntime.phaseTwoTransitionSeconds
        PhaseTwoDamageMultiplier = [string]$bossRuntime.phaseTwoDamageMultiplier
        PhaseTwoAttackIntervalMultiplier = [string]$bossRuntime.phaseTwoAttackIntervalMultiplier
        PhaseTwoGagDamage = [string]$bossRuntime.phaseTwoGagDamage
        PhaseTwoGagRadius = [string]$bossRuntime.phaseTwoGagRadius
        PhaseTwoGagStunSeconds = [string]$bossRuntime.phaseTwoGagStunSeconds
        bRuntimeAttackTuningConfigured = 'true'
        RuntimeAttackTuningStatus = 'engineering_runtime_tuning_v1'
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

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$encoding = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($enemyCsvPath, (ConvertTo-CsvContent $enemyRows), $encoding)
[System.IO.File]::WriteAllText($bossCsvPath, (ConvertTo-CsvContent $bossRows), $encoding)
[System.IO.File]::WriteAllText($bossAttackCsvPath, (ConvertTo-CsvContent $bossAttackRows), $encoding)

$unconfiguredEnemyIds = @($enemyRows | Where-Object { $_.RuntimeTuningStatus -eq 'missing_runtime_tuning' } | ForEach-Object { $_.EnemyId })
$missingBossAttackDetails = @()
$manifest = [ordered]@{
    artifact = 'P4 Beta6 Enemy and Boss Data Projection'
    projectionVersion = '0.1'
    generatedAtLocalDate = (Get-Date).ToString('yyyy-MM-dd')
    source = [ordered]@{
        chapterDataPath = $sourceJsonRelative
        chapterDataSchema = [string]$designData.schema
        chapterDataVersion = [string]$designData.version
        chapterDataSha256 = Get-Sha256 $sourceJsonPath
        runtimeTuningPath = 'UE5/SourceAssets/P4/Data/Beta6/P4Beta6_RuntimeTuning_v01.json'
        runtimeTuningVersion = [string]$runtimeTuning.version
        runtimeTuningSha256 = Get-Sha256 $runtimeTuningPath
        acceptedAlphaEnemyPath = 'UE5/SourceAssets/P4/Data/P4EnemyAlpha.csv'
        acceptedAlphaEnemySha256 = Get-Sha256 $alphaEnemyPath
        acceptedAlphaWavePath = 'UE5/SourceAssets/P4/Data/P4WaveAlpha.csv'
        acceptedAlphaWaveSha256 = Get-Sha256 $alphaWavePath
    }
    scope = [ordered]@{
        enemyIds = $enemyIds
        bossIds = $bossIds
        enemyRows = @($enemyRows.ToArray())
        bossRows = @($bossRows.ToArray())
        bossAttackRows = @($bossAttackRows.ToArray())
        alphaRuntimeRowsPreserved = @('naiding','naidun')
        debutChapterOverrides = @($enemyRows | Where-Object { $_.DesignDebutChapter -ne $_.DebutChapter } | ForEach-Object {
            [ordered]@{ enemyId = $_.EnemyId; designDebutChapter = $_.DesignDebutChapter; effectiveDebutChapter = $_.DebutChapter; source = $_.DebutSource }
        })
    }
    outputs = @(
        [ordered]@{ path = 'UE5/SourceAssets/P4/Data/Beta6/P4Beta6_Enemies.csv'; rowCount = $enemyRows.Count; sha256 = Get-Sha256 $enemyCsvPath; plannedDataTable = '/Game/Naiwa/P4/Data/DT_P4_Beta6_Enemies' },
        [ordered]@{ path = 'UE5/SourceAssets/P4/Data/Beta6/P4Beta6_Bosses.csv'; rowCount = $bossRows.Count; sha256 = Get-Sha256 $bossCsvPath; plannedDataTable = '/Game/Naiwa/P4/Data/DT_P4_Beta6_Bosses' },
        [ordered]@{ path = 'UE5/SourceAssets/P4/Data/Beta6/P4Beta6_BossAttacks.csv'; rowCount = $bossAttackRows.Count; sha256 = Get-Sha256 $bossAttackCsvPath; plannedDataTable = '/Game/Naiwa/P4/Data/DT_P4_Beta6_BossAttacks' }
    )
    unresolvedSourceFields = @(
        [ordered]@{
            field = 'enemy runtime movement/attack tuning'
            affectedEnemyIds = $unconfiguredEnemyIds
            sourceStatus = if ($unconfiguredEnemyIds.Count -eq 0) { 'all rows explicitly configured in versioned Beta6 runtime tuning' } else { 'one or more rows are still unconfigured' }
            action = 'Retain the source tuning version/hash and recalibrate after deterministic runtime capture.'
        },
        [ordered]@{
            field = 'Boss per-attack runtime parameters'
            affectedBossIds = $bossIds
            missing = $missingBossAttackDetails
            sourceStatus = if ($missingBossAttackDetails.Count -eq 0) { 'all per-attack fields and phase values are explicitly configured in versioned Beta6 runtime tuning' } else { 'one or more attack fields remain unconfigured' }
            action = 'Retain the source tuning version/hash and recalibrate after deterministic runtime capture.'
        }
    )
    ueDataTableImport = [ordered]@{
        status = 'pending_native_row_struct_extension_and_import'
        rowCounts = $null
        note = 'Tuning values are an explicit engineering baseline derived from the accepted design contract; UE readback and runtime calibration remain outstanding.'
    }
}

$json = $manifest | ConvertTo-Json -Depth 30
[System.IO.File]::WriteAllText($manifestPath, ($json + "`r`n"), $encoding)
Write-Output "Generated Beta6 enemy/Boss projections: enemies=$($enemyRows.Count); bosses=$($bossRows.Count); boss attacks=$($bossAttackRows.Count)"
Write-Output "Unconfigured runtime enemy tuning IDs=$($unconfiguredEnemyIds -join ',')"
Write-Output "Enemy CSV SHA256=$(Get-Sha256 $enemyCsvPath)"
Write-Output "Boss CSV SHA256=$(Get-Sha256 $bossCsvPath)"
Write-Output "Boss attack CSV SHA256=$(Get-Sha256 $bossAttackCsvPath)"
