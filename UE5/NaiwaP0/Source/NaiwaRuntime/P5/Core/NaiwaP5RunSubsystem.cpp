#include "NaiwaP5RunSubsystem.h"

#include "NaiwaP5Simulation.h"
#include "NaiwaP5TrackData.h"
#include "../Data/NaiwaP5SkinCatalog.h"
#include "../../NaiwaMenuProgress.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	constexpr int32 M1CoinPoints = NaiwaP5Rules::PointsPerCoin;

	uint64 HashByte64(uint64 Hash, const uint8 Byte)
	{
		return (Hash ^ Byte) * 1099511628211ull;
	}

	uint64 HashUInt32LE64(uint64 Hash, const uint32 Value)
	{
		for (uint32 Shift = 0; Shift < 32; Shift += 8)
		{
			Hash = HashByte64(Hash, static_cast<uint8>((Value >> Shift) & 0xFFu));
		}
		return Hash;
	}

	uint64 HashUInt64LE64(uint64 Hash, const uint64 Value)
	{
		for (uint32 Shift = 0; Shift < 64; Shift += 8)
		{
			Hash = HashByte64(Hash, static_cast<uint8>((Value >> Shift) & 0xFFull));
		}
		return Hash;
	}

	uint64 HashQuantizedDouble64(uint64 Hash, const double Value, const double Scale = 1000.0)
	{
		const int64 Quantized = FMath::RoundToInt64(Value * Scale);
		return HashUInt64LE64(Hash, static_cast<uint64>(Quantized));
	}

	uint64 HashBool64(uint64 Hash, const bool Value)
	{
		return HashByte64(Hash, Value ? 1u : 0u);
	}

	uint64 HashName64(uint64 Hash, const FName Name)
	{
		const FTCHARToUTF8 NameUtf8(*Name.ToString());
		for (int32 Index = 0; Index < NameUtf8.Length(); ++Index)
		{
			Hash = HashByte64(Hash, static_cast<uint8>(NameUtf8.Get()[Index]));
		}
		return Hash;
	}

	bool HasId(const TArray<int32>& Ids, const int32 Id)
	{
		return Ids.Contains(Id);
	}
}

void UNaiwaP5RunSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	State = FNaiwaP5RunState{};
	FixedAccumulatorSeconds = 0.0;
	MaxCatchUpSteps = NaiwaP5Rules::MaxCatchUpSteps;
}

void UNaiwaP5RunSubsystem::Deinitialize()
{
	State.Phase = ENaiwaP5RunPhase::NotStarted;
	State.DeferredCatchUpSteps = 0;
	PendingInput = FNaiwaP5InputCommand{};
	Super::Deinitialize();
}

void UNaiwaP5RunSubsystem::Tick(const float DeltaTime)
{
	if (State.Phase != ENaiwaP5RunPhase::Running || DeltaTime <= 0.0f)
	{
		return;
	}

	FixedAccumulatorSeconds += static_cast<double>(DeltaTime);
	int32 Steps = 0;
	while (FixedAccumulatorSeconds >= NaiwaP5Rules::FixedStepSeconds && Steps < MaxCatchUpSteps)
	{
		FixedAccumulatorSeconds -= NaiwaP5Rules::FixedStepSeconds;
		AdvanceFixedStep();
		++Steps;
		if (State.Phase != ENaiwaP5RunPhase::Running)
		{
			break;
		}
	}

	const double DeferredSteps = State.Phase == ENaiwaP5RunPhase::Running
		? FMath::FloorToDouble(FixedAccumulatorSeconds / NaiwaP5Rules::FixedStepSeconds)
		: 0.0;
	State.DeferredCatchUpSteps = static_cast<int32>(FMath::Clamp(
		DeferredSteps, 0.0, static_cast<double>(MAX_int32)));
}

TStatId UNaiwaP5RunSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNaiwaP5RunSubsystem, STATGROUP_Tickables);
}

void UNaiwaP5RunSubsystem::StartM1PrototypeRun(const uint32 MasterSeed)
{
	StartPrototypeRun(false, MasterSeed);
}

void UNaiwaP5RunSubsystem::StartM2PrototypeRun(const uint32 MasterSeed)
{
	StartPrototypeRun(true, MasterSeed);
}

void UNaiwaP5RunSubsystem::RestartPrototypeRun()
{
	const bool bRestartM2Route = State.bM2PrototypeRoute;
	const uint32 RestartSeed = State.MasterSeed;
	StartPrototypeRun(bRestartM2Route, RestartSeed);
}

const FNaiwaP5RouteFixture& UNaiwaP5RunSubsystem::GetActivePrototypeFixture() const
{
	return State.bM2PrototypeRoute
		? FNaiwaP5TrackData::GetM2PrototypeFixture()
		: FNaiwaP5TrackData::GetM1PrototypeFixture();
}

FString UNaiwaP5RunSubsystem::GetLastAbilityEventText() const
{
	if (State.AbilityEvents.IsEmpty())
	{
		return TEXT("ABILITY LOG  -");
	}
	const FNaiwaP5AbilityEvent& Event = State.AbilityEvents.Last();
	return FString::Printf(TEXT("ABILITY LOG  %s / %s"),
		*Event.AbilityId.ToString(), *Event.TriggerId.ToString());
}

