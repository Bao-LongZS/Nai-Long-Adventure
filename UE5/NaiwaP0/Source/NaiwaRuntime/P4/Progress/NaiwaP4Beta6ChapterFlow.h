#pragma once

#include "CoreMinimal.h"
#include "../Data/NaiwaP4Beta6ChapterCatalog.h"

class UDataTable;
class UNaiwaP4ProgressSubsystem;

enum class ENaiwaP4Beta6FlowPhase : uint8
{
	NoRun,
	ChapterDataBlocked,
	WaveActive,
	BossActive,
	ChapterSettlement,
	AwaitingChapterStart,
	Beta6StageEnd
};

/** Reusable data-driven chapter state machine; gameplay actors consume its wave/Boss outputs. */
class NAIWARUNTIME_API FNaiwaP4Beta6ChapterFlow final
{
public:
	bool LoadCatalog(const UDataTable* ChapterTable, const UDataTable* ThemeTable,
		const UDataTable* WaveSpawnTable, const UDataTable* EnemyTable,
		const UDataTable* BossTable, const UDataTable* BossAttackTable,
		FString& OutError);

	bool StartNewRun(int64 RunSeed, UNaiwaP4ProgressSubsystem& Progress, FString& OutError);
	bool ResumeRun(UNaiwaP4ProgressSubsystem& Progress, FString& OutError);
	bool RetryActiveChapter(UNaiwaP4ProgressSubsystem& Progress, FString& OutError);
	bool BeginActiveChapter(UNaiwaP4ProgressSubsystem& Progress, FString& OutError);
	bool RecordAcquiredUpgrade(UNaiwaP4ProgressSubsystem& Progress, FName CardId, FString& OutError);
	bool NotifyWaveCleared(FString& OutError);
	bool NotifyBossDefeated(FString& OutError);
	bool CompleteChapter(UNaiwaP4ProgressSubsystem& Progress, float CumulativeRunSeconds,
		int32 HighestCombo, int32& OutNextChapterIndex, FString& OutError);

	ENaiwaP4Beta6FlowPhase GetPhase() const { return Phase; }
	int32 GetCurrentChapterIndex() const { return CurrentChapterIndex; }
	int32 GetCurrentWaveIndex() const { return CurrentWaveIndex; }
	int64 GetCurrentChapterSeed() const { return CurrentChapterSeed; }
	const FNaiwaP4Beta6ChapterBundle* GetCurrentChapter() const;
	TArray<FNaiwaP4Beta6WaveSpawnRow> GetCurrentWaveSpawns() const;
	FName GetBossIdToSpawn() const;
	const FNaiwaP4Beta6ChapterCatalog& GetCatalog() const { return Catalog; }

private:
	FNaiwaP4Beta6ChapterCatalog Catalog;
	ENaiwaP4Beta6FlowPhase Phase = ENaiwaP4Beta6FlowPhase::NoRun;
	int32 CurrentChapterIndex = 0;
	int32 CurrentWaveIndex = 0;
	int64 CurrentChapterSeed = 0;
};
