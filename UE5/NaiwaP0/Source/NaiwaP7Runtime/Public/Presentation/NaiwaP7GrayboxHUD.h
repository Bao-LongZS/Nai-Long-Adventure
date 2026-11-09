#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NaiwaP7GrayboxHUD.generated.h"

/** Temporary, read-only Canvas renderer for validating the P7 board and HUD layout. */
UCLASS()
class NAIWAP7RUNTIME_API ANaiwaP7GrayboxHUD final : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

    /** Fits the 1920x1080 reference layout while preserving the 20x11 board's 96x52.8 cell ratio. */
    static bool TryCalculateBoardRect(
        const FVector2D& ViewportSize,
        FVector2D& OutOrigin,
        FVector2D& OutSize);
};