void UNaiwaP5RunSubsystem::StartPrototypeRun(const bool bM2Route, const uint32 MasterSeed)
{
	State = FNaiwaP5RunState{};
	State.Phase = ENaiwaP5RunPhase::Running;
	State.MasterSeed = MasterSeed;
	State.bM2PrototypeRoute = bM2Route;
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (const UNaiwaProgressSubsystem* Progress = GameInstance->GetSubsystem<UNaiwaProgressSubsystem>())
			{
				State.EquippedSkinId = bM2Route
					? Progress->GetSelectedP5SkinId()
					: FName(TEXT("NaiwaSkin_Base"));
			}
		}
	}
	State.AvalancheGapMeters = NaiwaP5Rules::AvalancheGapMaxMeters;
	State.PlayerSpeedMetersPerSecond = FNaiwaP5TrackData::GetRouteSpeedMetersPerSecond(
		GetActivePrototypeFixture(), 0);
	FixedAccumulatorSeconds = 0.0;
	State.DeferredCatchUpSteps = 0;
	PendingGapPenaltyMeters = 0.0;
	PendingInput = FNaiwaP5InputCommand{};
	bJumpPressQueued = false;
	bJumpReleaseQueued = false;
}

void UNaiwaP5RunSubsystem::StopRun()
{
	State.Phase = ENaiwaP5RunPhase::NotStarted;
	FixedAccumulatorSeconds = 0.0;
	State.DeferredCatchUpSteps = 0;
	PendingInput = FNaiwaP5InputCommand{};
	bJumpPressQueued = false;
	bJumpReleaseQueued = false;
}

void UNaiwaP5RunSubsystem::SetInputCommand(const FNaiwaP5InputCommand& Command)
{
	if (State.Phase != ENaiwaP5RunPhase::Running && State.Phase != ENaiwaP5RunPhase::Paused)
	{
		return;
	}

	bJumpPressQueued = bJumpPressQueued || Command.bJumpPressed;
	bJumpReleaseQueued = bJumpReleaseQueued || Command.bJumpReleased;
	PendingInput.bJumpHeld = Command.bJumpHeld;
	PendingInput.bRotateLeft = Command.bRotateLeft;
	PendingInput.bRotateRight = Command.bRotateRight;
}

void UNaiwaP5RunSubsystem::SetPaused(const bool bPaused)
{
	if (bPaused && State.Phase == ENaiwaP5RunPhase::Running)
	{
		State.Phase = ENaiwaP5RunPhase::Paused;
	}
	else if (!bPaused && State.Phase == ENaiwaP5RunPhase::Paused)
	{
		State.Phase = ENaiwaP5RunPhase::Running;
	}
}

void UNaiwaP5RunSubsystem::NotifySnowContact(const bool bOnSnow)
{
	if (State.Phase != ENaiwaP5RunPhase::Running)
	{
		return;
	}
	if (bOnSnow)
	{
		State.bOnSnow = true;
		State.CoyoteElapsedSeconds = 0.0;
	}
	else if (State.bOnSnow)
	{
		State.bOnSnow = false;
		State.CoyoteElapsedSeconds = 0.0;
	}
}

