#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NaiwaP6PlayerController.generated.h"

UCLASS()
class NAIWARUNTIME_API ANaiwaP6PlayerController final : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
};
