#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NaiwaP5HUD.generated.h"

UCLASS()
class NAIWARUNTIME_API ANaiwaP5HUD final : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