uint64 UNaiwaP5RunSubsystem::GetSimulationStateHashV4() const
{
	uint64 Hash = 14695981039346656037ull;
	Hash = HashUInt32LE64(Hash, 4u);
	Hash = HashUInt32LE64(Hash, FNaiwaP5TrackData::GetM0BaselineFixture().FixtureHash);
	Hash = HashUInt32LE64(Hash, GetActivePrototypeFixture().FixtureHash);
	Hash = HashUInt32LE64(Hash, static_cast<uint32>(FNaiwaP5SkinCatalog::CatalogVersion));
	Hash = HashBool64(Hash, State.bM2PrototypeRoute);
	Hash = HashUInt32LE64(Hash, static_cast<uint32>(State.Phase));
	Hash = HashUInt64LE64(Hash, static_cast<uint64>(State.CurrentTick));
	Hash = HashUInt32LE64(Hash, State.MasterSeed);
	Hash = HashQuantizedDouble64(Hash, State.ElapsedSeconds, 1000000.0);
	Hash = HashQuantizedDouble64(Hash, State.DistanceMeters);
	Hash = HashQuantizedDouble64(Hash, State.PlayerSpeedMetersPerSecond);
	Hash = HashQuantizedDouble64(Hash, State.AvalancheGapMeters);
	Hash = HashQuantizedDouble64(Hash, State.JumpChargeSeconds, 1000000.0);
	Hash = HashQuantizedDouble64(Hash, State.JumpElapsedSeconds, 1000000.0);
	Hash = HashQuantizedDouble64(Hash, State.JumpHeightMeters);
	Hash = HashQuantizedDouble64(Hash, State.JumpVerticalVelocityMetersPerSecond);
	Hash = HashQuantizedDouble64(Hash, State.CoyoteElapsedSeconds, 1000000.0);
	Hash = HashQuantizedDouble64(Hash, State.RotationDegrees);
	Hash = HashQuantizedDouble64(Hash, State.FallElapsedSeconds, 1000000.0);
	Hash = HashQuantizedDouble64(Hash, State.ComboMultiplier, 10000.0);
	Hash = HashQuantizedDouble64(Hash, State.LastSuccessfulActionSeconds, 1000000.0);
	Hash = HashName64(Hash, State.EquippedSkinId);
	Hash = HashQuantizedDouble64(Hash, State.LandingSpeedBoostSeconds, 1000000.0);
	Hash = HashQuantizedDouble64(Hash, State.TrickBoostSeconds, 1000000.0);
	Hash = HashUInt32LE64(Hash, static_cast<uint32>(State.TrickBoostLevel));
	Hash = HashQuantizedDouble64(Hash, State.AvalanchePushbackCooldownSeconds, 1000000.0);
	Hash = HashQuantizedDouble64(Hash, State.RideElapsedSeconds, 1000000.0);
	Hash = HashQuantizedDouble64(Hash, State.RideStartDistanceMeters);
	Hash = HashQuantizedDouble64(Hash, State.RideDeliveryElapsedSeconds, 1000000.0);
	Hash = HashQuantizedDouble64(Hash, State.DismountElapsedSeconds, 1000000.0);
	Hash = HashQuantizedDouble64(Hash, State.ObstacleWarningDistanceMeters);
	Hash = HashUInt32LE64(Hash, static_cast<uint32>(State.ObstacleWarningEventId));
	Hash = HashUInt32LE64(Hash, static_cast<uint32>(State.CompletedFlips));
	Hash = HashUInt32LE64(Hash, static_cast<uint32>(State.Falls));
	Hash = HashUInt32LE64(Hash, static_cast<uint32>(State.Score.DistancePoints));
	Hash = HashUInt32LE64(Hash, static_cast<uint32>(State.Score.CoinPoints));
	Hash = HashUInt32LE64(Hash, static_cast<uint32>(State.Score.TrickAndComboPoints));
	Hash = HashUInt32LE64(Hash, static_cast<uint32>(State.Score.RidePoints));
	Hash = HashUInt32LE64(Hash, static_cast<uint32>(State.Score.NoFallPoints));
	Hash = HashBool64(Hash, State.bJumpCharging);
	Hash = HashBool64(Hash, State.bJumpBuffered);
	Hash = HashBool64(Hash, State.bAirborne);
	Hash = HashBool64(Hash, State.bOnSnow);
	Hash = HashBool64(Hash, State.bFallen);
	Hash = HashBool64(Hash, State.bFallInvulnerable);
	Hash = HashBool64(Hash, State.bEverFell);
	Hash = HashBool64(Hash, State.bAvalancheWarning);
	Hash = HashBool64(Hash, State.bWasSwallowed);
	Hash = HashBool64(Hash, State.bRiding);
	Hash = HashBool64(Hash, State.bRidePouchAvailable);
	Hash = HashBool64(Hash, State.bRidePouchUsedThisRun);
	Hash = HashBool64(Hash, State.bRideDeliveryCanceled);
	Hash = HashBool64(Hash, State.bRideDeliveryAwarded);
	Hash = HashBool64(Hash, State.bRideSpeedBoostActive);
	Hash = HashBool64(Hash, State.bDismounted);
	Hash = HashBool64(Hash, State.bObstacleWarning);
	Hash = HashBool64(Hash, State.bSettlementSaved);
	Hash = HashQuantizedDouble64(Hash, PendingGapPenaltyMeters);
	Hash = HashBool64(Hash, PendingInput.bJumpHeld);
	Hash = HashBool64(Hash, PendingInput.bRotateLeft);
	Hash = HashBool64(Hash, PendingInput.bRotateRight);
	Hash = HashBool64(Hash, bJumpPressQueued);
	Hash = HashBool64(Hash, bJumpReleaseQueued);
	Hash = HashUInt32LE64(Hash, static_cast<uint32>(State.CollectedCoinIds.Num()));
	for (const int32 Id : State.CollectedCoinIds)
	{
		Hash = HashUInt32LE64(Hash, static_cast<uint32>(Id));
	}
	Hash = HashUInt32LE64(Hash, static_cast<uint32>(State.ResolvedObstacleIds.Num()));
	for (const int32 Id : State.ResolvedObstacleIds)
	{
		Hash = HashUInt32LE64(Hash, static_cast<uint32>(Id));
	}
	Hash = HashUInt32LE64(Hash, static_cast<uint32>(State.AbilityEvents.Num()));
	for (const FNaiwaP5AbilityEvent& Event : State.AbilityEvents)
	{
		Hash = HashName64(Hash, Event.AbilityId);
		Hash = HashName64(Hash, Event.TriggerId);
		Hash = HashUInt64LE64(Hash, static_cast<uint64>(Event.Tick));
		Hash = HashUInt32LE64(Hash, static_cast<uint32>(Event.DistanceMm));
		Hash = HashUInt32LE64(Hash, static_cast<uint32>(Event.Value));
	}
	return Hash;
}

