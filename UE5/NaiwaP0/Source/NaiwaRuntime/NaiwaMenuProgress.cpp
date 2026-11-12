#include "NaiwaMenuProgress.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "P5/Data/NaiwaP5SkinCatalog.h"

const TCHAR* UNaiwaProgressSubsystem::SaveSlotName = TEXT("NaiwaMenuProgress");

namespace
{
	FName GetP6BaseClearRewardKey()
	{
		return FName(TEXT("P6_FirstClear_Base"));
	}

	FName GetP6PerfectClearRewardKey()
	{
		return FName(TEXT("P6_FirstClear_Perfect"));
	}

	FName GetP6CaseRunRewardKey(const FGuid& CaseRunId)
	{
		return FName(*FString::Printf(TEXT("P6_CaseRun_%s"), *CaseRunId.ToString(EGuidFormats::Digits)));
	}

	int32 AddMilkTickets(UNaiwaMenuSaveGame& SaveData, const int32 Tickets)
	{
		const int32 CurrentTickets = FMath::Max(0, SaveData.MilkTickets);
		const int32 TicketsAdded = FMath::Clamp(Tickets, 0, MAX_int32 - CurrentTickets);
		SaveData.MilkTickets = CurrentTickets + TicketsAdded;
		return TicketsAdded;
	}

	bool IsP6ProbeMode()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("NaiwaP6M1Probe")) ||
			FParse::Param(FCommandLine::Get(), TEXT("NaiwaP6M2Probe")) ||
			FParse::Param(FCommandLine::Get(), TEXT("NaiwaP6M3Probe")) ||
			FParse::Param(FCommandLine::Get(), TEXT("NaiwaP6M4Probe"));
	}
}

void UNaiwaProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const bool bP6Probe = IsP6ProbeMode();
    if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
    {
        SaveData = Cast<UNaiwaMenuSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));
    }
    if (!SaveData)
    {
        SaveData = Cast<UNaiwaMenuSaveGame>(UGameplayStatics::CreateSaveGameObject(UNaiwaMenuSaveGame::StaticClass()));
    }

    if (SaveData)
    {
        const FName BaseSkinId(TEXT("NaiwaSkin_Base"));
        const bool bAddedBaseSkin = !SaveData->OwnedP5SkinIds.Contains(BaseSkinId);
        SaveData->OwnedP5SkinIds.AddUnique(BaseSkinId);
        bool bNeedsSave = bAddedBaseSkin;
        if (!SaveData->OwnedP5SkinIds.Contains(SaveData->SelectedP5SkinId))
        {
            SaveData->SelectedP5SkinId = BaseSkinId;
            bNeedsSave = true;
        }
        if (SaveData->DataVersion < 2)
        {
            SaveData->DataVersion = 2;
            bNeedsSave = true;
        }
        if (SaveData->DataVersion < 3)
        {
            SaveData->P5BestRecord.bHasFinishedRun = SaveData->P5BestRecord.bHasRun &&
                SaveData->P5BestRecord.BestTimeSeconds > 0.0f;
            SaveData->DataVersion = 3;
            bNeedsSave = true;
        }
        if (SaveData->DataVersion < 4)
        {
            SaveData->DataVersion = 4;
            bNeedsSave = true;
        }
        if (SaveData->DataVersion < 5)
        {
            SaveData->P6WrongSubmissionCount = FMath::Max(0, SaveData->P6WrongSubmissionCount);
            SaveData->P6CaseElapsedMinutes = FMath::Clamp(SaveData->P6CaseElapsedMinutes, 20, 360);
            SaveData->P6BestCaseRating = FMath::Clamp(SaveData->P6BestCaseRating, 0, 2);
            SaveData->DataVersion = 5;
            bNeedsSave = true;
        }
        if (SaveData->DataVersion < 6)
        {
            SaveData->P6FailureStreakCount = 0;
            SaveData->DataVersion = 6;
            bNeedsSave = true;
        }
        if (SaveData->DataVersion < 7)
        {
            SaveData->P6BestCaseRating = FMath::Clamp(SaveData->P6BestCaseRating, 0, 2);
            SaveData->P6ClaimedRewardKeys.RemoveAll([](const FName RewardKey) { return RewardKey.IsNone(); });

            // M4 saved case ratings before ticket rewards were connected. Pay the
            // matching one-time first-clear amount during migration, then persist
            // the keys with the updated ticket balance in the same save write.
            if (SaveData->P6BestCaseRating >= 1 &&
                !SaveData->P6ClaimedRewardKeys.Contains(GetP6BaseClearRewardKey()))
            {
                AddMilkTickets(*SaveData, 60);
                SaveData->P6ClaimedRewardKeys.Add(GetP6BaseClearRewardKey());
            }
            if (SaveData->P6BestCaseRating >= 2 &&
                !SaveData->P6ClaimedRewardKeys.Contains(GetP6PerfectClearRewardKey()))
            {
                AddMilkTickets(*SaveData, 60);
                SaveData->P6ClaimedRewardKeys.Add(GetP6PerfectClearRewardKey());
            }

            SaveData->DataVersion = 7;
            bNeedsSave = true;
        }
        if (bNeedsSave && !bP6Probe)
        {
            SaveProgress();
        }
    }
}

