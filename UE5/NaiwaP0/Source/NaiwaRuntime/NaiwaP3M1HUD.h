#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NaiwaP3M1HUD.generated.h"

UCLASS()
class NAIWARUNTIME_API ANaiwaP3M1HUD final : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& Accent);
	void DrawLabel(const FString& Text, float X, float Y, float Scale, const FLinearColor& Color);
};