void UNaiwaP5RunSubsystem::AdvanceFixedStep()
{
	if (State.Phase != ENaiwaP5RunPhase::Running)
	{
		return;
	}

	constexpr double DeltaSeconds = NaiwaP5Rules::FixedStepSeconds;
	++State.CurrentTick;
	State.ElapsedSeconds += DeltaSeconds;
	UpdateAbilityAndRideTimers(DeltaSeconds);
	if (!State.bOnSnow && !State.bAirborne)
	{
		State.CoyoteElapsedSeconds += DeltaSeconds;
	}

	const FNaiwaP5RouteFixture& RouteFixture = GetActivePrototypeFixture();
	const double PreviousDistanceMeters = State.DistanceMeters;
	const int32 CurrentDistanceMm = FMath::FloorToInt(State.DistanceMeters * 1000.0);
	const double PreviousSurfaceHeightMeters = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(
		RouteFixture, CurrentDistanceMm);
	const double TerrainSpeed = FNaiwaP5TrackData::GetRouteSpeedMetersPerSecond(RouteFixture, CurrentDistanceMm);
	if (State.bFallen || State.bFallInvulnerable)
	{
		State.FallElapsedSeconds += DeltaSeconds;
		State.PlayerSpeedMetersPerSecond = FNaiwaP5Simulation::GetFallSpeedMetersPerSecond(
			TerrainSpeed, State.FallElapsedSeconds);
		State.bFallInvulnerable = FNaiwaP5Simulation::IsFallInvulnerable(State.FallElapsedSeconds);
		if (State.FallElapsedSeconds >= NaiwaP5Rules::FallHardStunSeconds + NaiwaP5Rules::FallRecoverySeconds)
		{
			State.bFallen = false;
			State.bOnSnow = true;
			State.CoyoteElapsedSeconds = 0.0;
			State.JumpHeightMeters = 0.0;
		}
	}
	else
	{
		State.PlayerSpeedMetersPerSecond = TerrainSpeed;
		if (State.bDismounted)
		{
			const double RecoveryAlpha = FMath::Clamp(State.DismountElapsedSeconds / 2.0, 0.0, 1.0);
			State.PlayerSpeedMetersPerSecond *= FMath::Lerp(0.7, 1.0, RecoveryAlpha);
		}
		if (State.bRiding && State.bRideSpeedBoostActive)
		{
			State.PlayerSpeedMetersPerSecond *= 1.15;
		}
		if (const FNaiwaP5SkinRow* SkinRow = GetEquippedSkinRow())
		{
			if (State.LandingSpeedBoostSeconds > 0.0)
			{
				State.PlayerSpeedMetersPerSecond *= FMath::Max(1.0f, SkinRow->LandingSpeedMultiplier);
			}
		}
		double SpeedCap = NaiwaP5Rules::MaxPlayerSpeedMetersPerSecond;
		if (State.TrickBoostSeconds > 0.0 && State.TrickBoostLevel >= 2)
		{
			State.PlayerSpeedMetersPerSecond *= NaiwaP5Rules::TrickBoostTier2Multiplier;
			SpeedCap = NaiwaP5Rules::TrickBoostTier2MaxSpeedMetersPerSecond;
		}
		else if (State.TrickBoostSeconds > 0.0 && State.TrickBoostLevel == 1)
		{
			State.PlayerSpeedMetersPerSecond *= NaiwaP5Rules::TrickBoostTier1Multiplier;
			SpeedCap = NaiwaP5Rules::TrickBoostTier1MaxSpeedMetersPerSecond;
		}
		State.PlayerSpeedMetersPerSecond = FMath::Clamp(State.PlayerSpeedMetersPerSecond,
			0.0, SpeedCap);
	}
	State.LandingSpeedBoostSeconds = FMath::Max(0.0, State.LandingSpeedBoostSeconds - DeltaSeconds);

	FNaiwaP5InputCommand StepInput = PendingInput;
	StepInput.bJumpPressed = bJumpPressQueued;
	StepInput.bJumpReleased = bJumpReleaseQueued;
	bJumpPressQueued = false;
	bJumpReleaseQueued = false;
	ApplyInput(StepInput);
	State.DistanceMeters += State.PlayerSpeedMetersPerSecond * DeltaSeconds;
	const int32 NextDistanceMm = FMath::FloorToInt(State.DistanceMeters * 1000.0);
	const double CurrentSurfaceHeightMeters = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(
		RouteFixture, NextDistanceMm);

	if (State.bAirborne)
	{
		double JumpWorldHeightMeters = PreviousSurfaceHeightMeters + State.JumpHeightMeters;
		const int8 RotationAxis = FNaiwaP5Simulation::ResolveRotationAxis(
			StepInput.bRotateLeft, StepInput.bRotateRight);
		State.RotationDegrees = FNaiwaP5Simulation::AdvanceRotationDegrees(
			State.RotationDegrees, RotationAxis, DeltaSeconds);
		State.JumpElapsedSeconds += DeltaSeconds;
		if (State.bJumpCharging && StepInput.bJumpHeld &&
			State.JumpChargeSeconds < NaiwaP5Rules::JumpMaxHoldSeconds)
		{
			const double ChargeStep = FMath::Min(DeltaSeconds,
				NaiwaP5Rules::JumpMaxHoldSeconds - State.JumpChargeSeconds);
			State.JumpChargeSeconds += ChargeStep;
			State.JumpVerticalVelocityMetersPerSecond +=
				NaiwaP5Rules::JumpHoldAccelerationMetersPerSecondSquared * ChargeStep;
			if (State.JumpChargeSeconds >= NaiwaP5Rules::JumpMaxHoldSeconds)
			{
				State.bJumpCharging = false;
			}
		}
		if (StepInput.bJumpReleased || !StepInput.bJumpHeld)
		{
			State.bJumpCharging = false;
		}
		State.JumpVerticalVelocityMetersPerSecond -=
			NaiwaP5Rules::JumpGravityMetersPerSecondSquared * DeltaSeconds;
		JumpWorldHeightMeters += State.JumpVerticalVelocityMetersPerSecond * DeltaSeconds;
		State.JumpHeightMeters = JumpWorldHeightMeters - CurrentSurfaceHeightMeters;
		if (State.JumpElapsedSeconds > 0.05 && State.JumpHeightMeters <= 0.0)
		{
			ResolveLanding();
		}
	}

	ProcessRouteEvents(PreviousDistanceMeters, State.DistanceMeters);
	RefreshObstacleWarning();

	bool bSwallowed = false;
	State.AvalancheGapMeters = FNaiwaP5Simulation::AdvanceAvalancheGapMeters(
		State.AvalancheGapMeters, State.PlayerSpeedMetersPerSecond, DeltaSeconds,
		PendingGapPenaltyMeters, 0.0, bSwallowed);
	PendingGapPenaltyMeters = 0.0;
	State.bAvalancheWarning = State.AvalancheGapMeters <= NaiwaP5Rules::AvalancheWarningGapMeters;

	const double RouteLengthMeters = static_cast<double>(RouteFixture.RouteLengthMm) / 1000.0;
	if (State.DistanceMeters >= RouteLengthMeters)
	{
		FinalizeRun(ENaiwaP5RunPhase::Finished);
	}
	else if (bSwallowed)
	{
		State.bWasSwallowed = true;
		FinalizeRun(ENaiwaP5RunPhase::Swallowed);
	}
}

