#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NaiwaP2HUD.generated.h"

class UTexture2D;

UCLASS()
class NAIWARUNTIME_API ANaiwaP2HUD : public AHUD
{
    GENERATED_BODY()

public:
    ANaiwaP2HUD();
    virtual void DrawHUD() override;

private:
    void DrawCenteredLabel(const FString& Text, float CenterX, float Y, float Scale,
        const FLinearColor& Color);

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> OneMinuteFeedbackImage = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> FifthResetFeedbackImage = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> OutOfEggsFeedbackImage = nullptr;
};
