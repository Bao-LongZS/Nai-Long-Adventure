#include "NaiwaP4Beta6ChapterCatalog.h"

#include "Engine/DataTable.h"

namespace
{
	constexpr int32 Beta6ChapterCount = 6;
	constexpr int32 Beta6ThemeCount = 6;
	constexpr int32 Beta6WaveSpawnRowCount = 66;
	constexpr int32 Beta6EnemyCount = 12;
	constexpr int32 Beta6BossCount = 2;
	constexpr int32 Beta6BossAttackCount = 8;

	bool HasRowStruct(const UDataTable* Table, const UScriptStruct* ExpectedStruct)
	{
		return Table && Table->GetRowStruct() == ExpectedStruct;
	}

	void AddBlocker(FNaiwaP4Beta6ChapterBundle& Bundle, const FString& Blocker)
	{
		Bundle.RuntimeBlockers.AddUnique(Blocker);
	}

	bool IsArenaEnemyBehaviorImplemented(const FName BehaviorId)
	{
		return BehaviorId == FName(TEXT("chase"))
			|| BehaviorId == FName(TEXT("ranged_line"))
			|| BehaviorId == FName(TEXT("suicide_proximity"))
			|| BehaviorId == FName(TEXT("leap_slam"))
			|| BehaviorId == FName(TEXT("front_shield_75"))
			|| BehaviorId == FName(TEXT("bell_aoe_r130"))
			|| BehaviorId == FName(TEXT("slow_heavy_slam"))
			|| BehaviorId == FName(TEXT("disguise_ambush"))
			|| BehaviorId == FName(TEXT("slow_projectile_40pct_3s"))
			|| BehaviorId == FName(TEXT("patrol_then_call_reinforce"))
			|| BehaviorId == FName(TEXT("lob_projectile"))
			|| BehaviorId == FName(TEXT("rolling_fuse_bomb"));
	}

	bool HasValidEnemyBehaviorTuning(const FNaiwaP4Beta6EnemyRow& Enemy)
	{
		if (Enemy.MoveSpeed < 0.0f || Enemy.AttackRange <= 0.0f
			|| Enemy.AttackIntervalSeconds <= 0.0f || Enemy.CollisionRadius <= 0.0f
			|| Enemy.AbilityRadius < 0.0f || Enemy.AbilityDurationSeconds < 0.0f
			|| Enemy.ProjectileSpeed < 0.0f || Enemy.SlowDurationSeconds < 0.0f
			|| Enemy.SummonCount < 0)
		{
			return false;
		}
		const FName Behavior = Enemy.BehaviorId;
		if (Behavior == FName(TEXT("ranged_line")) || Behavior == FName(TEXT("slow_projectile_40pct_3s")))
		{
			return Enemy.ProjectileSpeed > 0.0f;
		}
		if (Behavior == FName(TEXT("suicide_proximity")) || Behavior == FName(TEXT("bell_aoe_r130"))
			|| Behavior == FName(TEXT("slow_heavy_slam")) || Behavior == FName(TEXT("disguise_ambush")))
		{
			return Enemy.AbilityRadius > 0.0f;
		}
		if (Behavior == FName(TEXT("leap_slam")) || Behavior == FName(TEXT("rolling_fuse_bomb")))
		{
			return Enemy.AbilityRadius > 0.0f && Enemy.AbilityDurationSeconds > 0.0f;
		}
		if (Behavior == FName(TEXT("patrol_then_call_reinforce")))
		{
			return !Enemy.SummonEnemyId.IsNone() && Enemy.SummonCount > 0;
		}
		if (Behavior == FName(TEXT("lob_projectile")))
		{
			return Enemy.ProjectileSpeed > 0.0f && Enemy.AbilityRadius > 0.0f;
		}
		if (Behavior == FName(TEXT("chase")) || Behavior == FName(TEXT("front_shield_75")))
		{
			return true;
		}
		return false;
	}
}