void UNaiwaP5RunSubsystem::ApplyInput(const FNaiwaP5InputCommand& Command)
{
	if (State.bFallen)
	{
		return;
	}

	if (Command.bJumpPressed)
	{
		if (State.bAirborne)
		{
			const double TimeToLanding = FNaiwaP5Simulation::GetJumpTimeToLandingSeconds(
				State.JumpHeightMeters, State.JumpVerticalVelocityMetersPerSecond);
			if (TimeToLanding <= NaiwaP5Rules::JumpBufferSeconds)
			{
				State.bJumpBuffered = true;
			}
		}
		else if (!State.bJumpCharging &&
			(State.bOnSnow || State.CoyoteElapsedSeconds <= NaiwaP5Rules::CoyoteTimeSeconds))
		{
			StartJump(true);
		}
	}

	if (State.bAirborne && State.bJumpCharging && (Command.bJumpReleased || !Command.bJumpHeld))
	{
		State.bJumpCharging = false;
	}

}

void UNaiwaP5RunSubsystem::StartJump(const bool bChargeWhileHeld)
{
	State.bJumpCharging = bChargeWhileHeld;
	State.bAirborne = true;
	State.bOnSnow = false;
	State.CoyoteElapsedSeconds = 0.0;
	State.bJumpBuffered = false;
	State.JumpChargeSeconds = 0.0;
	State.JumpElapsedSeconds = 0.0;
	State.JumpHeightMeters = 0.0;
	State.RotationDegrees = 0.0;
	State.JumpVerticalVelocityMetersPerSecond = NaiwaP5Rules::JumpLaunchVelocityMetersPerSecond *
		(State.bRiding ? NaiwaP5Rules::RideJumpVelocityMultiplier : 1.0);
}

void UNaiwaP5RunSubsystem::StartCliffLaunch()
{
	State.bJumpCharging = false;
	State.bAirborne = true;
	State.bOnSnow = false;
	State.CoyoteElapsedSeconds = 0.0;
	State.bJumpBuffered = false;
	State.JumpChargeSeconds = 0.0;
	State.JumpElapsedSeconds = 0.0;
	State.JumpHeightMeters = 0.0;
	State.JumpVerticalVelocityMetersPerSecond = NaiwaP5Rules::CliffLaunchVelocityMetersPerSecond;
	State.RotationDegrees = 0.0;
	AppendAbilityEvent(FName(TEXT("CliffLaunch")), FName(TEXT("Takeoff")),
		FMath::RoundToInt(NaiwaP5Rules::CliffLaunchVelocityMetersPerSecond * 1000.0));
}

void UNaiwaP5RunSubsystem::ResolveLanding()
{
	State.bAirborne = false;
	State.bJumpCharging = false;
	State.bOnSnow = true;
	State.CoyoteElapsedSeconds = 0.0;
	State.JumpChargeSeconds = 0.0;
	State.JumpHeightMeters = 0.0;
	State.JumpVerticalVelocityMetersPerSecond = 0.0;
	double JudgedRotationDegrees = FNaiwaP5Simulation::WrapAngleDegrees(State.RotationDegrees);
	if (const FNaiwaP5SkinRow* SkinRow = GetEquippedSkinRow())
	{
		if (SkinRow->LandingToleranceDeltaDegrees < 0.0f)
		{
			const double CorrectedMagnitude = FMath::Max(0.0,
				FMath::Abs(JudgedRotationDegrees) + static_cast<double>(SkinRow->LandingToleranceDeltaDegrees));
			JudgedRotationDegrees = FMath::Sign(JudgedRotationDegrees) * CorrectedMagnitude;
		}
	}
	const ENaiwaP5LandingGrade Grade = FNaiwaP5Simulation::ClassifyLanding(JudgedRotationDegrees);
	if (Grade == ENaiwaP5LandingGrade::Fall)
	{
		TriggerLandingAbilities(false);
		StartFall(NaiwaP5Rules::FallGapPenaltyMeters);
		return;
	}

	const int32 CompletedFlips = FMath::Min(2, FMath::FloorToInt(FMath::Abs(State.RotationDegrees) / 360.0));
	if (CompletedFlips > 0)
	{
		const double SecondsSinceLastAction = State.ElapsedSeconds - State.LastSuccessfulActionSeconds;
		State.ComboMultiplier = FNaiwaP5Simulation::AdvanceComboMultiplier(
			State.ComboMultiplier, SecondsSinceLastAction, State.LastSuccessfulActionSeconds >= 0.0);
		State.LastSuccessfulActionSeconds = State.ElapsedSeconds;
		State.CompletedFlips += CompletedFlips;
		State.Score.TrickAndComboPoints = FMath::Min(
			NaiwaP5Rules::MaxTrickAndComboPoints,
			State.Score.TrickAndComboPoints + FNaiwaP5Simulation::ScoreCompletedFlips(
				CompletedFlips, State.ComboMultiplier));
		const int32 AwardedBoostLevel = CompletedFlips >= 2 ? 2 : 1;
		State.TrickBoostLevel = FMath::Max(State.TrickBoostLevel, AwardedBoostLevel);
		State.TrickBoostSeconds = State.TrickBoostLevel >= 2
			? NaiwaP5Rules::TrickBoostTier2DurationSeconds
			: NaiwaP5Rules::TrickBoostTier1DurationSeconds;
		AppendAbilityEvent(FName(TEXT("TrickSpeedBoost")),
			State.TrickBoostLevel >= 2 ? FName(TEXT("TwoFlipTier2")) : FName(TEXT("OneFlipTier1")),
			State.TrickBoostLevel);
	}

	State.RotationDegrees = 0.0;
	State.JumpElapsedSeconds = 0.0;
	TriggerLandingAbilities(true);
	if (State.bJumpBuffered)
	{
		State.bJumpBuffered = false;
		StartJump(false);
	}
}

