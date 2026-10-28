#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NaiwaP5PrototypeGameMode.generated.h"

UCLASS()
class NAIWARUNTIME_API ANaiwaP5PrototypeGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANaiwaP5PrototypeGameMode();
	virtual void BeginPlay() override;
};
