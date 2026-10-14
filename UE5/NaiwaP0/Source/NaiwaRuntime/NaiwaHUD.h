#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NaiwaHUD.generated.h"

/** Keyboard help and a visible pause state without changing input focus or gameplay. */
UCLASS()
class NAIWARUNTIME_API ANaiwaHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

private:
    void DrawCenteredLabel(const FString& Text, float CenterX, float Y, float Scale,
        const FLinearColor& Color);
};