void UNaiwaP5RunSubsystem::StartFall(const double GapPenaltyMeters)
{
	if (State.bFallen)
	{
		return;
	}
	if (TryAbsorbRideFall())
	{
		return;
	}
	if (State.bRiding)
	{
		EndRide(false, FName(TEXT("Fall")));
	}
	State.bFallen = true;
	State.bEverFell = true;
	State.bFallInvulnerable = false;
	State.FallElapsedSeconds = 0.0;
	State.bJumpCharging = false;
	State.bAirborne = false;
	State.bOnSnow = false;
	State.bJumpBuffered = false;
	State.JumpHeightMeters = 0.0;
	State.JumpVerticalVelocityMetersPerSecond = 0.0;
	State.JumpElapsedSeconds = 0.0;
	State.RotationDegrees = 0.0;
	State.ComboMultiplier = 1.0;
	State.LastSuccessfulActionSeconds = -1.0;
	++State.Falls;
	PendingGapPenaltyMeters += FMath::Max(0.0, GapPenaltyMeters);
}

void UNaiwaP5RunSubsystem::ProcessRouteEvents(const double PreviousDistanceMeters,
	const double NewDistanceMeters)
{
	const int32 PreviousDistanceMm = FMath::FloorToInt(PreviousDistanceMeters * 1000.0);
	const int32 NewDistanceMm = FMath::FloorToInt(NewDistanceMeters * 1000.0);
	for (const FNaiwaP5RouteEvent& Event : GetActivePrototypeFixture().Events)
	{
		if (Event.DistanceMm <= PreviousDistanceMm || Event.DistanceMm > NewDistanceMm)
		{
			continue;
		}

		if (Event.Kind == ENaiwaP5RouteEventKind::Coin)
		{
			if (!HasId(State.CollectedCoinIds, Event.Id))
			{
				State.CollectedCoinIds.Add(Event.Id);
				State.Score.CoinPoints = FMath::Min(
					NaiwaP5Rules::MaxCoinCount * M1CoinPoints,
					State.Score.CoinPoints + M1CoinPoints);
			}
			continue;
		}

		if (HasId(State.ResolvedObstacleIds, Event.Id))
		{
			continue;
		}
		State.ResolvedObstacleIds.Add(Event.Id);

		if (Event.Kind == ENaiwaP5RouteEventKind::Kangaroo)
		{
			if (State.bRiding || State.bFallen)
			{
				continue;
			}
			if (State.JumpHeightMeters * 1000.0 >= Event.RequiredClearanceMm)
			{
				AppendAbilityEvent(FName(TEXT("KangarooRide")), FName(TEXT("SkippedByJump")));
				continue;
			}
			StartRide(Event);
			continue;
		}
		if (Event.Kind == ENaiwaP5RouteEventKind::CliffLaunch)
		{
			if (!State.bAirborne && !State.bFallen)
			{
				StartCliffLaunch();
			}
			else
			{
				AppendAbilityEvent(FName(TEXT("CliffLaunch")), FName(TEXT("AlreadyAirborne")));
			}
			continue;
		}

		HandleObstacleEvent(Event);
	}
}

void UNaiwaP5RunSubsystem::HandleObstacleEvent(const FNaiwaP5RouteEvent& Event)
{
	if (State.JumpHeightMeters * 1000.0 >= Event.RequiredClearanceMm || IsProtectedFromImpact())
	{
		return;
	}

	if (State.bRiding)
	{
		const bool bGapEvent = Event.Kind == ENaiwaP5RouteEventKind::IceGap ||
			Event.Kind == ENaiwaP5RouteEventKind::CliffGap;
		if (!bGapEvent)
		{
			State.bRideDeliveryCanceled = true;
		}
		if (TryAbsorbRideFall())
		{
			return;
		}

		if (bGapEvent)
		{
			EndRide(false, FName(TEXT("IceGapFall")));
			TriggerLandingAbilities(false);
			StartFall(NaiwaP5Rules::FallGapPenaltyMeters);
			return;
		}

		EndRide(false, FName(TEXT("ImpactDismount")));
		PendingGapPenaltyMeters += static_cast<double>(Event.HitGapPenaltyMm) / 1000.0;
		State.bDismounted = true;
		State.DismountElapsedSeconds = 0.0;
		State.bOnSnow = true;
		State.bAirborne = false;
		State.JumpHeightMeters = 0.0;
		State.JumpVerticalVelocityMetersPerSecond = 0.0;
		State.JumpElapsedSeconds = 0.0;
		State.bRideDeliveryCanceled = true;
		AppendAbilityEvent(FName(TEXT("KangarooRide")), FName(TEXT("ImpactDismount")), Event.HitGapPenaltyMm);
		return;
	}

	if (Event.Kind == ENaiwaP5RouteEventKind::IceGap ||
		Event.Kind == ENaiwaP5RouteEventKind::CliffGap)
	{
		TriggerLandingAbilities(false);
		StartFall(NaiwaP5Rules::FallGapPenaltyMeters);
		return;
	}

	const FNaiwaP5SkinRow* SkinRow = GetEquippedSkinRow();
	if (Event.Kind == ENaiwaP5RouteEventKind::Tree && SkinRow &&
		SkinRow->bIgnoresTreeHitPenalty && SkinRow->bIgnoresTreeSlowdown)
	{
		ResetCombo();
		AppendAbilityEvent(FName(TEXT("TreeImpactGuard")), FName(TEXT("TreeHit")), Event.Id);
		return;
	}

	PendingGapPenaltyMeters += static_cast<double>(Event.HitGapPenaltyMm) / 1000.0;
	TriggerLandingAbilities(false);
	StartFall(NaiwaP5Rules::FallGapPenaltyMeters);
}

