#include "NaiwaP4ArenaSubsystem.h"

#include "NaiwaP4Determinism.h"
#include "NaiwaP4Settings.h"
#include "Engine/DataTable.h"

void UNaiwaP4ArenaSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UNaiwaP4Settings* Settings = GetDefault<UNaiwaP4Settings>();
	if (Settings)
	{
		FixedStepSeconds = FMath::Clamp(Settings->FixedStepSeconds, 0.001, 0.05);
		MaxCatchUpSteps = FMath::Clamp(Settings->MaxCatchUpSteps, 1, 8);
		LogicalArenaWidth = FMath::Max(100.0, static_cast<double>(Settings->LogicalArenaWidth));
		LogicalArenaDepth = FMath::Max(50.0, static_cast<double>(Settings->LogicalArenaDepth));
		PlayerWalkSpeed = FMath::Max(0.0, static_cast<double>(Settings->WalkSpeed));
		PlayerDepthSpeedScale = FMath::Clamp(static_cast<double>(Settings->DepthSpeedScale), 0.0, 1.0);
		PlayerBlockMoveSpeedScale = FMath::Clamp(static_cast<double>(Settings->BlockMoveSpeedScale), 0.0, 1.0);
		PlayerMaxHP = FMath::Max(1, Settings->PlayerMaxHP);
	}
	State.Reset(0);
}

void UNaiwaP4ArenaSubsystem::Deinitialize()
{
	bSimulationActive = false;
	Super::Deinitialize();
}

void UNaiwaP4ArenaSubsystem::Tick(const float DeltaTime)
{
	if (!bSimulationActive || DeltaTime <= 0.0f || FixedStepSeconds <= 0.0)
	{
		return;
	}

	FixedAccumulatorSeconds += static_cast<double>(DeltaTime);
	int32 Steps = 0;
	// Bound work per frame without discarding simulation time; later ticks drain the backlog.
	while (FixedAccumulatorSeconds >= FixedStepSeconds && Steps < MaxCatchUpSteps)
	{
		FixedAccumulatorSeconds = FMath::Max(0.0, FixedAccumulatorSeconds - FixedStepSeconds);
		AdvanceFixedStep();
		++Steps;
	}
}

TStatId UNaiwaP4ArenaSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNaiwaP4ArenaSubsystem, STATGROUP_Tickables);
}

void UNaiwaP4ArenaSubsystem::StartSimulation(const uint32 MasterSeed)
{
	State.Reset(MasterSeed);
	FNaiwaP4EntityState Player;
	Player.EntityId = State.NextEntityId++;
	Player.Kind = ENaiwaP4EntityKind::Player;
	Player.Position = FVector2D(LogicalArenaWidth * 0.5, LogicalArenaDepth * 0.5);
	Player.HP = PlayerMaxHP;
	Player.Facing = ENaiwaP4Facing::Side;
	State.Entities.Add(Player);
	InputBuffer.Reset();
	LastConsumedPlayerCommand = FNaiwaP4InputCommand{};
	FixedAccumulatorSeconds = 0.0;
	bSimulationActive = true;
}

void UNaiwaP4ArenaSubsystem::StopSimulation()
{
	bSimulationActive = false;
	ClearPlayerInput();
	FixedAccumulatorSeconds = 0.0;
}

void UNaiwaP4ArenaSubsystem::ClearPlayerInput()
{
	InputBuffer.Reset();
	LastConsumedPlayerCommand = FNaiwaP4InputCommand{};
}

void UNaiwaP4ArenaSubsystem::SetPlayerMoveAxis(const FVector2D Axis)
{
	if (bSimulationActive)
	{
		InputBuffer.SetMoveAxis(Axis);
	}
}

void UNaiwaP4ArenaSubsystem::SetPlayerAttackHeld(const bool bHeld)
{
	if (bSimulationActive)
	{
		InputBuffer.SetAttackHeld(bHeld);
	}
}

void UNaiwaP4ArenaSubsystem::QueuePlayerDashPressed()
{
	if (bSimulationActive)
	{
		InputBuffer.QueueDashPressed();
	}
}

