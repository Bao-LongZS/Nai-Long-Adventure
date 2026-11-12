#include "NaiwaP4Beta6ChapterFlow.h"

#include "NaiwaP4ProgressSubsystem.h"

bool FNaiwaP4Beta6ChapterFlow::LoadCatalog(const UDataTable* ChapterTable,
	const UDataTable* ThemeTable, const UDataTable* WaveSpawnTable,
	const UDataTable* EnemyTable, const UDataTable* BossTable,
	const UDataTable* BossAttackTable, FString& OutError)
{
	return Catalog.Load(ChapterTable, ThemeTable, WaveSpawnTable, EnemyTable,
		BossTable, BossAttackTable, OutError);
}

bool FNaiwaP4Beta6ChapterFlow::StartNewRun(const int64 RunSeed,
	UNaiwaP4ProgressSubsystem& Progress, FString& OutError)
{
	if (!Progress.StartNewBeta6Run(RunSeed, OutError))
	{
		Phase = ENaiwaP4Beta6FlowPhase::NoRun;
		return false;
	}
	return BeginActiveChapter(Progress, OutError);
}

bool FNaiwaP4Beta6ChapterFlow::ResumeRun(UNaiwaP4ProgressSubsystem& Progress, FString& OutError)
{
	return BeginActiveChapter(Progress, OutError);
}

bool FNaiwaP4Beta6ChapterFlow::RetryActiveChapter(UNaiwaP4ProgressSubsystem& Progress, FString& OutError)
{
	int64 RetrySeed = 0;
	if (!Progress.RetryCurrentChapter(RetrySeed, OutError))
	{
		return false;
	}
	if (!BeginActiveChapter(Progress, OutError))
	{
		return false;
	}
	if (CurrentChapterSeed != RetrySeed)
	{
		Phase = ENaiwaP4Beta6FlowPhase::ChapterDataBlocked;
		OutError = TEXT("P4 Beta6 retry changed the saved chapter entry seed.");
		return false;
	}
	return true;
}

bool FNaiwaP4Beta6ChapterFlow::BeginActiveChapter(UNaiwaP4ProgressSubsystem& Progress, FString& OutError)
{
	int32 SavedChapterIndex = 0;
	int64 SavedChapterSeed = 0;
	if (!Progress.ResumeCurrentChapter(SavedChapterIndex, SavedChapterSeed, OutError))
	{
		Phase = ENaiwaP4Beta6FlowPhase::NoRun;
		return false;
	}

	CurrentChapterIndex = SavedChapterIndex;
	CurrentChapterSeed = SavedChapterSeed;
	CurrentWaveIndex = 0;
	if (!Progress.IsChapterUnlocked(CurrentChapterIndex))
	{
		Phase = ENaiwaP4Beta6FlowPhase::ChapterDataBlocked;
		OutError = FString::Printf(TEXT("P4 Beta6 chapter %d is locked."), CurrentChapterIndex);
		return false;
	}
	if (!Catalog.CanStartChapter(CurrentChapterIndex, OutError))
	{
		Phase = ENaiwaP4Beta6FlowPhase::ChapterDataBlocked;
		return false;
	}

	CurrentWaveIndex = 1;
	Phase = ENaiwaP4Beta6FlowPhase::WaveActive;
	OutError.Reset();
	return true;
}

bool FNaiwaP4Beta6ChapterFlow::RecordAcquiredUpgrade(UNaiwaP4ProgressSubsystem& Progress,
	const FName CardId, FString& OutError)
{
	return Progress.RecordAcquiredUpgrade(CardId, OutError);
}