void UNaiwaP5RunSubsystem::UpdateAbilityAndRideTimers(const double DeltaSeconds)
{
	State.AvalanchePushbackCooldownSeconds = FMath::Max(0.0,
		State.AvalanchePushbackCooldownSeconds - DeltaSeconds);
	State.TrickBoostSeconds = FMath::Max(0.0, State.TrickBoostSeconds - DeltaSeconds);
	if (State.TrickBoostSeconds <= 0.0)
	{
		State.TrickBoostLevel = 0;
	}

	if (State.bDismounted)
	{
		State.DismountElapsedSeconds += DeltaSeconds;
		if (State.DismountElapsedSeconds >= 2.0)
		{
			State.DismountElapsedSeconds = 2.0;
			State.bDismounted = false;
		}
	}

	if (!State.bRiding)
	{
		return;
	}

	State.RideElapsedSeconds += DeltaSeconds;
	if (!State.bRideDeliveryCanceled && !State.bRideDeliveryAwarded)
	{
		State.RideDeliveryElapsedSeconds += DeltaSeconds;
		if (State.RideDeliveryElapsedSeconds >= 8.0)
		{
			State.bRideDeliveryAwarded = true;
			State.bRideSpeedBoostActive = true;
			State.Score.RidePoints = FMath::Min(NaiwaP5Rules::MaxRidePoints,
				State.Score.RidePoints + 300);
			AppendAbilityEvent(FName(TEXT("KangarooRide")), FName(TEXT("DeliveryBoost")), 1500);
		}
	}

	const double RiddenDistanceMeters = FMath::Max(0.0, State.DistanceMeters - State.RideStartDistanceMeters);
	if (State.RideElapsedSeconds >= 30.0 || RiddenDistanceMeters >= 350.0)
	{
		EndRide(true, FName(TEXT("NaturalEnd")));
	}
}

void UNaiwaP5RunSubsystem::RefreshObstacleWarning()
{
	const FNaiwaP5SkinRow* SkinRow = GetEquippedSkinRow();
	const double WarningDistance = SkinRow ? FMath::Max(0.0f, SkinRow->ObstacleWarningDistanceMeters) : 0.0;
	int32 NextEventId = INDEX_NONE;
	double NextEventDistanceMeters = 0.0;
	if (WarningDistance > 0.0)
	{
		const int32 CurrentDistanceMm = FMath::FloorToInt(State.DistanceMeters * 1000.0);
		for (const FNaiwaP5RouteEvent& Event : GetActivePrototypeFixture().Events)
		{
			const bool bIsObstacle = Event.Kind == ENaiwaP5RouteEventKind::Tree ||
				Event.Kind == ENaiwaP5RouteEventKind::Rock || Event.Kind == ENaiwaP5RouteEventKind::IceGap ||
				Event.Kind == ENaiwaP5RouteEventKind::CliffGap;
			if (!bIsObstacle || Event.DistanceMm <= CurrentDistanceMm || HasId(State.ResolvedObstacleIds, Event.Id))
			{
				continue;
			}
			const double DistanceAhead = static_cast<double>(Event.DistanceMm - CurrentDistanceMm) / 1000.0;
			if (DistanceAhead <= WarningDistance)
			{
				NextEventId = Event.Id;
				NextEventDistanceMeters = DistanceAhead;
			}
			break;
		}
	}

	const bool bWasWarning = State.bObstacleWarning;
	const int32 PreviousEventId = State.ObstacleWarningEventId;
	State.bObstacleWarning = NextEventId != INDEX_NONE;
	State.ObstacleWarningEventId = NextEventId;
	State.ObstacleWarningDistanceMeters = NextEventDistanceMeters;
	if (State.bObstacleWarning && (!bWasWarning || PreviousEventId != NextEventId))
	{
		AppendAbilityEvent(FName(TEXT("ObstacleWarning")), FName(TEXT("WarningStarted")),
			FMath::RoundToInt(NextEventDistanceMeters * 1000.0));
	}
}

void UNaiwaP5RunSubsystem::TriggerLandingAbilities(const bool bSuccessfulLanding)
{
	const FNaiwaP5SkinRow* SkinRow = GetEquippedSkinRow();
	if (!SkinRow)
	{
		return;
	}

	if (bSuccessfulLanding && SkinRow->AbilityId == FName(TEXT("WaterFilm")) &&
		SkinRow->LandingSpeedDurationSeconds > 0.0f)
	{
		State.LandingSpeedBoostSeconds = FMath::Max(State.LandingSpeedBoostSeconds,
			static_cast<double>(SkinRow->LandingSpeedDurationSeconds));
		AppendAbilityEvent(SkinRow->AbilityId, FName(TEXT("LandingBoost")),
			FMath::RoundToInt((SkinRow->LandingSpeedMultiplier - 1.0f) * 1000.0f));
	}

	if (SkinRow->AbilityId == FName(TEXT("AvalanchePushback")) &&
		State.AvalanchePushbackCooldownSeconds <= 0.0 && SkinRow->AvalanchePushbackMeters > 0.0f)
	{
		State.AvalancheGapMeters = FMath::Min(NaiwaP5Rules::AvalancheGapMaxMeters,
			State.AvalancheGapMeters + static_cast<double>(SkinRow->AvalanchePushbackMeters));
		State.AvalanchePushbackCooldownSeconds = FMath::Max(0.0f,
			SkinRow->AvalanchePushbackCooldownSeconds);
		AppendAbilityEvent(SkinRow->AbilityId, FName(TEXT("LandingPushback")),
			FMath::RoundToInt(SkinRow->AvalanchePushbackMeters * 1000.0f));
	}
}