bool UNaiwaProgressSubsystem::IsLevelUnlocked(const FString& LevelId) const
{
    if (LevelId == TEXT("1-1") || LevelId == TEXT("2-1")) return true;
    if (LevelId == TEXT("2-2"))
    {
        FNaiwaLevelRecord Record;
        return GetLevelRecord(TEXT("2-1"), Record) && Record.bCleared;
    }
    if (LevelId == TEXT("2-3"))
    {
        FNaiwaLevelRecord Record;
        return GetLevelRecord(TEXT("2-2"), Record) && Record.bCleared;
    }
    if (LevelId == TEXT("3-1"))
    {
        FNaiwaLevelRecord Record;
        return GetLevelRecord(TEXT("2-3"), Record) && Record.bCleared;
    }
    if (LevelId == TEXT("3-2"))
    {
        FNaiwaLevelRecord Record;
        return GetLevelRecord(TEXT("3-1"), Record) && Record.bCleared;
    }
    if (LevelId == TEXT("4-1"))
    {
        FNaiwaLevelRecord Record;
        return GetLevelRecord(TEXT("3-2"), Record) && Record.bCleared;
    }
    if (LevelId == TEXT("5-1"))
    {
        FNaiwaLevelRecord Record;
        return GetLevelRecord(TEXT("4-1"), Record) && Record.bCleared;
    }
    return false;
}

bool UNaiwaProgressSubsystem::GetLevelRecord(const FString& LevelId, FNaiwaLevelRecord& OutRecord) const
{
    if (!SaveData) return false;
    if (const FNaiwaLevelRecord* Record = SaveData->LevelRecords.Find(FName(*LevelId)))
    {
        OutRecord = *Record;
        return true;
    }
    return false;
}

int32 UNaiwaProgressSubsystem::GetClearedLevelCount() const
{
    if (!SaveData) return 0;
    int32 Count = 0;
    for (const TPair<FName, FNaiwaLevelRecord>& Pair : SaveData->LevelRecords)
    {
        if (Pair.Value.bCleared) ++Count;
    }
    return Count;
}

int32 UNaiwaProgressSubsystem::GetMilkTickets() const
{
    return SaveData ? FMath::Max(0, SaveData->MilkTickets) : 0;
}

const TArray<FName>& UNaiwaProgressSubsystem::GetOwnedP5SkinIds() const
{
    static const TArray<FName> EmptyIds;
    return SaveData ? SaveData->OwnedP5SkinIds : EmptyIds;
}

FName UNaiwaProgressSubsystem::GetSelectedP5SkinId() const
{
    if (SaveData && IsP5SkinOwned(SaveData->SelectedP5SkinId))
    {
        return SaveData->SelectedP5SkinId;
    }
    return FName(TEXT("NaiwaSkin_Base"));
}

const FNaiwaP5SaveRecord& UNaiwaProgressSubsystem::GetP5BestRecord() const
{
    static const FNaiwaP5SaveRecord EmptyRecord;
    return SaveData ? SaveData->P5BestRecord : EmptyRecord;
}

const TArray<FName>& UNaiwaProgressSubsystem::GetP6CollectedClueIds() const
{
    static const TArray<FName> EmptyIds;
    return SaveData ? SaveData->P6CollectedClueIds : EmptyIds;
}

int32 UNaiwaProgressSubsystem::GetP6WrongSubmissionCount() const
{
    return SaveData ? FMath::Max(0, SaveData->P6WrongSubmissionCount) : 0;
}

int32 UNaiwaProgressSubsystem::GetP6FailureStreakCount() const
{
    return SaveData ? FMath::Max(0, SaveData->P6FailureStreakCount) : 0;
}

int32 UNaiwaProgressSubsystem::GetP6CaseElapsedMinutes() const
{
    return SaveData ? FMath::Clamp(SaveData->P6CaseElapsedMinutes, 20, 360) : 20;
}

