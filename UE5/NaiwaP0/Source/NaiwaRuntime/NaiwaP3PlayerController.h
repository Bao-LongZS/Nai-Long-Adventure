#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NaiwaP3PlayerController.generated.h"

/** Restores gameplay input when entering P3 after the menu's UI-only input mode. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP3PlayerController final : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
};