void UNaiwaP4ArenaSubsystem::SetPlayerBlockHeld(const bool bHeld)
{
	if (bSimulationActive)
	{
		InputBuffer.SetBlockHeld(bHeld);
	}
}

void UNaiwaP4ArenaSubsystem::QueuePlayerUltimatePressed()
{
	if (bSimulationActive)
	{
		InputBuffer.QueueUltimatePressed();
	}
}

int64 UNaiwaP4ArenaSubsystem::GetPendingCatchUpSteps() const
{
	if (FixedStepSeconds <= 0.0 || FixedAccumulatorSeconds < FixedStepSeconds)
	{
		return 0;
	}

	const double PendingSteps = FMath::FloorToDouble(FixedAccumulatorSeconds / FixedStepSeconds);
	return PendingSteps >= static_cast<double>(MAX_int64)
		? MAX_int64
		: static_cast<int64>(PendingSteps);
}

bool UNaiwaP4ArenaSubsystem::LoadUpgradeDataTable(const UDataTable* DataTable, FString& OutError)
{
	return UpgradeCatalog.Load(DataTable, OutError);
}

uint64 UNaiwaP4ArenaSubsystem::GetSimulationStateHash() const
{
	return FNaiwaP4Determinism::ComputeSimulationStateHashV1(State);
}

void UNaiwaP4ArenaSubsystem::AdvanceFixedStep()
{
	LastConsumedPlayerCommand = InputBuffer.ConsumeForFixedStep();
	AdvancePlayerMovement(LastConsumedPlayerCommand);
	++State.CurrentTick;
}

void UNaiwaP4ArenaSubsystem::AdvancePlayerMovement(const FNaiwaP4InputCommand& Command)
{
	FNaiwaP4EntityState* Player = State.Entities.FindByPredicate([](const FNaiwaP4EntityState& Entity)
	{
		return Entity.Kind == ENaiwaP4EntityKind::Player;
	});
	if (!Player || !Player->bAlive)
	{
		return;
	}

	const double SpeedScale = Command.bBlockHeld ? PlayerBlockMoveSpeedScale : 1.0;
	const FVector2D RequestedVelocity(
		Command.MoveAxis.X * PlayerWalkSpeed * SpeedScale,
		Command.MoveAxis.Y * PlayerWalkSpeed * PlayerDepthSpeedScale * SpeedScale);
	const FVector2D PreviousPosition = Player->Position;
	Player->Position.X = FMath::Clamp(
		Player->Position.X + RequestedVelocity.X * FixedStepSeconds, 0.0, LogicalArenaWidth);
	Player->Position.Y = FMath::Clamp(
		Player->Position.Y + RequestedVelocity.Y * FixedStepSeconds, 0.0, LogicalArenaDepth);
	Player->Velocity = (Player->Position - PreviousPosition) / FixedStepSeconds;

	if (!Command.MoveAxis.IsNearlyZero())
	{
		const double Horizontal = FMath::Abs(Command.MoveAxis.X);
		const double Vertical = FMath::Abs(Command.MoveAxis.Y);
		if (Horizontal >= Vertical)
		{
			Player->Facing = ENaiwaP4Facing::Side;
			Player->bFacingRight = Command.MoveAxis.X > 0.0;
		}
		else
		{
			Player->Facing = Command.MoveAxis.Y > 0.0
				? ENaiwaP4Facing::Back
				: ENaiwaP4Facing::Front;
		}
	}

	const ENaiwaP4ActionCue NextCue = Command.bBlockHeld
		? ENaiwaP4ActionCue::Defend
		: (Command.MoveAxis.IsNearlyZero() ? ENaiwaP4ActionCue::Idle : ENaiwaP4ActionCue::Move);
	if (Player->ActionCue != NextCue)
	{
		Player->ActionTick = 0;
		Player->ActionCue = NextCue;
	}
	else if (Player->ActionTick < MAX_int32)
	{
		++Player->ActionTick;
	}
}