int32 UNaiwaProgressSubsystem::GetP6BestCaseRating() const
{
    return SaveData ? FMath::Clamp(SaveData->P6BestCaseRating, 0, 2) : 0;
}

bool UNaiwaProgressSubsystem::IsP5SkinOwned(const FName SkinId) const
{
    if (SkinId == FName(TEXT("NaiwaSkin_Base")))
    {
        return true;
    }
    return SaveData && SaveData->OwnedP5SkinIds.Contains(SkinId);
}

bool UNaiwaProgressSubsystem::TryPurchaseP5Skin(const FName SkinId)
{
    if (!SaveData || IsP5SkinOwned(SkinId))
    {
        return IsP5SkinOwned(SkinId);
    }
    const FNaiwaP5SkinRow* SkinRow = FNaiwaP5SkinCatalog::GetRows().FindByPredicate(
        [SkinId](const FNaiwaP5SkinRow& Row) { return Row.SkinId == SkinId; });
    if (!SkinRow || SkinRow->TicketPrice < 0 || SaveData->MilkTickets < SkinRow->TicketPrice)
    {
        return false;
    }
    SaveData->MilkTickets -= SkinRow->TicketPrice;
    SaveData->OwnedP5SkinIds.AddUnique(SkinId);
    SaveProgress();
    return true;
}

bool UNaiwaProgressSubsystem::SetSelectedP5Skin(const FName SkinId)
{
    if (!SaveData || !IsP5SkinOwned(SkinId))
    {
        return false;
    }
	const bool bIsKnownSkin = FNaiwaP5SkinCatalog::GetRows().FindByPredicate(
		[SkinId](const FNaiwaP5SkinRow& Row) { return Row.SkinId == SkinId; }) != nullptr;
    if (!bIsKnownSkin)
    {
        return false;
    }
    SaveData->SelectedP5SkinId = SkinId;
    SaveProgress();
    return true;
}

void UNaiwaProgressSubsystem::RecordLevelClear(const FString& LevelId, int32 Shots, float TimeSeconds,
    int32 Deaths, int32 Collectibles)
{
    if (!SaveData || LevelId.IsEmpty()) return;
    FNaiwaLevelRecord& Record = SaveData->LevelRecords.FindOrAdd(FName(*LevelId));
    const bool bFirstClear = !Record.bCleared;
    Record.bCleared = true;
    Record.BestShots = bFirstClear ? Shots : FMath::Min(Record.BestShots, Shots);
    Record.BestTimeSeconds = bFirstClear ? TimeSeconds : FMath::Min(Record.BestTimeSeconds, TimeSeconds);
    Record.BestDeaths = bFirstClear ? Deaths : FMath::Min(Record.BestDeaths, Deaths);
    Record.BestCollectibles = bFirstClear ? Collectibles : FMath::Max(Record.BestCollectibles, Collectibles);
    SaveProgress();
}

void UNaiwaProgressSubsystem::RecordP5RunSettlement(const bool bFinished, const int32 TicketsEarned,
    const int32 DistanceMm, const int32 Score, const float TimeSeconds, const int32 Falls,
    const int32 CoinsCollected)
{
    if (!SaveData)
    {
        return;
    }

    SaveData->MilkTickets = FMath::Max(0, SaveData->MilkTickets) + FMath::Max(0, TicketsEarned);
    FNaiwaP5SaveRecord& Best = SaveData->P5BestRecord;
    const bool bFirstRun = !Best.bHasRun;
    const bool bHadFinishedRun = Best.bHasFinishedRun;
    Best.bHasRun = true;
    Best.BestDistanceMm = bFirstRun ? FMath::Max(0, DistanceMm) : FMath::Max(Best.BestDistanceMm, DistanceMm);
    Best.BestScore = bFirstRun ? FMath::Max(0, Score) : FMath::Max(Best.BestScore, Score);
    if (bFinished && (!bHadFinishedRun || TimeSeconds < Best.BestTimeSeconds))
    {
        Best.BestTimeSeconds = FMath::Max(0.0f, TimeSeconds);
    }
    Best.bHasFinishedRun = Best.bHasFinishedRun || bFinished;
    Best.BestFalls = bFirstRun ? FMath::Max(0, Falls) : FMath::Min(Best.BestFalls, FMath::Max(0, Falls));

    FNaiwaLevelRecord& P5Record = SaveData->LevelRecords.FindOrAdd(FName(TEXT("4-1")));
    const bool bFirstClear = !P5Record.bCleared;
    P5Record.bCleared = true;
    P5Record.BestShots = 0;
    if (bFinished && (!bHadFinishedRun || P5Record.BestTimeSeconds <= 0.0f || TimeSeconds < P5Record.BestTimeSeconds))
    {
        P5Record.BestTimeSeconds = FMath::Max(0.0f, TimeSeconds);
    }
    P5Record.BestDeaths = bFirstClear ? FMath::Max(0, Falls) : FMath::Min(P5Record.BestDeaths, FMath::Max(0, Falls));
    P5Record.BestCollectibles = bFirstClear ? FMath::Max(0, CoinsCollected)
        : FMath::Max(P5Record.BestCollectibles, CoinsCollected);
    SaveProgress();
}

