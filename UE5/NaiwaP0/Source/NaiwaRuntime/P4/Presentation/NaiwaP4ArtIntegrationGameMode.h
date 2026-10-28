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
};
