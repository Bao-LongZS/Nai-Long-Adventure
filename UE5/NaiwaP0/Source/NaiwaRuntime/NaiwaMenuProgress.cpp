#include "NaiwaMenuProgress.h"

#include "Kismet/GameplayStatics.h"
#include "P5/Data/NaiwaP5SkinCatalog.h"

const TCHAR* UNaiwaProgressSubsystem::SaveSlotName = TEXT("NaiwaMenuProgress");

void UNaiwaProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
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
        if (bNeedsSave)
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

void UNaiwaProgressSubsystem::SaveProgress()
{
    if (SaveData)
    {
        UGameplayStatics::SaveGameToSlot(SaveData, SaveSlotName, SaveUserIndex);
    }
}