bool FNaiwaP4Beta6ChapterFlow::NotifyWaveCleared(FString& OutError)
{
	if (Phase != ENaiwaP4Beta6FlowPhase::WaveActive)
	{
		OutError = TEXT("P4 Beta6 cannot clear a wave unless a wave is active.");
		return false;
	}
	const FNaiwaP4Beta6ChapterBundle* Bundle = GetCurrentChapter();
	if (!Bundle || CurrentWaveIndex < 1 || CurrentWaveIndex > Bundle->Chapter.WaveCount)
	{
		OutError = TEXT("P4 Beta6 active chapter or wave is invalid.");
		return false;
	}

	if (CurrentWaveIndex < Bundle->Chapter.WaveCount)
	{
		++CurrentWaveIndex;
		OutError.Reset();
		return true;
	}
	if (!Bundle->Chapter.BossId.IsNone())
	{
		Phase = ENaiwaP4Beta6FlowPhase::BossActive;
	}
	else
	{
		Phase = ENaiwaP4Beta6FlowPhase::ChapterSettlement;
	}
	OutError.Reset();
	return true;
}

bool FNaiwaP4Beta6ChapterFlow::NotifyBossDefeated(FString& OutError)
{
	if (Phase != ENaiwaP4Beta6FlowPhase::BossActive || GetBossIdToSpawn().IsNone())
	{
		OutError = TEXT("P4 Beta6 cannot settle a Boss chapter unless its configured Boss is active.");
		return false;
	}
	Phase = ENaiwaP4Beta6FlowPhase::ChapterSettlement;
	OutError.Reset();
	return true;
}

bool FNaiwaP4Beta6ChapterFlow::CompleteChapter(UNaiwaP4ProgressSubsystem& Progress,
	const float CumulativeRunSeconds, const int32 HighestCombo,
	int32& OutNextChapterIndex, FString& OutError)
{
	OutNextChapterIndex = 0;
	if (Phase != ENaiwaP4Beta6FlowPhase::ChapterSettlement || CurrentChapterIndex <= 0)
	{
		OutError = TEXT("P4 Beta6 chapter settlement is not ready.");
		return false;
	}
	if (!Progress.CompleteCurrentChapter(CurrentChapterIndex, CumulativeRunSeconds, HighestCombo, OutError))
	{
		return false;
	}
	if (Progress.HasReachedBeta6StageEnd())
	{
		Phase = ENaiwaP4Beta6FlowPhase::Beta6StageEnd;
		CurrentWaveIndex = 0;
		return true;
	}

	OutNextChapterIndex = Progress.GetActiveChapterIndex();
	CurrentChapterIndex = OutNextChapterIndex;
	CurrentChapterSeed = 0;
	CurrentWaveIndex = 0;
	Phase = ENaiwaP4Beta6FlowPhase::AwaitingChapterStart;
	return true;
}

const FNaiwaP4Beta6ChapterBundle* FNaiwaP4Beta6ChapterFlow::GetCurrentChapter() const
{
	return Catalog.FindChapter(CurrentChapterIndex);
}

TArray<FNaiwaP4Beta6WaveSpawnRow> FNaiwaP4Beta6ChapterFlow::GetCurrentWaveSpawns() const
{
	TArray<FNaiwaP4Beta6WaveSpawnRow> Rows;
	if (Phase != ENaiwaP4Beta6FlowPhase::WaveActive || CurrentWaveIndex <= 0)
	{
		return Rows;
	}
	const FNaiwaP4Beta6ChapterBundle* Bundle = GetCurrentChapter();
	if (!Bundle)
	{
		return Rows;
	}
	for (const FNaiwaP4Beta6WaveSpawnRow& Spawn : Bundle->WaveSpawns)
	{
		if (Spawn.WaveIndex == CurrentWaveIndex)
		{
			Rows.Add(Spawn);
		}
	}
	return Rows;
}

FName FNaiwaP4Beta6ChapterFlow::GetBossIdToSpawn() const
{
	if (Phase != ENaiwaP4Beta6FlowPhase::BossActive)
	{
		return NAME_None;
	}
	const FNaiwaP4Beta6ChapterBundle* Bundle = GetCurrentChapter();
	return Bundle ? Bundle->Chapter.BossId : NAME_None;
}
