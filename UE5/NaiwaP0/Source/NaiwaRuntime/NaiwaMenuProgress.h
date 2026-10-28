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

USTRUCT(BlueprintType)
struct FNaiwaP5SaveRecord
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadOnly)
    bool bHasRun = false;

    UPROPERTY(SaveGame, BlueprintReadOnly)
    bool bHasFinishedRun = false;

    UPROPERTY(SaveGame, BlueprintReadOnly)
    int32 BestDistanceMm = 0;

    UPROPERTY(SaveGame, BlueprintReadOnly)
    int32 BestScore = 0;

    UPROPERTY(SaveGame, BlueprintReadOnly)
    float BestTimeSeconds = 0.0f;

    UPROPERTY(SaveGame, BlueprintReadOnly)
    int32 BestFalls = 0;
};

UCLASS()
class NAIWARUNTIME_API UNaiwaMenuSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame)
    int32 DataVersion = 1;

    UPROPERTY(SaveGame)
    TMap<FName, FNaiwaLevelRecord> LevelRecords;

    UPROPERTY(SaveGame, BlueprintReadOnly)
    int32 MilkTickets = 0;

    UPROPERTY(SaveGame, BlueprintReadOnly)
    TArray<FName> OwnedP5SkinIds;

    UPROPERTY(SaveGame, BlueprintReadOnly)
    FName SelectedP5SkinId = FName(TEXT("NaiwaSkin_Base"));

    UPROPERTY(SaveGame, BlueprintReadOnly)
    FNaiwaP5SaveRecord P5BestRecord;
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
    int32 GetMilkTickets() const;
    const TArray<FName>& GetOwnedP5SkinIds() const;
    FName GetSelectedP5SkinId() const;
    const FNaiwaP5SaveRecord& GetP5BestRecord() const;
    bool IsP5SkinOwned(FName SkinId) const;
    bool TryPurchaseP5Skin(FName SkinId);
    bool SetSelectedP5Skin(FName SkinId);
    void RecordLevelClear(const FString& LevelId, int32 Shots, float TimeSeconds,
        int32 Deaths, int32 Collectibles);
    void RecordP5RunSettlement(bool bFinished, int32 TicketsEarned, int32 DistanceMm, int32 Score,
        float TimeSeconds, int32 Falls, int32 CoinsCollected);

private:
    void SaveProgress();

    UPROPERTY(Transient)
    TObjectPtr<UNaiwaMenuSaveGame> SaveData;

    static constexpr int32 SaveUserIndex = 0;
    static const TCHAR* SaveSlotName;
};
