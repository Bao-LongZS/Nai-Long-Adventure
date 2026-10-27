#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NaiwaP3M2MechanicsSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="P3 M2 机关参数"))
class NAIWARUNTIME_API UNaiwaP3M2MechanicsSettings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category="传送门", meta=(DisplayName="传送冷却（秒）", ClampMin="0.1", ClampMax="10.0"))
	float PortalCooldownSeconds = 2.0f;

	UPROPERTY(Config, EditAnywhere, Category="踩板门", meta=(DisplayName="松开后关门延迟（秒）", ClampMin="0.1", ClampMax="5.0"))
	float GateCloseDelaySeconds = 1.5f;

	UPROPERTY(Config, EditAnywhere, Category="尖刺", meta=(DisplayName="尖刺周期（秒）", ClampMin="0.5", ClampMax="10.0"))
	float SpikeCycleSeconds = 3.0f;

	UPROPERTY(Config, EditAnywhere, Category="尖刺", meta=(DisplayName="尖刺弹出时长（秒）", ClampMin="0.1", ClampMax="5.0"))
	float SpikeActiveSeconds = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category="断裂地板", meta=(DisplayName="烧毁次数", ClampMin="1", ClampMax="5"))
	int32 CrackedFloorBurnsToCollapse = 2;

	UPROPERTY(Config, EditAnywhere, Category="断裂地板", meta=(DisplayName="烧毁后塌陷延迟（秒）", ClampMin="0.1", ClampMax="5.0"))
	float CrackedFloorCollapseDelaySeconds = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category="断裂地板", meta=(DisplayName="洞持续时间（秒）", ClampMin="1.0", ClampMax="30.0"))
	float CrackedFloorHoleSeconds = 10.0f;
};
