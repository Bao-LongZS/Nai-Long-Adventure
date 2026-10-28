#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NaiwaP5M2PrototypeGameMode.generated.h"

UCLASS()
class NAIWARUNTIME_API ANaiwaP5M2PrototypeGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANaiwaP5M2PrototypeGameMode();
	virtual void BeginPlay() override;
};
