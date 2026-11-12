#pragma once

#include "CoreMinimal.h"
#include "NaiwaP4DataTypes.h"

class UDataTable;

/** A validated, per-chapter view over the Beta6 projection tables. */
struct FNaiwaP4Beta6ChapterBundle
{
	FNaiwaP4Beta6ChapterRow Chapter;
	FNaiwaP4Beta6ThemeRow Theme;
	TArray<FNaiwaP4Beta6WaveSpawnRow> WaveSpawns;
	TMap<FName, FNaiwaP4Beta6EnemyRow> EnemiesById;
	FNaiwaP4Beta6BossRow Boss;
	TArray<FNaiwaP4Beta6BossAttackRow> BossAttacks;
	TArray<FString> RuntimeBlockers;
	bool bThemeRuntimeReady = false;

	bool IsCombatRuntimeReady() const { return RuntimeBlockers.IsEmpty(); }
	const FNaiwaP4Beta6EnemyRow* FindEnemy(FName EnemyId) const { return EnemiesById.Find(EnemyId); }
	const FNaiwaP4Beta6BossAttackRow* FindBossAttack(FName AttackId) const
	{
		return BossAttacks.FindByPredicate([AttackId](const FNaiwaP4Beta6BossAttackRow& Attack)
			{ return Attack.AttackId == AttackId; });
	}
};

/** Validates and indexes Beta6 tables without embedding chapter-specific flow branches. */
class NAIWARUNTIME_API FNaiwaP4Beta6ChapterCatalog final
{
public:
	bool Load(const UDataTable* ChapterTable, const UDataTable* ThemeTable,
		const UDataTable* WaveSpawnTable, const UDataTable* EnemyTable,
		const UDataTable* BossTable, const UDataTable* BossAttackTable,
		FString& OutError);

	const FNaiwaP4Beta6ChapterBundle* FindChapter(int32 ChapterIndex) const;
	TArray<int32> GetOrderedChapterIndices() const;
	bool CanStartChapter(int32 ChapterIndex, FString& OutError) const;
	int32 Num() const { return ChaptersByIndex.Num(); }

private:
	TMap<int32, FNaiwaP4Beta6ChapterBundle> ChaptersByIndex;
};
