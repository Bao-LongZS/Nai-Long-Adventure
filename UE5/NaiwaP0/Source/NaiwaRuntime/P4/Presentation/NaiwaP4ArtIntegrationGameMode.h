#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NaiwaP4ArtIntegrationGameMode.generated.h"

/** Isolated map GameMode for reviewing the approved A1/A2/A3 integration slice. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP4ArtIntegrationGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANaiwaP4ArtIntegrationGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	float GetPreviewArenaDepth() const { return PreviewArenaDepth; }
	float GetPreviewCameraPitch() const { return PreviewCameraPitch; }

private:
	UPROPERTY(EditDefaultsOnly, Category="P4|Preview", meta=(ClampMin="185.0"))
	float PreviewArenaDepth = 420.0f;

	UPROPERTY(EditDefaultsOnly, Category="P4|Preview", meta=(ClampMin="1.0", ClampMax="89.0"))
	float PreviewCameraPitch = 50.0f;
};
