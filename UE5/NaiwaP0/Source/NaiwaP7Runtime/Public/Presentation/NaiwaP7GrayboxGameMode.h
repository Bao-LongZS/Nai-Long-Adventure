#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NaiwaP7GrayboxGameMode.generated.h"

class UDataTable;

/** Explicit P7-only entry class for levels used to inspect the temporary graybox HUD. */
UCLASS(Blueprintable)
class NAIWAP7RUNTIME_API ANaiwaP7GrayboxGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ANaiwaP7GrayboxGameMode();

    virtual void StartPlay() override;

protected:
    UPROPERTY(EditDefaultsOnly, Category="P7|Graybox Data")
    TObjectPtr<UDataTable> LevelTable;

    UPROPERTY(EditDefaultsOnly, Category="P7|Graybox Data")
    TObjectPtr<UDataTable> TowerTable;

    UPROPERTY(EditDefaultsOnly, Category="P7|Graybox Data")
    TObjectPtr<UDataTable> EnemyTable;

    UPROPERTY(EditDefaultsOnly, Category="P7|Graybox Data")
    TObjectPtr<UDataTable> ObstacleTable;

    UPROPERTY(EditDefaultsOnly, Category="P7|Graybox Data")
    TObjectPtr<UDataTable> WaveTable;

    UPROPERTY(EditDefaultsOnly, Category="P7|Graybox Data")
    FName LevelRowName = NAME_None;
};

/** Alpha graybox entry with the provisional P7_Alpha01 DataTables pre-wired. */
UCLASS()
class NAIWAP7RUNTIME_API ANaiwaP7AlphaGrayboxGameMode final : public ANaiwaP7GrayboxGameMode
{
    GENERATED_BODY()

public:
    ANaiwaP7AlphaGrayboxGameMode();
};