void UNaiwaP5RunSubsystem::StartRide(const FNaiwaP5RouteEvent& Event)
{
	State.bRiding = true;
	State.bRidePouchAvailable = !State.bRidePouchUsedThisRun;
	State.bRideDeliveryCanceled = false;
	State.bRideDeliveryAwarded = false;
	State.bRideSpeedBoostActive = false;
	State.RideElapsedSeconds = 0.0;
	State.RideDeliveryElapsedSeconds = 0.0;
	State.RideStartDistanceMeters = static_cast<double>(Event.DistanceMm) / 1000.0;
	AppendAbilityEvent(FName(TEXT("KangarooRide")), FName(TEXT("Mounted")), Event.Id);
}

void UNaiwaP5RunSubsystem::EndRide(const bool bNaturalEnd, const FName TriggerId)
{
	if (!State.bRiding)
	{
		return;
	}
	State.bRiding = false;
	State.bRidePouchAvailable = false;
	State.bRideSpeedBoostActive = false;
	if (bNaturalEnd)
	{
		State.Score.RidePoints = FMath::Min(NaiwaP5Rules::MaxRidePoints,
			State.Score.RidePoints + 200);
		AppendAbilityEvent(FName(TEXT("KangarooRide")), TriggerId, 200);
	}
	else
	{
		State.bRideDeliveryCanceled = true;
		AppendAbilityEvent(FName(TEXT("KangarooRide")), TriggerId);
	}
	State.RideElapsedSeconds = 0.0;
	State.RideDeliveryElapsedSeconds = 0.0;
}

bool UNaiwaP5RunSubsystem::TryAbsorbRideFall()
{
	if (!State.bRiding || !State.bRidePouchAvailable)
	{
		return false;
	}
	State.bRidePouchAvailable = false;
	State.bRidePouchUsedThisRun = true;
	State.bFallen = false;
	State.bFallInvulnerable = false;
	State.FallElapsedSeconds = 0.0;
	State.bJumpCharging = false;
	State.bAirborne = false;
	State.bOnSnow = true;
	State.bJumpBuffered = false;
	State.JumpChargeSeconds = 0.0;
	State.JumpElapsedSeconds = 0.0;
	State.JumpHeightMeters = 0.0;
	State.JumpVerticalVelocityMetersPerSecond = 0.0;
	State.CoyoteElapsedSeconds = 0.0;
	State.RotationDegrees = 0.0;
	AppendAbilityEvent(FName(TEXT("KangarooRide")), FName(TEXT("PouchAbsorb")));
	return true;
}

bool UNaiwaP5RunSubsystem::IsProtectedFromImpact() const
{
	return State.bFallInvulnerable || (State.bDismounted && State.DismountElapsedSeconds < 1.2);
}

void UNaiwaP5RunSubsystem::AppendAbilityEvent(const FName AbilityId, const FName TriggerId,
	const int32 Value)
{
	FNaiwaP5AbilityEvent Event;
	Event.AbilityId = AbilityId;
	Event.TriggerId = TriggerId;
	Event.Tick = State.CurrentTick;
	Event.DistanceMm = FMath::RoundToInt(State.DistanceMeters * 1000.0);
	Event.Value = Value;
	State.AbilityEvents.Add(Event);
	UE_LOG(LogTemp, Log, TEXT("NAIWA_P5_ABILITY ability=%s trigger=%s tick=%lld distance_mm=%d value=%d"),
		*AbilityId.ToString(), *TriggerId.ToString(), static_cast<long long>(Event.Tick), Event.DistanceMm, Value);
}

void UNaiwaP5RunSubsystem::ResetCombo()
{
	State.ComboMultiplier = 1.0;
	State.LastSuccessfulActionSeconds = -1.0;
}

void UNaiwaP5RunSubsystem::RecordP5Settlement()
{
	if (State.bSettlementSaved || !GetWorld())
	{
		return;
	}
	if (!State.bM2PrototypeRoute)
	{
		State.bSettlementSaved = true;
		return;
	}
	UGameInstance* GameInstance = GetWorld()->GetGameInstance();
	UNaiwaProgressSubsystem* Progress = GameInstance
		? GameInstance->GetSubsystem<UNaiwaProgressSubsystem>() : nullptr;
	if (!Progress)
	{
		return;
	}
	Progress->RecordP5RunSettlement(State.Phase == ENaiwaP5RunPhase::Finished,
		State.Score.GetTicketsEarned(),
		FMath::RoundToInt(State.DistanceMeters * 1000.0), State.Score.GetTotalScore(),
		static_cast<float>(State.ElapsedSeconds), State.Falls, State.CollectedCoinIds.Num());
	State.bSettlementSaved = true;
}

const FNaiwaP5SkinRow* UNaiwaP5RunSubsystem::GetEquippedSkinRow() const
{
	return FNaiwaP5SkinCatalog::GetRows().FindByPredicate(
		[this](const FNaiwaP5SkinRow& Row) { return Row.SkinId == State.EquippedSkinId; });
}

void UNaiwaP5RunSubsystem::FinalizeRun(const ENaiwaP5RunPhase FinalPhase)
{
	const double RouteLengthMeters = static_cast<double>(GetActivePrototypeFixture().RouteLengthMm) / 1000.0;
	State.DistanceMeters = FMath::Min(State.DistanceMeters, RouteLengthMeters);
	State.Score.DistancePoints = FMath::FloorToInt(State.DistanceMeters);
	State.Score.NoFallPoints = State.bEverFell ? 0 : NaiwaP5Rules::NoFallBonusPoints;
	State.Phase = FinalPhase;
	FixedAccumulatorSeconds = 0.0;
	State.DeferredCatchUpSteps = 0;
	PendingInput = FNaiwaP5InputCommand{};
	bJumpPressQueued = false;
	bJumpReleaseQueued = false;
	RecordP5Settlement();
}
