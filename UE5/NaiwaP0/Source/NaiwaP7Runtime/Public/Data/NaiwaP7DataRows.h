#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NaiwaP7DataRows.generated.h"

UENUM(BlueprintType)
enum class ENaiwaP7TowerRole : uint8
{
    DirectDamage,
    AreaDamage,
    SlowSupport
};

UENUM(BlueprintType)
enum class ENaiwaP7EnemyKind : uint8
{
    Normal,
    Fast,
    Interferer
};

USTRUCT(BlueprintType)
struct NAIWAP7RUNTIME_API FNaiwaP7TowerRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Tower")
    FName TowerId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Tower")
    ENaiwaP7TowerRole Role = ENaiwaP7TowerRole::DirectDamage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Tower", meta=(ClampMin="1", ClampMax="3"))
    int32 Level = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Tower", meta=(ClampMin="0"))
    int32 CostCoins = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Tower", meta=(ClampMin="0"))
    int32 Damage = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Tower", meta=(ClampMin="1"))
    int32 AttackIntervalSteps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Tower", meta=(ClampMin="1"))
    int32 RangeMilliCells = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Tower", meta=(ClampMin="0"))
    int32 SplashRadiusMilliCells = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Tower", meta=(ClampMin="0", ClampMax="99"))
    int32 SlowPercent = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Tower", meta=(ClampMin="0"))
    int32 SlowDurationSteps = 0;
};

USTRUCT(BlueprintType)
struct NAIWAP7RUNTIME_API FNaiwaP7EnemyRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Enemy")
    FName EnemyId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Enemy")
    ENaiwaP7EnemyKind Kind = ENaiwaP7EnemyKind::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Enemy", meta=(ClampMin="1"))
    int32 MaxHealth = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Enemy", meta=(ClampMin="1"))
    int32 SpeedMilliCellsPerSecond = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Enemy", meta=(ClampMin="0"))
    int32 InterferenceRadiusMilliCells = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Enemy", meta=(ClampMin="0"))
    int32 InterferenceDurationSteps = 0;
};

USTRUCT(BlueprintType)
struct NAIWAP7RUNTIME_API FNaiwaP7ObstacleRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Obstacle")
    FName ObstacleId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Obstacle")
    FIntPoint Cell = FIntPoint::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Obstacle", meta=(ClampMin="1"))
    int32 MaxHealth = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Obstacle", meta=(ClampMin="0"))
    int32 RewardCoins = 0;
};

USTRUCT(BlueprintType)
struct NAIWAP7RUNTIME_API FNaiwaP7WaveSpawnGroup
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Wave")
    FName EnemyId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Wave", meta=(ClampMin="1"))
    int32 Count = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Wave", meta=(ClampMin="0"))
    int32 FirstSpawnDelaySteps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Wave", meta=(ClampMin="0"))
    int32 SpawnIntervalSteps = 0;
};

USTRUCT(BlueprintType)
struct NAIWAP7RUNTIME_API FNaiwaP7WaveRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Wave", meta=(ClampMin="1", ClampMax="8"))
    int32 WaveIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Wave")
    TArray<FNaiwaP7WaveSpawnGroup> Spawns;
};

USTRUCT(BlueprintType)
struct NAIWAP7RUNTIME_API FNaiwaP7HiddenBuildSiteRow
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Level")
    FIntPoint Cell = FIntPoint::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Level")
    FName UnlockObstacleId = NAME_None;
};

USTRUCT(BlueprintType)
struct NAIWAP7RUNTIME_API FNaiwaP7LevelRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Level")
    FName LevelId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Level")
    TArray<FIntPoint> RouteACells;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Level")
    TArray<FIntPoint> RouteBCells;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Level")
    FIntPoint InterferenceTriggerCell = FIntPoint::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Level")
    TArray<FIntPoint> VisibleBuildCells;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Level")
    TArray<FName> ObstacleIds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Level")
    TArray<FNaiwaP7HiddenBuildSiteRow> HiddenBuildSites;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P7|Level", meta=(ClampMin="0"))
    int32 StartingCoins = 0;
};
