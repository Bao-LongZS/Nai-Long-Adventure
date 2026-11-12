#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/StreamableManager.h"
#include "NaiwaP4Beta6TestGameMode.generated.h"

class UDataTable;

/** Isolated startup and resume path for an internal P4 Beta6 test map. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP4Beta6TestGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANaiwaP4Beta6TestGameMode();
	virtual void StartPlay() override;

	float GetPreviewArenaDepth() const { return PreviewArenaDepth; }
	float GetPreviewCameraPitch() const { return PreviewCameraPitch; }

private:
	UPROPERTY(EditDefaultsOnly, Category="P4|Beta6|Data")
	TSoftObjectPtr<UDataTable> ChapterTable;

	UPROPERTY(EditDefaultsOnly, Category="P4|Beta6|Data")
	TSoftObjectPtr<UDataTable> ThemeTable;

	UPROPERTY(EditDefaultsOnly, Category="P4|Beta6|Data")
	TSoftObjectPtr<UDataTable> WaveSpawnTable;

	UPROPERTY(EditDefaultsOnly, Category="P4|Beta6|Data")
	TSoftObjectPtr<UDataTable> EnemyTable;

	UPROPERTY(EditDefaultsOnly, Category="P4|Beta6|Data")
	TSoftObjectPtr<UDataTable> BossTable;

	UPROPERTY(EditDefaultsOnly, Category="P4|Beta6|Data")
	TSoftObjectPtr<UDataTable> BossAttackTable;

	UPROPERTY(EditDefaultsOnly, Category="P4|Beta6|Data")
	TSoftObjectPtr<UDataTable> UpgradeTable;

	UPROPERTY(EditDefaultsOnly, Category="P4|Beta6|Run")
	int64 RunSeed = 0x50344236;

	UPROPERTY(EditDefaultsOnly, Category="P4|Beta6|Run")
	bool bResumeSavedRun = true;

	UPROPERTY(EditDefaultsOnly, Category="P4|Beta6|Preview", meta=(ClampMin="185.0"))
	float PreviewArenaDepth = 420.0f;

	UPROPERTY(EditDefaultsOnly, Category="P4|Beta6|Preview", meta=(ClampMin="1.0", ClampMax="89.0"))
	float PreviewCameraPitch = 50.0f;
};
