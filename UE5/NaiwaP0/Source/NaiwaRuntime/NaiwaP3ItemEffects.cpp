#include "NaiwaP3ItemEffects.h"

void FNaiwaP3ItemEffects::Reset()
{
	BombCapacity = 1;
	BombRange = 2;
	ShieldCharges = 0;
	ChickenJumpReadyTick = 0;
	XrayUntilTick = 0;
	ChaosReverseUntilTick = 0;
	SpeedMultiplier = 1.0f;
	bChickenJumpUnlocked = false;
}

FString FNaiwaP3ItemEffects::Collect(const ENaiwaP3LootType Loot, const int32 CurrentTick)
{
	switch (Loot)
	{
	case ENaiwaP3LootType::BombCapacity:
		if (BombCapacity >= MaxBombCapacity)
		{
			return TEXT("炸弹上限已满");
		}
		++BombCapacity;
		return TEXT("炸弹上限 +1");

	case ENaiwaP3LootType::BombRange:
		if (BombRange >= MaxBombRange)
		{
			return TEXT("炸弹威力已满");
		}
		++BombRange;
		return TEXT("威力 +1");

	case ENaiwaP3LootType::ChickenJump:
		bChickenJumpUnlocked = true;
		return TEXT("获得奶鸡跳");

	case ENaiwaP3LootType::SpeedBoost:
		if (SpeedMultiplier >= MaxSpeedMultiplier)
		{
			return TEXT("移动速度已满");
		}
		{
			const float PreviousMultiplier = SpeedMultiplier;
			SpeedMultiplier = FMath::Min(MaxSpeedMultiplier, SpeedMultiplier + SpeedUpgradeStep);
			const int32 AppliedPercent = FMath::RoundToInt(
				(SpeedMultiplier - PreviousMultiplier) * 100.0f);
			return FString::Printf(TEXT("速度提升 %d%%"), AppliedPercent);
		}

	case ENaiwaP3LootType::Shield:
		if (ShieldCharges >= MaxShieldCharges)
		{
			return TEXT("奶罩护盾已满");
		}
		++ShieldCharges;
		return TEXT("获得奶罩护盾");

	case ENaiwaP3LootType::Xray:
		XrayUntilTick = CurrentTick + TimedItemDurationTicks;
		return TEXT("透视 3 秒");

	case ENaiwaP3LootType::ChaosReverse:
		ChaosReverseUntilTick = CurrentTick + TimedItemDurationTicks;
		return TEXT("方向反转 3 秒");

	default:
		return TEXT("未知道具");
	}
}

bool FNaiwaP3ItemEffects::TryUseChickenJump(const int32 CurrentTick,
	const FIntPoint& TargetCell, const bool bTargetCellEnterable, FIntPoint& OutTargetCell)
{
	if (!bChickenJumpUnlocked || CurrentTick < ChickenJumpReadyTick || !bTargetCellEnterable)
	{
		return false;
	}
	OutTargetCell = TargetCell;
	ChickenJumpReadyTick = CurrentTick + ChickenJumpCooldownTicks;
	return true;
}

FIntPoint FNaiwaP3ItemEffects::ApplyControlEffects(
	const FIntPoint& Direction, const int32 CurrentTick) const
{
	return IsChaosReverseActive(CurrentTick) ? FIntPoint(-Direction.X, -Direction.Y) : Direction;
}

bool FNaiwaP3ItemEffects::ConsumeExplosionShield()
{
	if (ShieldCharges <= 0)
	{
		return false;
	}
	--ShieldCharges;
	return true;
}

bool FNaiwaP3ItemEffects::IsChickenJumpReady(const int32 CurrentTick) const
{
	return bChickenJumpUnlocked && CurrentTick >= ChickenJumpReadyTick;
}
