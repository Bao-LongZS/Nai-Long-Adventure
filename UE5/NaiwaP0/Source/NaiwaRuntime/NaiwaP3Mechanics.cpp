#include "NaiwaP3Mechanics.h"

#include "NaiwaP3M2MechanicsSettings.h"

void FNaiwaP3MechanicsRuntime::Initialize(const TArray<FNaiwaP3M2MechanicPlacement>& InMechanics)
{
	Mechanics = InMechanics;
	PortalCooldownUntilTick.Reset();
	GateOpenUntilTick.Reset();
	PressurePlateWasOccupied.Reset();
	CrackedFloorBurnCount.Reset();
	CrackedFloorCollapseTick.Reset();
	HoleExpireTick.Reset();

	const UNaiwaP3M2MechanicsSettings* Settings = GetDefault<UNaiwaP3M2MechanicsSettings>();
	if (!Settings)
	{
		return;
	}
	PortalCooldownTicks = SecondsToTicks(Settings->PortalCooldownSeconds);
	GateCloseDelayTicks = SecondsToTicks(Settings->GateCloseDelaySeconds);
	SpikeCycleTicks = SecondsToTicks(Settings->SpikeCycleSeconds);
	SpikeActiveTicks = FMath::Clamp(SecondsToTicks(Settings->SpikeActiveSeconds), 1, SpikeCycleTicks);
	CrackedFloorBurnsToCollapse = FMath::Clamp(Settings->CrackedFloorBurnsToCollapse, 1, 5);
	CrackedFloorCollapseDelayTicks = SecondsToTicks(Settings->CrackedFloorCollapseDelaySeconds);
	CrackedFloorHoleTicks = SecondsToTicks(Settings->CrackedFloorHoleSeconds);
}

void FNaiwaP3MechanicsRuntime::AdvanceTick(const int32 CurrentTick, const TArray<FIntPoint>& OccupiedCells)
{
	for (const FNaiwaP3M2MechanicPlacement& Mechanic : Mechanics)
	{
		if (Mechanic.Kind != ENaiwaP3M2MechanicKind::PressurePlate)
		{
			continue;
		}

		const bool bOccupied = OccupiedCells.Contains(Mechanic.Cell);
		const bool bWasOccupied = PressurePlateWasOccupied.FindRef(Mechanic.Variant);
		if (bOccupied)
		{
			GateOpenUntilTick.FindOrAdd(Mechanic.Variant) = MAX_int32;
		}
		else if (bWasOccupied)
		{
			GateOpenUntilTick.FindOrAdd(Mechanic.Variant) = CurrentTick + GateCloseDelayTicks;
		}
		PressurePlateWasOccupied.Add(Mechanic.Variant, bOccupied);
	}

	TArray<FIntPoint> CollapsedCells;
	for (const TPair<FIntPoint, int32>& Pending : CrackedFloorCollapseTick)
	{
		if (Pending.Value <= CurrentTick)
		{
			CollapsedCells.Add(Pending.Key);
		}
	}
	CollapsedCells.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return A.Y == B.Y ? A.X < B.X : A.Y < B.Y;
	});
	for (const FIntPoint& Cell : CollapsedCells)
	{
		CrackedFloorCollapseTick.Remove(Cell);
		HoleExpireTick.Add(Cell, CurrentTick + CrackedFloorHoleTicks);
	}

	TArray<FIntPoint> ExpiredHoles;
	for (const TPair<FIntPoint, int32>& Hole : HoleExpireTick)
	{
		if (Hole.Value <= CurrentTick)
		{
			ExpiredHoles.Add(Hole.Key);
		}
	}
	for (const FIntPoint& Cell : ExpiredHoles)
	{
		HoleExpireTick.Remove(Cell);
		CrackedFloorBurnCount.Remove(Cell);
	}
}

bool FNaiwaP3MechanicsRuntime::TryTeleport(const FIntPoint& Source, const uint64 ActorId,
	const int32 CurrentTick, FIntPoint& OutDestination)
{
	const FNaiwaP3M2MechanicPlacement* SourcePortal = FindMechanic(Source, ENaiwaP3M2MechanicKind::Portal);
	if (!SourcePortal || PortalCooldownUntilTick.FindRef(ActorId) > CurrentTick)
	{
		return false;
	}

	for (const FNaiwaP3M2MechanicPlacement& Candidate : Mechanics)
	{
		if (Candidate.Kind == ENaiwaP3M2MechanicKind::Portal &&
			Candidate.Variant == SourcePortal->Variant && Candidate.Cell != Source)
		{
			OutDestination = Candidate.Cell;
			PortalCooldownUntilTick.Add(ActorId, CurrentTick + PortalCooldownTicks);
			return true;
		}
	}
	return false;
}

