#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/StreamableManager.h"
#include "NaiwaP4W1PreviewGameMode.generated.h"

class UDataTable;

/** Minimal, pawn-free GameMode for the isolated W1 camera/art calibration map. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP4W1PreviewGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANaiwaP4W1PreviewGameMode();
	virtual void StartPlay() override;

private:
	UPROPERTY()
	TSoftObjectPtr<UDataTable> UpgradeDataTable;
};
