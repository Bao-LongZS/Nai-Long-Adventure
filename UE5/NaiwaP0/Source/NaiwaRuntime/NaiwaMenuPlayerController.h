#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NaiwaMenuPlayerController.generated.h"

class UNaiwaMenuWidget;
class UAudioComponent;
class USoundBase;

UCLASS()
class NAIWARUNTIME_API ANaiwaMenuPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ANaiwaMenuPlayerController();
    virtual void BeginPlay() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UNaiwaMenuWidget> MenuWidget;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> MenuMusicComponent;

    UPROPERTY()
    TObjectPtr<USoundBase> MenuMusic;
};