bool FNaiwaP3MechanicsRuntime::IsGateOpen(const FIntPoint& Cell, const int32 CurrentTick) const
{
	const FNaiwaP3M2MechanicPlacement* Gate = FindMechanic(Cell, ENaiwaP3M2MechanicKind::VaultGate);
	if (!Gate)
	{
		return false;
	}
	const int32* OpenUntil = GateOpenUntilTick.Find(Gate->Variant);
	return OpenUntil && *OpenUntil > CurrentTick;
}

bool FNaiwaP3MechanicsRuntime::TryGetConveyorDirection(
	const FIntPoint& Cell, FIntPoint& OutDirection) const
{
	const FNaiwaP3M2MechanicPlacement* Conveyor = FindMechanic(Cell, ENaiwaP3M2MechanicKind::Conveyor);
	if (!Conveyor || Conveyor->Variant == 0)
	{
		return false;
	}
	OutDirection = FIntPoint(Conveyor->Variant < 0 ? -1 : 1, 0);
	return true;
}

bool FNaiwaP3MechanicsRuntime::IsSpikeActive(const FIntPoint& Cell, const int32 CurrentTick) const
{
	if (!FindMechanic(Cell, ENaiwaP3M2MechanicKind::Spike) || SpikeCycleTicks <= 0)
	{
		return false;
	}
	const int32 Phase = ((CurrentTick % SpikeCycleTicks) + SpikeCycleTicks) % SpikeCycleTicks;
	return Phase < SpikeActiveTicks;
}

bool FNaiwaP3MechanicsRuntime::ShouldForceBombDetonation(
	const FIntPoint& Cell, const int32 CurrentTick) const
{
	return IsSpikeActive(Cell, CurrentTick);
}

void FNaiwaP3MechanicsRuntime::RegisterFireAtCell(const FIntPoint& Cell, const int32 CurrentTick)
{
	if (!FindMechanic(Cell, ENaiwaP3M2MechanicKind::CrackedFloor) || IsHole(Cell, CurrentTick) ||
		CrackedFloorCollapseTick.Contains(Cell))
	{
		return;
	}

	const int32 NewBurnCount = CrackedFloorBurnCount.FindOrAdd(Cell) + 1;
	CrackedFloorBurnCount.Add(Cell, NewBurnCount);
	if (NewBurnCount >= CrackedFloorBurnsToCollapse)
	{
		CrackedFloorCollapseTick.Add(Cell, CurrentTick + CrackedFloorCollapseDelayTicks);
	}
}

bool FNaiwaP3MechanicsRuntime::IsHole(const FIntPoint& Cell, const int32 CurrentTick) const
{
	const int32* ExpireTick = HoleExpireTick.Find(Cell);
	return ExpireTick && *ExpireTick > CurrentTick;
}

bool FNaiwaP3MechanicsRuntime::ShouldConsumeFire(const FIntPoint& Cell, const int32 CurrentTick) const
{
	return IsHole(Cell, CurrentTick);
}

bool FNaiwaP3MechanicsRuntime::ShouldDestroyBomb(const FIntPoint& Cell, const int32 CurrentTick) const
{
	return IsHole(Cell, CurrentTick);
}

bool FNaiwaP3MechanicsRuntime::BlocksMovement(const FIntPoint& Cell, const int32 CurrentTick) const
{
	if (IsHole(Cell, CurrentTick))
	{
		return true;
	}
	const FNaiwaP3M2MechanicPlacement* Gate = FindMechanic(Cell, ENaiwaP3M2MechanicKind::VaultGate);
	return Gate && !IsGateOpen(Cell, CurrentTick);
}

const FNaiwaP3M2MechanicPlacement* FNaiwaP3MechanicsRuntime::FindMechanic(
	const FIntPoint& Cell, const ENaiwaP3M2MechanicKind Kind) const
{
	return Mechanics.FindByPredicate([Cell, Kind](const FNaiwaP3M2MechanicPlacement& Placement)
	{
		return Placement.Kind == Kind && Placement.Cell == Cell;
	});
}

int32 FNaiwaP3MechanicsRuntime::SecondsToTicks(const float Seconds) const
{
	return FMath::Max(1, FMath::RoundToInt(FMath::Max(Seconds, 0.1f) / 0.1f));
}
