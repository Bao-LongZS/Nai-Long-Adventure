#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NaiwaP3M2Settings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="P3 3-2 地图生成"))
class NAIWARUNTIME_API UNaiwaP3M2Settings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category="炸弹", meta=(DisplayName="玩家放弹间隔（秒）", ClampMin="0.1", ClampMax="2.0", UIMin="0.1", UIMax="1.0"))
	float BombPlacementIntervalSeconds = 0.4f;

	UPROPERTY(Config, EditAnywhere, Category="软砖", meta=(DisplayName="外环软砖率", ClampMin="0.0", ClampMax="1.0", UIMin="0.1", UIMax="0.8"))
	float OuterSoftBrickRatio = 0.45f;

	UPROPERTY(Config, EditAnywhere, Category="软砖", meta=(DisplayName="机关带软砖率", ClampMin="0.0", ClampMax="1.0", UIMin="0.1", UIMax="0.8"))
	float BeltSoftBrickRatio = 0.40f;

	UPROPERTY(Config, EditAnywhere, Category="软砖", meta=(DisplayName="金库区软砖率", ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="0.6"))
	float VaultSoftBrickRatio = 0.30f;

	UPROPERTY(Config, EditAnywhere, Category="软砖", meta=(DisplayName="道具砖占软砖比例", ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="0.5"))
	float ItemBrickRatio = 0.20f;

	UPROPERTY(Config, EditAnywhere, Category="校验", meta=(DisplayName="候选地图最大重试次数", ClampMin="1", ClampMax="64", UIMin="1", UIMax="64"))
	int32 MaxGenerationAttempts = 64;
};
