#include "NaiwaP4ProgressSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	UNaiwaP4SaveGame* CloneSaveData(const UNaiwaP4SaveGame* Source, UObject* Outer)
	{
		return Source ? DuplicateObject<UNaiwaP4SaveGame>(Source, Outer) : nullptr;
	}
}

const TCHAR* UNaiwaP4ProgressSubsystem::SaveSlotName = TEXT("NaiwaP4Beta6Progress");

void UNaiwaP4ProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bSaveWritable = true;

	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
	{
		CreateDefaultSave();
		return;
	}

	SaveData = Cast<UNaiwaP4SaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));
	if (!SaveData)
	{
		UE_LOG(LogTemp, Error, TEXT("P4 save slot exists but is not a UNaiwaP4SaveGame; leaving it untouched."));
		bSaveWritable = false;
		return;
	}

	if (SaveData->SchemaVersion > UNaiwaP4SaveGame::CurrentSchemaVersion)
	{
		UE_LOG(LogTemp, Warning, TEXT("P4 save schema %d is newer than supported schema %d; loaded read-only."),
			SaveData->SchemaVersion, UNaiwaP4SaveGame::CurrentSchemaVersion);
		bSaveWritable = false;
		return;
	}

	if (SaveData->SchemaVersion < UNaiwaP4SaveGame::CurrentSchemaVersion)
	{
		// Version 0 had no persisted Beta6 schema. Current defaults are intentionally safe.
		SaveData->SchemaVersion = UNaiwaP4SaveGame::CurrentSchemaVersion;
		SaveData->HighestUnlockedChapterIndex = FMath::Clamp(SaveData->HighestUnlockedChapterIndex,
			1, UNaiwaP4SaveGame::Beta6ChapterCount);
		SaveData->ActiveChapterIndex = FMath::Clamp(SaveData->ActiveChapterIndex,
			1, UNaiwaP4SaveGame::Beta6ChapterCount);
	}

	SaveData->HighestUnlockedChapterIndex = FMath::Clamp(SaveData->HighestUnlockedChapterIndex,
		1, UNaiwaP4SaveGame::Beta6ChapterCount);
	SaveData->ActiveChapterIndex = FMath::Clamp(SaveData->ActiveChapterIndex,
		1, UNaiwaP4SaveGame::Beta6ChapterCount);
	SaveData->ActiveChapterRetryCount = FMath::Max(0, SaveData->ActiveChapterRetryCount);
	SaveData->RunElapsedSeconds = FMath::Max(0.0f, SaveData->RunElapsedSeconds);
	SaveData->RunHighestCombo = FMath::Max(0, SaveData->RunHighestCombo);
}

bool UNaiwaP4ProgressSubsystem::IsChapterUnlocked(const int32 ChapterIndex) const
{
	return SaveData
		&& ChapterIndex >= 1
		&& ChapterIndex <= UNaiwaP4SaveGame::Beta6ChapterCount
		&& ChapterIndex <= SaveData->HighestUnlockedChapterIndex;
}

int32 UNaiwaP4ProgressSubsystem::GetActiveChapterIndex() const
{
	return SaveData ? SaveData->ActiveChapterIndex : 1;
}

bool UNaiwaP4ProgressSubsystem::HasActiveRun() const
{
	return SaveData && SaveData->bHasActiveRun && !SaveData->bBeta6StageComplete;
}

bool UNaiwaP4ProgressSubsystem::HasReachedBeta6StageEnd() const
{
	return SaveData && SaveData->bBeta6StageComplete;
}

TArray<FName> UNaiwaP4ProgressSubsystem::GetAcquiredUpgradeCardIds() const
{
	return SaveData ? SaveData->AcquiredUpgradeCardIds : TArray<FName>();
}

float UNaiwaP4ProgressSubsystem::GetRunElapsedSeconds() const
{
	return SaveData ? SaveData->RunElapsedSeconds : 0.0f;
}

int32 UNaiwaP4ProgressSubsystem::GetRunHighestCombo() const
{
	return SaveData ? SaveData->RunHighestCombo : 0;
}

bool UNaiwaP4ProgressSubsystem::StartNewBeta6Run(const int64 InRunSeed, FString& OutError)
{
	if (!bSaveWritable)
	{
		OutError = TEXT("P4 progress save is read-only because its slot is incompatible or newer.");
		return false;
	}
	if (InRunSeed < 0 || InRunSeed > static_cast<int64>(MAX_uint32))
	{
		OutError = TEXT("P4 run seed must fit an unsigned 32-bit value.");
		return false;
	}

	UNaiwaP4SaveGame* PreviousSaveData = CloneSaveData(SaveData, this);
	const float PreviousBestSeconds = SaveData ? SaveData->BestBeta6RunSeconds : 0.0f;
	const int32 PreviousBestCombo = SaveData ? SaveData->BestBeta6HighestCombo : 0;
	CreateDefaultSave();
	SaveData->BestBeta6RunSeconds = PreviousBestSeconds;
	SaveData->BestBeta6HighestCombo = PreviousBestCombo;
	SaveData->RunSeed = InRunSeed;
	SaveData->ActiveChapterIndex = 1;
	SaveData->ActiveChapterSeed = static_cast<int64>(DeriveChapterSeed(static_cast<uint32>(InRunSeed), 1));
	SaveData->bHasActiveRun = true;
	if (!Persist(OutError))
	{
		SaveData = PreviousSaveData;
		return false;
	}
	return true;
}

