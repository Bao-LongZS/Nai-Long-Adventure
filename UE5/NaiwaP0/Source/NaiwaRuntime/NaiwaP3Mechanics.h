#pragma once

#include "CoreMinimal.h"
#include "NaiwaP3M2SeededMap.h"

// 固定 0.1 秒 tick 下的 M2 机关状态。网格/角色系统负责执行返回的移动和伤害结果。
class NAIWARUNTIME_API FNaiwaP3MechanicsRuntime final
{
public:
	void Initialize(const TArray<FNaiwaP3M2MechanicPlacement>& InMechanics);
	void AdvanceTick(int32 CurrentTick, const TArray<FIntPoint>& OccupiedCells);

	bool TryTeleport(const FIntPoint& Source, uint64 ActorId, int32 CurrentTick, FIntPoint& OutDestination);
	bool IsGateOpen(const FIntPoint& Cell, int32 CurrentTick) const;
	bool TryGetConveyorDirection(const FIntPoint& Cell, FIntPoint& OutDirection) const;
	bool IsSpikeActive(const FIntPoint& Cell, int32 CurrentTick) const;
	bool ShouldForceBombDetonation(const FIntPoint& Cell, int32 CurrentTick) const;
	void RegisterFireAtCell(const FIntPoint& Cell, int32 CurrentTick);
	bool IsHole(const FIntPoint& Cell, int32 CurrentTick) const;
	bool ShouldConsumeFire(const FIntPoint& Cell, int32 CurrentTick) const;
	bool ShouldDestroyBomb(const FIntPoint& Cell, int32 CurrentTick) const;
	bool BlocksMovement(const FIntPoint& Cell, int32 CurrentTick) const;

private:
	const FNaiwaP3M2MechanicPlacement* FindMechanic(
		const FIntPoint& Cell, ENaiwaP3M2MechanicKind Kind) const;
	int32 SecondsToTicks(float Seconds) const;

	TArray<FNaiwaP3M2MechanicPlacement> Mechanics;
	TMap<uint64, int32> PortalCooldownUntilTick;
	TMap<int32, int32> GateOpenUntilTick;
	TMap<int32, bool> PressurePlateWasOccupied;
	TMap<FIntPoint, int32> CrackedFloorBurnCount;
	TMap<FIntPoint, int32> CrackedFloorCollapseTick;
	TMap<FIntPoint, int32> HoleExpireTick;

	int32 PortalCooldownTicks = 20;
	int32 GateCloseDelayTicks = 15;
	int32 SpikeCycleTicks = 30;
	int32 SpikeActiveTicks = 10;
	int32 CrackedFloorBurnsToCollapse = 2;
	int32 CrackedFloorCollapseDelayTicks = 10;
	int32 CrackedFloorHoleTicks = 100;
};