bool FNaiwaP4Beta6ChapterCatalog::Load(const UDataTable* ChapterTable,
	const UDataTable* ThemeTable, const UDataTable* WaveSpawnTable,
	const UDataTable* EnemyTable, const UDataTable* BossTable,
	const UDataTable* BossAttackTable, FString& OutError)
{
	if (!HasRowStruct(ChapterTable, FNaiwaP4Beta6ChapterRow::StaticStruct())
		|| !HasRowStruct(ThemeTable, FNaiwaP4Beta6ThemeRow::StaticStruct())
		|| !HasRowStruct(WaveSpawnTable, FNaiwaP4Beta6WaveSpawnRow::StaticStruct())
		|| !HasRowStruct(EnemyTable, FNaiwaP4Beta6EnemyRow::StaticStruct())
		|| !HasRowStruct(BossTable, FNaiwaP4Beta6BossRow::StaticStruct())
		|| !HasRowStruct(BossAttackTable, FNaiwaP4Beta6BossAttackRow::StaticStruct()))
	{
		OutError = TEXT("One or more Beta6 DataTables are missing or use the wrong row struct.");
		return false;
	}
	if (ChapterTable->GetRowNames().Num() != Beta6ChapterCount
		|| ThemeTable->GetRowNames().Num() != Beta6ThemeCount
		|| WaveSpawnTable->GetRowNames().Num() != Beta6WaveSpawnRowCount
		|| EnemyTable->GetRowNames().Num() != Beta6EnemyCount
		|| BossTable->GetRowNames().Num() != Beta6BossCount
		|| BossAttackTable->GetRowNames().Num() != Beta6BossAttackCount)
	{
		OutError = TEXT("Beta6 DataTable row counts differ from the frozen projection (6/6/66/12/2/8).");
		return false;
	}

	TMap<int32, FNaiwaP4Beta6ChapterBundle> CandidateChapters;
	for (const FName RowName : ChapterTable->GetRowNames())
	{
		const FNaiwaP4Beta6ChapterRow* Chapter = ChapterTable->FindRow<FNaiwaP4Beta6ChapterRow>(
			RowName, TEXT("P4 Beta6 chapter catalog validation"), false);
		if (!Chapter || Chapter->ChapterIndex < 1 || Chapter->ChapterIndex > Beta6ChapterCount
			|| Chapter->WaveCount <= 0 || Chapter->OrdinaryEnemyCount < 0 || Chapter->ThemeId.IsNone())
		{
			OutError = FString::Printf(TEXT("Beta6 chapter row '%s' is invalid."), *RowName.ToString());
			return false;
		}
		if (CandidateChapters.Contains(Chapter->ChapterIndex))
		{
			OutError = FString::Printf(TEXT("Beta6 has duplicate ChapterIndex %d."), Chapter->ChapterIndex);
			return false;
		}

		const FNaiwaP4Beta6ThemeRow* Theme = ThemeTable->FindRow<FNaiwaP4Beta6ThemeRow>(
			Chapter->ThemeId, TEXT("P4 Beta6 theme reference validation"), false);
		if (!Theme || Theme->ThemeId != Chapter->ThemeId)
		{
			OutError = FString::Printf(TEXT("Chapter %d references missing theme '%s'."),
				Chapter->ChapterIndex, *Chapter->ThemeId.ToString());
			return false;
		}

		FNaiwaP4Beta6ChapterBundle& Bundle = CandidateChapters.FindOrAdd(Chapter->ChapterIndex);
		Bundle.Chapter = *Chapter;
		Bundle.Theme = *Theme;
		Bundle.bThemeRuntimeReady = Theme->RuntimeAssetBindingsStatus == FName(TEXT("accepted_alpha_theme_frozen"))
			|| Theme->RuntimeAssetBindingsStatus == FName(TEXT("approved_runtime_bindings"));
		if (!Bundle.bThemeRuntimeReady)
		{
			Bundle.RuntimeBlockers.Add(FString::Printf(
				TEXT("Chapter theme binding status is '%s'; approved runtime asset bindings are required."),
				*Theme->RuntimeAssetBindingsStatus.ToString()));
		}
	}

	for (int32 ChapterIndex = 1; ChapterIndex <= Beta6ChapterCount; ++ChapterIndex)
	{
		if (!CandidateChapters.Contains(ChapterIndex))
		{
			OutError = FString::Printf(TEXT("Beta6 chapter indices are not contiguous; Chapter %d is missing."), ChapterIndex);
			return false;
		}
	}

	for (const FName RowName : WaveSpawnTable->GetRowNames())
	{
		const FNaiwaP4Beta6WaveSpawnRow* Spawn = WaveSpawnTable->FindRow<FNaiwaP4Beta6WaveSpawnRow>(
			RowName, TEXT("P4 Beta6 wave row validation"), false);
		if (!Spawn || Spawn->ChapterIndex < 1 || Spawn->ChapterIndex > Beta6ChapterCount
			|| Spawn->WaveIndex <= 0 || Spawn->SpawnOrder < 0 || Spawn->Count <= 0
			|| Spawn->SpawnIntervalSeconds < 0.0f)
		{
			OutError = FString::Printf(TEXT("Beta6 wave row '%s' is invalid."), *RowName.ToString());
			return false;
		}

		FNaiwaP4Beta6ChapterBundle& Bundle = CandidateChapters.FindChecked(Spawn->ChapterIndex);
		if (Spawn->WaveIndex > Bundle.Chapter.WaveCount)
		{
			OutError = FString::Printf(TEXT("Wave row '%s' exceeds Chapter %d's declared wave count."),
				*RowName.ToString(), Spawn->ChapterIndex);
			return false;
		}

		const FNaiwaP4Beta6EnemyRow* Enemy = EnemyTable->FindRow<FNaiwaP4Beta6EnemyRow>(
			Spawn->EnemyTypeId, TEXT("P4 Beta6 enemy reference validation"), false);
		if (!Enemy || Enemy->EnemyId != Spawn->EnemyTypeId)
		{
			OutError = FString::Printf(TEXT("Wave row '%s' references missing enemy '%s'."),
				*RowName.ToString(), *Spawn->EnemyTypeId.ToString());
			return false;
		}

		if (!Spawn->bSpawnIntervalConfigured)
		{
			AddBlocker(Bundle, TEXT("One or more wave spawn intervals are not configured."));
		}
		if (!Enemy->bRuntimeTuningConfigured || Enemy->MaxHP <= 0 || Enemy->Damage <= 0
			|| Enemy->TelegraphSeconds <= 0.0f || !HasValidEnemyBehaviorTuning(*Enemy))
		{
			AddBlocker(Bundle, TEXT("One or more enemies lack valid runtime movement/attack tuning."));
		}
		if (!IsArenaEnemyBehaviorImplemented(Enemy->BehaviorId))
		{
			AddBlocker(Bundle, TEXT("One or more enemy behavior IDs have no Arena executor yet."));
		}
		Bundle.EnemiesById.Add(Spawn->EnemyTypeId, *Enemy);
		Bundle.WaveSpawns.Add(*Spawn);
	}

	for (int32 ChapterIndex = 1; ChapterIndex <= Beta6ChapterCount; ++ChapterIndex)
	{
		FNaiwaP4Beta6ChapterBundle& Bundle = CandidateChapters.FindChecked(ChapterIndex);
		TArray<int32> CountsByWave;
		CountsByWave.Init(0, Bundle.Chapter.WaveCount);
		int64 ProjectedEnemyCount = 0;
		for (const FNaiwaP4Beta6WaveSpawnRow& Spawn : Bundle.WaveSpawns)
		{
			CountsByWave[Spawn.WaveIndex - 1] += Spawn.Count;
			ProjectedEnemyCount += Spawn.Count;
		}
		for (int32 WaveIndex = 0; WaveIndex < CountsByWave.Num(); ++WaveIndex)
		{
			if (CountsByWave[WaveIndex] <= 0)
			{
				OutError = FString::Printf(TEXT("Chapter %d is missing wave %d."), ChapterIndex, WaveIndex + 1);
				return false;
			}
		}
		if (ProjectedEnemyCount != Bundle.Chapter.OrdinaryEnemyCount)
		{
			OutError = FString::Printf(TEXT("Chapter %d spawn total (%lld) differs from chapter row total (%d)."),
				ChapterIndex, ProjectedEnemyCount, Bundle.Chapter.OrdinaryEnemyCount);
			return false;
		}

		if (!Bundle.Chapter.BossId.IsNone())
		{
			const FNaiwaP4Beta6BossRow* Boss = BossTable->FindRow<FNaiwaP4Beta6BossRow>(
				Bundle.Chapter.BossId, TEXT("P4 Beta6 Boss reference validation"), false);
			if (!Boss || Boss->ChapterIndex != ChapterIndex)
			{
				OutError = FString::Printf(TEXT("Chapter %d references a missing or misassigned Boss '%s'."),
					ChapterIndex, *Bundle.Chapter.BossId.ToString());
				return false;
			}
			Bundle.Boss = *Boss;
			if (Boss->MaxHP <= 0 || Boss->PhaseTwoAt <= 0.0f || Boss->PhaseTwoAt >= 1.0f
				|| Boss->TelegraphSecondsMinimum < 0.65f || Boss->BreakWindowSeconds <= 0.0f
				|| Boss->BreakDamageMultiplier < 1.0f || Boss->BackDamageMultiplier < 1.0f
				|| Boss->AttackIds.Num() != 4 || Boss->MoveSpeed <= 0.0f || Boss->AttackRange <= 0.0f
				|| Boss->CollisionRadius <= 0.0f || Boss->PhaseTwoTransitionSeconds <= 0.0f
				|| Boss->PhaseTwoDamageMultiplier < 1.0f || Boss->PhaseTwoAttackIntervalMultiplier <= 0.0f
				|| Boss->PhaseTwoAttackIntervalMultiplier > 1.0f)
			{
				OutError = FString::Printf(TEXT("Boss '%s' does not match the frozen Beta6 baseline."),
					*Bundle.Chapter.BossId.ToString());
				return false;
			}
			TSet<FName> ProjectedAttackIds;
			TSet<int32> ProjectedAttackOrders;
			int32 BreakWindowRowCount = 0;
			for (const FName AttackId : Boss->AttackIds)
			{
				if (AttackId.IsNone() || ProjectedAttackIds.Contains(AttackId))
				{
					OutError = FString::Printf(TEXT("Boss '%s' has an empty or duplicate attack ID."),
						*Bundle.Chapter.BossId.ToString());
					return false;
				}
				ProjectedAttackIds.Add(AttackId);
				const FName AttackRowName(*FString::Printf(TEXT("%s_%s"),
					*Bundle.Chapter.BossId.ToString(), *AttackId.ToString()));
				const FNaiwaP4Beta6BossAttackRow* Attack = BossAttackTable->FindRow<FNaiwaP4Beta6BossAttackRow>(
					AttackRowName, TEXT("P4 Beta6 Boss attack reference validation"), false);
				if (!Attack || Attack->BossId != Bundle.Chapter.BossId || Attack->AttackId != AttackId)
				{
					OutError = FString::Printf(TEXT("Boss '%s' has no matching attack row for '%s'."),
						*Bundle.Chapter.BossId.ToString(), *AttackId.ToString());
					return false;
				}
				Bundle.BossAttacks.Add(*Attack);
				if (Attack->AttackOrder <= 0 || ProjectedAttackOrders.Contains(Attack->AttackOrder))
				{
					OutError = FString::Printf(TEXT("Boss '%s' has an invalid or duplicate attack order."),
						*Bundle.Chapter.BossId.ToString());
					return false;
				}
				ProjectedAttackOrders.Add(Attack->AttackOrder);
				const bool bIsProjectileAttack = Attack->DesignActionKind == FName(TEXT("ground_projectile"));
				const bool bHasValidHitGeometry = Attack->Damage >= 0 && (Attack->Damage == 0
					|| (!Attack->HitboxGeometry.IsNone() && (bIsProjectileAttack
						? Attack->ProjectileSpeed > 0.0f
						: Attack->HitboxExtentX > 0.0f && Attack->HitboxExtentY > 0.0f)));
				const bool bHasValidSummonData = !Attack->bSummonsEnemies
					|| (Attack->SummonCount > 0 && !Attack->SummonEnemyIds.IsEmpty());
				const bool bHasValidBreakWindow = Attack->bBreakWindowConfigured
					? Attack->BreakWindowSeconds > 0.0f : Attack->BreakWindowSeconds == 0.0f;
				if (Attack->bBreakWindowConfigured)
				{
					++BreakWindowRowCount;
					if (!FMath::IsNearlyEqual(Attack->BreakWindowSeconds, Boss->BreakWindowSeconds)
						|| !FMath::IsNearlyEqual(Attack->BreakDamageMultiplier, Boss->BreakDamageMultiplier)
						|| !FMath::IsNearlyEqual(Attack->BackDamageMultiplier, Boss->BackDamageMultiplier))
					{
						OutError = FString::Printf(TEXT("Boss '%s' break-window summary disagrees with attack '%s'."),
							*Bundle.Chapter.BossId.ToString(), *AttackId.ToString());
						return false;
					}
				}
				const bool bHasValidHitSequence = Attack->Damage == 0
					? Attack->HitCount == 0
					: Attack->HitCount > 0 && Attack->PhaseTwoHitCount >= Attack->HitCount
						&& (Attack->HitCount < 2 || Attack->HitIntervalSeconds > 0.0f);
				if (!Attack->bRuntimeTuningConfigured || Attack->TelegraphSecondsMinimum <= 0.0f
					|| Attack->ActiveDurationSeconds <= 0.0f || Attack->RecoverySeconds < 0.0f
					|| Attack->BreakDamageMultiplier < 1.0f || Attack->BackDamageMultiplier < 1.0f
					|| !bHasValidHitGeometry || !bHasValidHitSequence || !bHasValidSummonData || !bHasValidBreakWindow
					|| Attack->DesignActionKind.IsNone())
				{
					AddBlocker(Bundle, TEXT("One or more Boss attacks lack valid runtime action tuning."));
				}
				for (const FName SummonEnemyId : Attack->SummonEnemyIds)
				{
					const FNaiwaP4Beta6EnemyRow* SummonedEnemy = EnemyTable->FindRow<FNaiwaP4Beta6EnemyRow>(SummonEnemyId,
						TEXT("P4 Beta6 Boss summon reference validation"), false);
					if (!SummonedEnemy)
					{
						OutError = FString::Printf(TEXT("Boss '%s' attack '%s' summons missing enemy '%s'."),
							*Bundle.Chapter.BossId.ToString(), *AttackId.ToString(), *SummonEnemyId.ToString());
						return false;
					}
					if (!SummonedEnemy->bRuntimeTuningConfigured || !HasValidEnemyBehaviorTuning(*SummonedEnemy)
						|| !IsArenaEnemyBehaviorImplemented(SummonedEnemy->BehaviorId))
					{
						AddBlocker(Bundle, FString::Printf(TEXT("Boss summon enemy '%s' lacks implemented runtime tuning."),
							*SummonEnemyId.ToString()));
					}
					Bundle.EnemiesById.Add(SummonEnemyId, *SummonedEnemy);
				}
			}
			for (int32 ExpectedOrder = 1; ExpectedOrder <= Boss->AttackIds.Num(); ++ExpectedOrder)
			{
				if (!ProjectedAttackOrders.Contains(ExpectedOrder))
				{
					OutError = FString::Printf(TEXT("Boss '%s' attack order is not contiguous."),
						*Bundle.Chapter.BossId.ToString());
					return false;
				}
			}
			Bundle.BossAttacks.Sort([](const FNaiwaP4Beta6BossAttackRow& Left,
				const FNaiwaP4Beta6BossAttackRow& Right)
			{
				if (Left.AttackOrder != Right.AttackOrder) return Left.AttackOrder < Right.AttackOrder;
				return Left.AttackId.LexicalLess(Right.AttackId);
			});
			if (BreakWindowRowCount != 1)
			{
				OutError = FString::Printf(TEXT("Boss '%s' must define exactly one attack break-window row."),
					*Bundle.Chapter.BossId.ToString());
				return false;
			}
			if (!Boss->bRuntimeAttackTuningConfigured)
			{
				AddBlocker(Bundle, TEXT("Boss attack-specific runtime tuning is not configured."));
			}
		}

		Bundle.WaveSpawns.Sort([](const FNaiwaP4Beta6WaveSpawnRow& Left,
			const FNaiwaP4Beta6WaveSpawnRow& Right)
		{
			if (Left.WaveIndex != Right.WaveIndex) return Left.WaveIndex < Right.WaveIndex;
			if (Left.SpawnOrder != Right.SpawnOrder) return Left.SpawnOrder < Right.SpawnOrder;
			return Left.EnemyTypeId.LexicalLess(Right.EnemyTypeId);
		});
	}

	ChaptersByIndex = MoveTemp(CandidateChapters);
	OutError.Reset();
	return true;
}

const FNaiwaP4Beta6ChapterBundle* FNaiwaP4Beta6ChapterCatalog::FindChapter(const int32 ChapterIndex) const
{
	return ChaptersByIndex.Find(ChapterIndex);
}

TArray<int32> FNaiwaP4Beta6ChapterCatalog::GetOrderedChapterIndices() const
{
	TArray<int32> Indices;
	ChaptersByIndex.GetKeys(Indices);
	Indices.Sort();
	return Indices;
}

bool FNaiwaP4Beta6ChapterCatalog::CanStartChapter(const int32 ChapterIndex, FString& OutError) const
{
	const FNaiwaP4Beta6ChapterBundle* Bundle = FindChapter(ChapterIndex);
	if (!Bundle)
	{
		OutError = FString::Printf(TEXT("Beta6 chapter %d is not loaded."), ChapterIndex);
		return false;
	}
	if (!Bundle->IsCombatRuntimeReady())
	{
		OutError = FString::Printf(TEXT("Beta6 chapter %d is not runtime-ready: %s"), ChapterIndex,
			*FString::Join(Bundle->RuntimeBlockers, TEXT(" ")));
		return false;
	}
	OutError.Reset();
	return true;
}
