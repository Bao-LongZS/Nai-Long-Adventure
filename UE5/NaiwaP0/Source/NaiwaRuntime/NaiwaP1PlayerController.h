#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NaiwaP1PlayerController.generated.h"

/** Selects the P1-only camera manager; input remains on ANaiwaCharacter. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP1PlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ANaiwaP1PlayerController();
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

private:
    void ReturnToLevelSelect();
};