void UNaiwaProgressSubsystem::RecordP6ClueCollected(const FName ClueId)
{
    if (IsP6ProbeMode() || !SaveData || ClueId.IsNone() || SaveData->P6CollectedClueIds.Contains(ClueId))
    {
        return;
    }
    SaveData->P6CollectedClueIds.Add(ClueId);
    SaveProgress();
}

void UNaiwaProgressSubsystem::RecordP6FailedSubmission(const int32 MinutesAdvanced)
{
    if (IsP6ProbeMode() || !SaveData)
    {
        return;
    }
    SaveData->P6WrongSubmissionCount = FMath::Clamp(SaveData->P6WrongSubmissionCount, 0, MAX_int32 - 1) + 1;
    SaveData->P6FailureStreakCount = FMath::Clamp(SaveData->P6FailureStreakCount, 0, MAX_int32 - 1) + 1;
    SaveData->P6CaseElapsedMinutes = FMath::Clamp(
        FMath::Clamp(SaveData->P6CaseElapsedMinutes, 20, 360) + FMath::Clamp(MinutesAdvanced, 0, 360), 20, 360);
    SaveProgress();
}

bool UNaiwaProgressSubsystem::ApplyP6CaseClearReward(UNaiwaMenuSaveGame& TargetSave,
    const int32 Rating, const FGuid& CaseRunId, int32& OutTicketsGranted)
{
    OutTicketsGranted = 0;
    if (!CaseRunId.IsValid() || Rating < 1 || Rating > 2)
    {
        return false;
    }
    const FName RunRewardKey = GetP6CaseRunRewardKey(CaseRunId);
    if (TargetSave.P6ClaimedRewardKeys.Contains(RunRewardKey))
    {
        return false;
    }

    const int32 SafeRating = Rating;
    const int32 ExistingBestRating = FMath::Clamp(TargetSave.P6BestCaseRating, 0, 2);
    const int32 UpdatedBestRating = FMath::Max(ExistingBestRating, SafeRating);
    TargetSave.P6FailureStreakCount = 0;

    int32 RewardAmount = 0;
    const FName BaseRewardKey = GetP6BaseClearRewardKey();
    const FName PerfectRewardKey = GetP6PerfectClearRewardKey();
    const bool bHasBaseReward = TargetSave.P6ClaimedRewardKeys.Contains(BaseRewardKey);
    if (!bHasBaseReward)
    {
        TargetSave.P6ClaimedRewardKeys.Add(BaseRewardKey);
        RewardAmount += 60;
    }
    if (SafeRating >= 2 && !TargetSave.P6ClaimedRewardKeys.Contains(PerfectRewardKey))
    {
        TargetSave.P6ClaimedRewardKeys.Add(PerfectRewardKey);
        RewardAmount += 60;
    }
    if (bHasBaseReward)
    {
        RewardAmount += 20;
    }

    TargetSave.P6ClaimedRewardKeys.Add(RunRewardKey);
    OutTicketsGranted = AddMilkTickets(TargetSave, RewardAmount);
    TargetSave.P6BestCaseRating = UpdatedBestRating;
    return true;
}

int32 UNaiwaProgressSubsystem::RecordP6CaseClear(const int32 Rating, const FGuid& CaseRunId)
{
    if (IsP6ProbeMode() || !SaveData)
    {
        return 0;
    }
    int32 TicketsGranted = 0;
    if (!ApplyP6CaseClearReward(*SaveData, Rating, CaseRunId, TicketsGranted))
    {
        return 0;
    }
    SaveProgress();
    return TicketsGranted;
}

void UNaiwaProgressSubsystem::SaveProgress()
{
    if (SaveData)
    {
        UGameplayStatics::SaveGameToSlot(SaveData, SaveSlotName, SaveUserIndex);
    }
}
