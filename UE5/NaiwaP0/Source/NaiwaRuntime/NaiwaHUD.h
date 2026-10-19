#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NaiwaHUD.generated.h"

class UTexture2D;

/** Keyboard help and a visible pause state without changing input focus or gameplay. */
UCLASS()
class NAIWARUNTIME_API ANaiwaHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void DrawHUD() override;

private:
    void LoadDeathLaughFrames();

    void DrawCenteredLabel(const FString& Text, float CenterX, float Y, float Scale,
        const FLinearColor& Color);

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> DeathLaughFrameTextures;
};
