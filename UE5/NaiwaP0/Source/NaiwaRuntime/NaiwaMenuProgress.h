#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NaiwaMenuProgress.generated.h"

USTRUCT(BlueprintType)
struct FNaiwaLevelRecord
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadOnly)
    bool bCleared = false;

    UPROPERTY(SaveGame, BlueprintReadOnly)
    int32 BestShots = 0;

    UPROPERTY(SaveGame, BlueprintReadOnly)
    float BestTimeSeconds = 0.0f;

    UPROPERTY(SaveGame, BlueprintReadOnly)
    int32 BestDeaths = 0;

    UPROPERTY(SaveGame, BlueprintReadOnly)
    int32 BestCollectibles = 0;
};

UCLASS()
class NAIWARUNTIME_API UNaiwaMenuSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame)
    TMap<FName, FNaiwaLevelRecord> LevelRecords;
};

UCLASS()
class NAIWARUNTIME_API UNaiwaProgressSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    bool IsLevelUnlocked(const FString& LevelId) const;
    bool GetLevelRecord(const FString& LevelId, FNaiwaLevelRecord& OutRecord) const;
    int32 GetClearedLevelCount() const;
    void RecordLevelClear(const FString& LevelId, int32 Shots, float TimeSeconds,
        int32 Deaths, int32 Collectibles);

private:
    void SaveProgress();

    UPROPERTY(Transient)
    TObjectPtr<UNaiwaMenuSaveGame> SaveData;

    static constexpr int32 SaveUserIndex = 0;
    static const TCHAR* SaveSlotName;
};
