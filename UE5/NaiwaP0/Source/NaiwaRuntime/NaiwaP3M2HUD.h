#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NaiwaP3BombTypes.h"
#include "NaiwaP3M2HUD.generated.h"

class UTexture2D;
class UNaiwaP3M2RuntimeSubsystem;
class FNaiwaP3M2LevelSession;

UCLASS()
class NAIWARUNTIME_API ANaiwaP3M2HUD final : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawLabel(const FString& Text, float X, float Y, float Scale, const FLinearColor& Color);
	void DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& Accent);
	void DrawBombLoadout(float X, float Y, float Width, float Scale, ENaiwaP3BombType Selected,
		bool bTeachingLevel);
	void DrawMinimap(const FNaiwaP3M2LevelSession& Session,
		const UNaiwaP3M2RuntimeSubsystem& Runtime, float X, float Y, float MaxWidth,
		float MaxHeight, float Scale);

	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> BombIcons;
};
