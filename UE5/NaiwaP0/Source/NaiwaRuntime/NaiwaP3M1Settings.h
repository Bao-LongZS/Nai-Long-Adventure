#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NaiwaP3M1Settings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="P3 M1 手感测试"))
class NAIWARUNTIME_API UNaiwaP3M1Settings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category="移动", meta=(DisplayName="玩家速度（格/秒）", ClampMin="0.5", ClampMax="10.0", UIMin="1.0", UIMax="8.0"))
	float PlayerCellsPerSecond = 4.0f;

	UPROPERTY(Config, EditAnywhere, Category="巡逻", meta=(DisplayName="奶察速度（格/秒）", ClampMin="0.1", ClampMax="8.0", UIMin="0.5", UIMax="4.0"))
	float PatrolCellsPerSecond = 2.0f;

	UPROPERTY(Config, EditAnywhere, Category="炸弹", meta=(DisplayName="炸弹容量", ClampMin="1", ClampMax="5", UIMin="1", UIMax="3"))
	int32 BombCapacity = 1;

	UPROPERTY(Config, EditAnywhere, Category="炸弹", meta=(DisplayName="放弹间隔（秒）", ClampMin="0.1", ClampMax="2.0", UIMin="0.1", UIMax="1.0"))
	float BombPlacementIntervalSeconds = 0.4f;

	UPROPERTY(Config, EditAnywhere, Category="炸弹", meta=(DisplayName="十字臂长（格）", ClampMin="1", ClampMax="8", UIMin="1", UIMax="5"))
	int32 BombRange = 1;

	UPROPERTY(Config, EditAnywhere, Category="炸弹", meta=(DisplayName="引爆时间（秒）", ClampMin="0.5", ClampMax="8.0", UIMin="1.0", UIMax="5.0"))
	float BombFuseSeconds = 2.5f;

	UPROPERTY(Config, EditAnywhere, Category="炸弹", meta=(DisplayName="连锁延迟（秒）", ClampMin="0.1", ClampMax="1.0", UIMin="0.1", UIMax="0.5"))
	float ChainDelaySeconds = 0.2f;

	UPROPERTY(Config, EditAnywhere, Category="炸弹", meta=(DisplayName="火焰时长（秒）", ClampMin="0.1", ClampMax="2.0", UIMin="0.1", UIMax="1.0"))
	float FlameDurationSeconds = 0.3f;
};
