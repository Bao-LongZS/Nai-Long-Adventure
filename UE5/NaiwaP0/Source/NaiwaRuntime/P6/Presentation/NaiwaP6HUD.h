#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NaiwaP6HUD.generated.h"

UCLASS()
class NAIWARUNTIME_API ANaiwaP6HUD final : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
