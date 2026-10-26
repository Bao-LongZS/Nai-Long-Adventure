#include "NaiwaMenuProgress.h"

#include "Kismet/GameplayStatics.h"

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

void UNaiwaProgressSubsystem::SaveProgress()
{
    if (SaveData)
    {
        UGameplayStatics::SaveGameToSlot(SaveData, SaveSlotName, SaveUserIndex);
    }
}
