#pragma once

#include "CoreMinimal.h"

enum class ENaiwaP3LootType : uint8
{
	BombCapacity,
	BombRange,
	ChickenJump,
	SpeedBoost,
	Shield,
	Xray,
	ChaosReverse
};

// 每张地图使用独立掉落随机流，拾取不会消耗地图或脚本 AI 的随机数。
class NAIWARUNTIME_API FNaiwaP3LootRandomStream final
{
public:
	explicit FNaiwaP3LootRandomStream(int32 InSeed) : RandomStream(InSeed) {}

	ENaiwaP3LootType RollDrop();
	ENaiwaP3LootType RollGuaranteedTeachingDrop() const { return ENaiwaP3LootType::BombCapacity; }
	uint32 GetDrawCount() const { return DrawCount; }

private:
	FRandomStream RandomStream;
	uint32 DrawCount = 0;
};

NAIWARUNTIME_API const TCHAR* GetNaiwaP3LootDisplayName(ENaiwaP3LootType Type);
