#pragma once

#include "CoreMinimal.h"
#include "NaiwaP3LootTable.h"

// M2 玩家成长与限时状态。逻辑使用统一的 0.1 秒 tick，由关卡运行时调用。
struct NAIWARUNTIME_API FNaiwaP3ItemEffects final
{
	static constexpr int32 TicksPerSecond = 10;
	static constexpr int32 ChickenJumpCooldownTicks = 8 * TicksPerSecond;
	static constexpr int32 TimedItemDurationTicks = 3 * TicksPerSecond;
	static constexpr int32 MaxBombCapacity = 3;
	static constexpr int32 MaxBombRange = 4;
	static constexpr int32 MaxShieldCharges = 3;
	static constexpr float SpeedUpgradeStep = 0.15f;
	static constexpr float MaxSpeedMultiplier = 1.60f;

	void Reset();
	FString Collect(ENaiwaP3LootType Loot, int32 CurrentTick);

	bool TryUseChickenJump(int32 CurrentTick, const FIntPoint& TargetCell,
		bool bTargetCellEnterable, FIntPoint& OutTargetCell);
	FIntPoint ApplyControlEffects(const FIntPoint& Direction, int32 CurrentTick) const;
	bool ConsumeExplosionShield();

	bool IsChickenJumpUnlocked() const { return bChickenJumpUnlocked; }
	bool IsChickenJumpReady(int32 CurrentTick) const;
	bool IsXrayActive(int32 CurrentTick) const { return XrayUntilTick > CurrentTick; }
	bool IsChaosReverseActive(int32 CurrentTick) const { return ChaosReverseUntilTick > CurrentTick; }
	int32 GetBombCapacity() const { return BombCapacity; }
	int32 GetBombRange() const { return BombRange; }
	int32 GetShieldCharges() const { return ShieldCharges; }
	float GetSpeedMultiplier() const { return SpeedMultiplier; }

private:
	int32 BombCapacity = 1;
	int32 BombRange = 2;
	int32 ShieldCharges = 0;
	int32 ChickenJumpReadyTick = 0;
	int32 XrayUntilTick = 0;
	int32 ChaosReverseUntilTick = 0;
	float SpeedMultiplier = 1.0f;
	bool bChickenJumpUnlocked = false;
};