bool UNaiwaP4ProgressSubsystem::ResumeCurrentChapter(int32& OutChapterIndex,
	int64& OutChapterSeed, FString& OutError) const
{
	if (!SaveData || !SaveData->bHasActiveRun || SaveData->bBeta6StageComplete)
	{
		OutError = TEXT("There is no active P4 Beta6 run to resume.");
		return false;
	}
	if (!IsChapterUnlocked(SaveData->ActiveChapterIndex))
	{
		OutError = TEXT("The saved P4 chapter is outside the current unlocked progression.");
		return false;
	}

	OutChapterIndex = SaveData->ActiveChapterIndex;
	OutChapterSeed = SaveData->ActiveChapterSeed;
	OutError.Reset();
	return true;
}

bool UNaiwaP4ProgressSubsystem::RetryCurrentChapter(int64& OutChapterSeed, FString& OutError)
{
	if (!HasActiveRun() || !bSaveWritable)
	{
		OutError = TEXT("There is no writable active P4 Beta6 chapter to retry.");
		return false;
	}

	UNaiwaP4SaveGame* PreviousSaveData = CloneSaveData(SaveData, this);
	++SaveData->ActiveChapterRetryCount;
	OutChapterSeed = SaveData->ActiveChapterSeed;
	if (!Persist(OutError))
	{
		SaveData = PreviousSaveData;
		return false;
	}
	return true;
}

bool UNaiwaP4ProgressSubsystem::RecordAcquiredUpgrade(const FName CardId, FString& OutError)
{
	if (!HasActiveRun() || !bSaveWritable || CardId.IsNone())
	{
		OutError = TEXT("Cannot record the upgrade without a writable active P4 run and a valid card ID.");
		return false;
	}

	UNaiwaP4SaveGame* PreviousSaveData = CloneSaveData(SaveData, this);
	SaveData->AcquiredUpgradeCardIds.Add(CardId);
	if (!Persist(OutError))
	{
		SaveData = PreviousSaveData;
		return false;
	}
	return true;
}

bool UNaiwaP4ProgressSubsystem::CompleteCurrentChapter(const int32 ChapterIndex,
	const float CumulativeRunSeconds, const int32 HighestCombo, FString& OutError)
{
	if (!HasActiveRun() || !bSaveWritable)
	{
		OutError = TEXT("There is no writable active P4 Beta6 chapter to complete.");
		return false;
	}
	if (ChapterIndex != SaveData->ActiveChapterIndex || ChapterIndex < 1
		|| ChapterIndex > UNaiwaP4SaveGame::Beta6ChapterCount
		|| CumulativeRunSeconds < 0.0f || HighestCombo < 0)
	{
		OutError = TEXT("P4 chapter settlement does not match the active chapter or contains invalid run metrics.");
		return false;
	}

	UNaiwaP4SaveGame* PreviousSaveData = CloneSaveData(SaveData, this);
	SaveData->LastClearedChapterIndex = ChapterIndex;
	SaveData->RunElapsedSeconds = CumulativeRunSeconds;
	SaveData->RunHighestCombo = FMath::Max(SaveData->RunHighestCombo, HighestCombo);
	SaveData->ActiveChapterRetryCount = 0;

	if (ChapterIndex == UNaiwaP4SaveGame::Beta6ChapterCount)
	{
		SaveData->bHasActiveRun = false;
		SaveData->bBeta6StageComplete = true;
		if (SaveData->BestBeta6RunSeconds <= 0.0f
			|| CumulativeRunSeconds < SaveData->BestBeta6RunSeconds)
		{
			SaveData->BestBeta6RunSeconds = CumulativeRunSeconds;
		}
		SaveData->BestBeta6HighestCombo = FMath::Max(SaveData->BestBeta6HighestCombo,
			SaveData->RunHighestCombo);
	}
	else
	{
		SaveData->HighestUnlockedChapterIndex = FMath::Max(SaveData->HighestUnlockedChapterIndex,
			ChapterIndex + 1);
		SaveData->ActiveChapterIndex = ChapterIndex + 1;
		SaveData->ActiveChapterSeed = static_cast<int64>(DeriveChapterSeed(
			static_cast<uint32>(SaveData->RunSeed), SaveData->ActiveChapterIndex));
	}

	if (!Persist(OutError))
	{
		SaveData = PreviousSaveData;
		return false;
	}
	return true;
}

uint32 UNaiwaP4ProgressSubsystem::DeriveChapterSeed(const uint32 InRunSeed, const int32 ChapterIndex)
{
	constexpr uint32 ChapterSeedSalt = 0x9E3779B9u;
	return InRunSeed ^ (static_cast<uint32>(ChapterIndex) * ChapterSeedSalt);
}

bool UNaiwaP4ProgressSubsystem::Persist(FString& OutError) const
{
	if (!bSaveWritable || !SaveData)
	{
		OutError = TEXT("P4 progress save is unavailable or read-only.");
		return false;
	}
	SaveData->SchemaVersion = UNaiwaP4SaveGame::CurrentSchemaVersion;
	if (!UGameplayStatics::SaveGameToSlot(SaveData, SaveSlotName, SaveUserIndex))
	{
		OutError = TEXT("Could not write the P4-specific Beta6 progress slot.");
		return false;
	}
	OutError.Reset();
	return true;
}

void UNaiwaP4ProgressSubsystem::CreateDefaultSave()
{
	SaveData = Cast<UNaiwaP4SaveGame>(UGameplayStatics::CreateSaveGameObject(UNaiwaP4SaveGame::StaticClass()));
	if (!SaveData)
	{
		SaveData = NewObject<UNaiwaP4SaveGame>(this);
	}
	SaveData->SchemaVersion = UNaiwaP4SaveGame::CurrentSchemaVersion;
}
