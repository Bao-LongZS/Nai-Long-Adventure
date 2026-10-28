#include "NaiwaP3M2RuntimeSubsystem.h"

#include "NaiwaP3BombActor.h"
#include "NaiwaP3M1Actors.h"
#include "NaiwaP3PlayerPawn.h"
#include "NaiwaP3M2WorldPresentation.h"
#include "NaiwaP3M2Settings.h"
#include "NaiwaP3M2GameMode.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameModeBase.h"

namespace
{
	ANaiwaP3M2GameMode* P3AudioGameMode(UWorld* World)
	{
		return World ? Cast<ANaiwaP3M2GameMode>(World->GetAuthGameMode()) : nullptr;
	}

	int32 M2CellIndex(const FIntPoint& Cell, const int32 GridWidth)
	{
		return Cell.Y * GridWidth + Cell.X;
	}
}

void UNaiwaP3M2RuntimeSubsystem::Deinitialize()
{
	StopSession();
	Super::Deinitialize();
}

void UNaiwaP3M2RuntimeSubsystem::Tick(const float DeltaTime)
{
	if (!bSessionActive || DeltaTime <= 0.0f)
	{
		return;
	}
	FixedAccumulator += DeltaTime;
	while (FixedAccumulator >= FixedStepSeconds)
	{
		FixedAccumulator -= FixedStepSeconds;
		AdvanceFixedStep();
	}
	AdvancePlayerMovement(DeltaTime);
	UpdatePresentation(DeltaTime);
}

TStatId UNaiwaP3M2RuntimeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNaiwaP3M2RuntimeSubsystem, STATGROUP_Tickables);
}

void UNaiwaP3M2RuntimeSubsystem::SetTransientEvent(const FString& Message, const int32 DurationTicks)
{
	if (Message.IsEmpty()) return;
	TransientEventMessage = Message;
	TransientEventUntilTick = Session.GetCurrentTick() + FMath::Max(1, DurationTicks);
}

void UNaiwaP3M2RuntimeSubsystem::TriggerExplosionFeedback(const FIntPoint& BombCell)
{
	if (!GetWorld()) return;
	const int32 Distance = FMath::Abs(BombCell.X - PlayerCell.X) + FMath::Abs(BombCell.Y - PlayerCell.Y);
	if (Distance > 12) return;
	const float Strength = FMath::Lerp(42.0f, 8.0f, static_cast<float>(Distance) / 12.0f);
	ExplosionShakeAmplitude = FMath::Max(ExplosionShakeAmplitude, Strength);
	ExplosionShakeStartSeconds = GetWorld()->GetTimeSeconds();
	ExplosionShakeDurationSeconds = 0.28f;
	ExplosionShakeEndSeconds = ExplosionShakeStartSeconds + ExplosionShakeDurationSeconds;
}

FVector2D UNaiwaP3M2RuntimeSubsystem::GetExplosionShakeOffset() const
{
	if (!GetWorld()) return FVector2D::ZeroVector;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= ExplosionShakeEndSeconds || ExplosionShakeAmplitude <= 0.0f) return FVector2D::ZeroVector;
	const float Remaining = FMath::Clamp((ExplosionShakeEndSeconds - Now) /
		FMath::Max(ExplosionShakeDurationSeconds, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const float Age = Now - ExplosionShakeStartSeconds;
	const float Envelope = FMath::Pow(Remaining, 1.4f) * ExplosionShakeAmplitude;
	return FVector2D(FMath::Sin(Age * 91.0f) * Envelope,
		FMath::Sin(Age * 67.0f + 1.7f) * Envelope * 0.72f);
}

bool UNaiwaP3M2RuntimeSubsystem::StartSession(const int32 LevelId, const int32 StartAttemptCount,
	const bool bFogEnabled, FString& OutError)
{
	StopSession();
	if (!Session.Initialize(LevelId, StartAttemptCount, bFogEnabled, OutError))
	{
		return false;
	}

	PlayerCell = Session.GetLevel().SpawnCell;
	EnemyCells.Reset(Session.GetLevel().Enemies.Num());
	for (const FNaiwaP3M2EnemyPlacement& Enemy : Session.GetLevel().Enemies)
	{
		EnemyCells.Add(Enemy.Cell);
	}
	EnemyIntents.SetNum(EnemyCells.Num());
	EnemyAlive.Init(1, EnemyCells.Num());
	EnemyLootStates.SetNum(EnemyCells.Num());
	EnemyActors.SetNum(EnemyCells.Num());
	EnemyVisualFromCells = EnemyCells;
	EnemyVisualToCells = EnemyCells;
	EnemyVisualProgress.Init(1.0f, EnemyCells.Num());
	EnemyVisualDuration.Init(0.25f, EnemyCells.Num());
	RageBombNextTicks.Init(INDEX_NONE, EnemyCells.Num());
	SlowSplashes.Reset();
	ActiveBombs.Reset();
	BombVisuals.Reset();
	HazardForecast.Reset();
	EnemyBombRequests.Reset();
	EnemyRangedAttackRequests.Reset();
	MechanicDestroyedBombCells.Reset();
	BombNextConveyorTick.Reset();
	FlameExpiryTick.Reset();
	FixedAccumulator = 0.0f;
	PlayerMoveProgress = 0.0f;
	PlayerDirection = FIntPoint::ZeroValue;
	PlayerVisualFromWorld = GridToWorld(PlayerCell);
	PlayerVisualToWorld = PlayerVisualFromWorld;
	PlayerVisualProgress = 1.0f;
	PlayerHitCount = 0;
	PlayerLives = 3;
	PreviousFogExploredCount = 0;
	PlayerInvulnerableUntilTick = 0;
	PlayerSlowedUntilTick = 0;
	ExplosionFlashUntilTick = 0;
	DamageFeedbackUntilTick = 0;
	DamageFeedbackMessage.Reset();
	TransientEventUntilTick = 0;
	TransientEventMessage.Reset();
	ExplosionShakeStartSeconds = 0.0f;
	ExplosionShakeEndSeconds = 0.0f;
	ExplosionShakeAmplitude = 0.0f;
	bLastDamageShielded = false;
	bRageEscape = false;
	RageStartTick = -1;
	RunState = ENaiwaP3M2RunState::Playing;
	bSessionActive = true;
	SelectedBombType = ENaiwaP3BombType::Standard;
	NextBombOrder = 1;
	NextPlayerBombTick = 0;
	const UNaiwaP3M2Settings* Settings = GetDefault<UNaiwaP3M2Settings>();
	BombPlacementIntervalTicks = Settings ? FMath::Max(1,
		FMath::RoundToInt(Settings->BombPlacementIntervalSeconds / FixedStepSeconds)) : 4;
	UpdatePresentation(0.0f);
	return true;
}

void UNaiwaP3M2RuntimeSubsystem::StopSession()
{
	if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
	{
		Audio->SetP3ConveyorAudio(false);
		Audio->SetP3RainAudio(false);
		Audio->SetP3RageAudio(false);
	}
	bSessionActive = false;
	for (const TPair<uint64, TWeakObjectPtr<ANaiwaP3BombActor>>& Visual : BombVisuals)
	{
		if (Visual.Value.IsValid()) { Visual.Value->Destroy(); }
	}
	FixedAccumulator = 0.0f;
	ActiveBombs.Reset();
	BombVisuals.Reset();
	HazardForecast.Reset();
	EnemyCells.Reset();
	EnemyIntents.Reset();
	EnemyVisualFromCells.Reset();
	EnemyVisualToCells.Reset();
	EnemyVisualProgress.Reset();
	EnemyVisualDuration.Reset();
	RageBombNextTicks.Reset();
	SlowSplashes.Reset();
	EnemyBombRequests.Reset();
	EnemyRangedAttackRequests.Reset();
	EnemyAlive.Reset();
	EnemyLootStates.Reset();
	MechanicDestroyedBombCells.Reset();
	BombNextConveyorTick.Reset();
	FlameExpiryTick.Reset();
	PlayerDirection = FIntPoint::ZeroValue;
	PlayerMoveProgress = 0.0f;
	PlayerVisualProgress = 1.0f;
	PlayerHitCount = 0;
	PlayerLives = 3;
	PlayerInvulnerableUntilTick = 0;
	PlayerSlowedUntilTick = 0;
	ExplosionFlashUntilTick = 0;
	DamageFeedbackUntilTick = 0;
	DamageFeedbackMessage.Reset();
	TransientEventUntilTick = 0;
	TransientEventMessage.Reset();
	ExplosionShakeStartSeconds = 0.0f;
	ExplosionShakeEndSeconds = 0.0f;
	ExplosionShakeAmplitude = 0.0f;
	bLastDamageShielded = false;
	bRageEscape = false;
	RageStartTick = -1;
	RunState = ENaiwaP3M2RunState::Playing;
}

void UNaiwaP3M2RuntimeSubsystem::RegisterPlayerPawn(ANaiwaP3PlayerPawn* Pawn)
{
	PlayerPawn = Pawn;
	UpdatePresentation(0.0f);
}

void UNaiwaP3M2RuntimeSubsystem::RegisterPresentationActors(
	ANaiwaP3M1MapActor* InMapActor, const TArray<ANaiwaP3M1EnemyActor*>& InEnemyActors,
	ANaiwaP3M2WorldPresentation* InWorldPresentation)
{
	MapActor = InMapActor;
	WorldPresentation = InWorldPresentation;
	EnemyActors.SetNum(InEnemyActors.Num());
	for (int32 Index = 0; Index < InEnemyActors.Num(); ++Index)
	{
		EnemyActors[Index] = InEnemyActors[Index];
	}
	UpdateFogVisibility();
	UpdatePresentation(0.0f);
}

void UNaiwaP3M2RuntimeSubsystem::SetPlayerDirection(const FIntPoint& Direction)
{
	if (FMath::Abs(Direction.X) + FMath::Abs(Direction.Y) > 1 || Direction == PlayerDirection)
	{
		return;
	}
	PlayerDirection = Direction;
	PlayerMoveProgress = 0.0f;
	if (Direction == FIntPoint::ZeroValue || RunState != ENaiwaP3M2RunState::Playing)
	{
		return;
	}

	// Start movement on the key-press frame; the fixed simulation clock handles hazards and bombs.
	FString Message;
	TryMovePlayer(Direction, Message);
}

void UNaiwaP3M2RuntimeSubsystem::SelectPlayerBombType(const ENaiwaP3BombType Type)
{
	if (Session.GetLevel().LevelId == 31)
	{
		SelectedBombType = ENaiwaP3BombType::Standard;
		return;
	}
	SelectedBombType = Type == ENaiwaP3BombType::RatFragment ? ENaiwaP3BombType::Standard : Type;
}

bool UNaiwaP3M2RuntimeSubsystem::PlacePlayerBomb()
{
	if (!bSessionActive || RunState != ENaiwaP3M2RunState::Playing)
	{
		return false;
	}
	if (Session.GetCurrentTick() < NextPlayerBombTick)
	{
		SetTransientEvent(FString::Printf(TEXT("放弹冷却 %.1f 秒"), GetBombPlacementCooldownSeconds()), 8);
		return false;
	}
	const int32 Range = SelectedBombType == ENaiwaP3BombType::Standard ?
		1 : Session.GetPlayerItems().GetBombRange();
	if (!SpawnBombAt(PlayerCell, Session.GetCurrentTick() + BombFuseTicks,
		SelectedBombType, Range))
	{
		SetTransientEvent(TEXT("当前位置无法放弹：容量已满或格子被占用"), 12);
		return false;
	}
	NextPlayerBombTick = Session.GetCurrentTick() + BombPlacementIntervalTicks;
	if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
	{
		Audio->PlayP3Audio2D(TEXT("SFX_P3_PlaceBomb"), -3.0f,
			1.0f + 0.04f * FMath::Sin(static_cast<float>(NextBombOrder)));
	}
	return true;
}

bool UNaiwaP3M2RuntimeSubsystem::SpawnBombAt(const FIntPoint& Cell, const int32 FuseTick,
	const ENaiwaP3BombType Type, const int32 Range, const bool bIgnoreCapacity,
	const int32 OwnerEnemyIndex, const FIntPoint& VisualStartCell)
{
	int32 PlayerOwnedBombCount = 0;
	for (const FNaiwaP3M2BombForecastInput& Existing : ActiveBombs)
	{
		PlayerOwnedBombCount += Existing.bPlayerOwned ? 1 : 0;
	}
	if (!bSessionActive || !Session.GetLevel().IsInside(Cell) ||
		Session.GetLevel().GetCell(Cell) != ENaiwaP3M2Cell::Floor ||
		Session.GetMechanics().ShouldDestroyBomb(Cell, Session.GetCurrentTick()) ||
		FlameExpiryTick.Contains(Cell) ||
	(!bIgnoreCapacity && PlayerOwnedBombCount >= Session.GetPlayerItems().GetBombCapacity()))
	{
		return false;
	}
	for (const FNaiwaP3M2BombForecastInput& Existing : ActiveBombs)
	{
		if (Existing.Cell == Cell) { return false; }
	}

	ANaiwaP3BombActor* BombActor = GetWorld() ? GetWorld()->SpawnActor<ANaiwaP3BombActor>() : nullptr;
	if (!BombActor)
	{
		return false;
	}
	int32 ResolvedFuseTick = FuseTick;
	if (Type != ENaiwaP3BombType::RatFragment)
	{
		const uint32 RollSeed = static_cast<uint32>(Session.GetLevel().Seed) ^
			(static_cast<uint32>(NextBombOrder) * 196314165u) ^
			(static_cast<uint32>(Cell.X) * 101u) ^ (static_cast<uint32>(Cell.Y) * 313u);
		FRandomStream MisfireStream(static_cast<int32>(RollSeed));
		if (MisfireStream.RandRange(0, 99) < 2)
		{
			ResolvedFuseTick = Session.GetCurrentTick() + BombFuseTicks * 2;
		}
	}
	BombActor->SetWorldPosition(GridToWorld(Cell));
	BombActor->SetBombType(Type);
	if (VisualStartCell.X != INDEX_NONE && VisualStartCell.Y != INDEX_NONE &&
		VisualStartCell != Cell)
	{
		BombActor->BeginVisualSlide(GridToWorld(VisualStartCell), GridToWorld(Cell), 0.18f);
	}

	FNaiwaP3M2BombForecastInput& Bomb = ActiveBombs.AddDefaulted_GetRef();
	Bomb.Cell = Cell;
	Bomb.FuseTick = ResolvedFuseTick;
	Bomb.FuseDurationTicks = FMath::Max(1, ResolvedFuseTick - Session.GetCurrentTick());
	Bomb.PlacementOrder = NextBombOrder++;
	Bomb.Type = Type;
	Bomb.Range = FMath::Max(1, Range);
	Bomb.bPlayerOwned = !bIgnoreCapacity;
	Bomb.OwnerEnemyIndex = OwnerEnemyIndex;
	BombVisuals.Add(Bomb.PlacementOrder, BombActor);
	return true;
}

void UNaiwaP3M2RuntimeSubsystem::ClearPendingEvents()
{
	EnemyBombRequests.Reset();
	EnemyRangedAttackRequests.Reset();
	MechanicDestroyedBombCells.Reset();
}

void UNaiwaP3M2RuntimeSubsystem::SetActiveBombForecasts(
	const TArray<FNaiwaP3M2BombForecastInput>& Bombs)
{
	if (!bSessionActive)
	{
		return;
	}
	ActiveBombs.Reset(Bombs.Num());
	for (const FNaiwaP3M2BombForecastInput& Bomb : Bombs)
	{
		if (Session.GetLevel().IsInside(Bomb.Cell))
		{
			ActiveBombs.Add(Bomb);
		}
	}
}

bool UNaiwaP3M2RuntimeSubsystem::TryMovePlayer(const FIntPoint& Direction, FString& OutMessage)
{
	OutMessage.Reset();
	if (!bSessionActive || RunState != ENaiwaP3M2RunState::Playing ||
		FMath::Abs(Direction.X) + FMath::Abs(Direction.Y) != 1)
	{
		return false;
	}

	const FIntPoint EffectiveDirection = Session.GetPlayerItems().ApplyControlEffects(
		Direction, Session.GetCurrentTick());
	const FIntPoint RequestedCell = PlayerCell + EffectiveDirection;
	for (int32 EnemyIndex = 0; EnemyIndex < EnemyCells.Num(); ++EnemyIndex)
	{
		if (IsEnemyAlive(EnemyIndex) && EnemyCells[EnemyIndex] == RequestedCell)
		{
			const int32 PreviousLives = PlayerLives;
			DamagePlayer(false, TEXT("奶察抓到你"));
			OutMessage = PlayerLives < PreviousLives ? TEXT("奶察抓到你，生命 -1") : TEXT("奶察撞到你（闪避状态）");
			SetTransientEvent(OutMessage, 16);
			return true;
		}
	}
	if (!IsPlayerDestinationOpen(RequestedCell))
	{
		return false;
	}
	FIntPoint LandingCell = RequestedCell;
	const FVector PreviousVisualLocation = PlayerPawn.IsValid() ? PlayerPawn->GetActorLocation() :
		FMath::Lerp(PlayerVisualFromWorld, PlayerVisualToWorld, PlayerVisualProgress);

	FIntPoint PortalDestination;
	if (Session.GetMechanics().TryTeleport(RequestedCell, 1, Session.GetCurrentTick(), PortalDestination) &&
		IsPlayerDestinationOpen(PortalDestination))
	{
		LandingCell = PortalDestination;
		OutMessage = TEXT("传送成功");
	}
	else
	{
		FIntPoint ConveyorDirection;
		if (Session.GetMechanics().TryGetConveyorDirection(RequestedCell, ConveyorDirection))
		{
			const FIntPoint PushedCell = RequestedCell + ConveyorDirection;
			if (IsPlayerDestinationOpen(PushedCell))
			{
				LandingCell = PushedCell;
				OutMessage = TEXT("被奶流传送带推送");
			}
		}
	}

	PlayerCell = LandingCell;
	if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
	{
		FIntPoint ConveyorDirection;
		const bool bEnteredConveyor = Session.GetMechanics().TryGetConveyorDirection(
			RequestedCell, ConveyorDirection);
		Audio->SetP3ConveyorAudio(bEnteredConveyor);
	}
	if (OutMessage.StartsWith(TEXT("传送")))
	{
		if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
			Audio->PlayP3AudioAtLocation(TEXT("SFX_P3_Portal"), GridToWorld(RequestedCell), -4.0f);
	}
	for (const FNaiwaP3M2MechanicPlacement& Mechanic : Session.GetLevel().Mechanics)
	{
		if (Mechanic.Kind == ENaiwaP3M2MechanicKind::PressurePlate && Mechanic.Cell == RequestedCell)
		{
			if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
				Audio->PlayP3AudioAtLocation(TEXT("SFX_P3_PressurePlate"), GridToWorld(RequestedCell), -8.0f);
		}
	}
	PlayerVisualFromWorld = PreviousVisualLocation;
	PlayerVisualToWorld = GridToWorld(LandingCell);
	if (FVector::DistSquared(PlayerVisualToWorld, PreviousVisualLocation) > FMath::Square(150.0f) &&
		OutMessage.StartsWith(TEXT("传送")))
	{
		PlayerVisualProgress = 1.0f;
	}
	else
	{
		PlayerVisualProgress = 0.0f;
		PlayerVisualDuration = PlayerCellSeconds / GetPlayerSpeedMultiplier();
	}
	ProcessPlayerLanding(PlayerCell, OutMessage);
	if (Session.GetMechanics().IsSpikeActive(PlayerCell, Session.GetCurrentTick()))
	{
		const int32 PreviousLives = PlayerLives;
		DamagePlayer(false, TEXT("尖刺扎伤"));
		const FString SpikeMessage = PlayerLives < PreviousLives ? TEXT("尖刺扎伤，生命 -1") : TEXT("尖刺擦过（闪避状态）");
		OutMessage = OutMessage.IsEmpty() ? SpikeMessage : OutMessage + TEXT("；") + SpikeMessage;
	}
	if (!OutMessage.IsEmpty()) SetTransientEvent(OutMessage, 20);
	return true;
}

bool UNaiwaP3M2RuntimeSubsystem::TryChickenJump(const FIntPoint& Direction, FString& OutMessage)
{
	OutMessage.Reset();
	if (!bSessionActive || RunState != ENaiwaP3M2RunState::Playing ||
		FMath::Abs(Direction.X) + FMath::Abs(Direction.Y) != 1)
	{
		return false;
	}
	const FIntPoint EffectiveDirection = Session.GetPlayerItems().ApplyControlEffects(
		Direction, Session.GetCurrentTick());
	const FIntPoint TargetCell = PlayerCell + EffectiveDirection * 2;
	const FIntPoint PreviousCell = PlayerCell;
	FIntPoint LandingCell;
	const bool bTargetOpen = IsPlayerDestinationOpen(TargetCell);
	if (!Session.GetPlayerItems().TryUseChickenJump(Session.GetCurrentTick(), TargetCell,
		bTargetOpen, LandingCell))
	{
		return false;
	}
	PlayerCell = LandingCell;
	PlayerVisualFromWorld = GridToWorld(PreviousCell);
	PlayerVisualToWorld = GridToWorld(LandingCell);
	PlayerVisualProgress = 1.0f;
	OutMessage = TEXT("奶鸡跳成功");
	if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
		Audio->PlayP3Audio2D(TEXT("SFX_P3_ItemChickenJump"), -4.0f, 1.0f, 0.025f);
	ProcessPlayerLanding(PlayerCell, OutMessage);
	if (Session.GetMechanics().IsSpikeActive(PlayerCell, Session.GetCurrentTick()))
	{
		const int32 PreviousLives = PlayerLives;
		DamagePlayer(false, TEXT("尖刺扎伤"));
		OutMessage += PlayerLives < PreviousLives ? TEXT("；尖刺扎伤，生命 -1") : TEXT("；尖刺擦过（闪避状态）");
	}
	if (!OutMessage.IsEmpty()) SetTransientEvent(OutMessage, 20);
	return true;
}

void UNaiwaP3M2RuntimeSubsystem::ReportFireAtCell(const FIntPoint& Cell)
{
	if (!bSessionActive || !Session.GetLevel().IsInside(Cell))
	{
		return;
	}
	if (Session.GetMechanics().ShouldConsumeFire(Cell, Session.GetCurrentTick()))
	{
		return;
	}
	const bool bGuaranteeTeachingDrop = Session.GetLevel().LevelId == 31 && Cell == FIntPoint(5, 2);
	const bool bBrokeBrick = Session.BreakBrickAt(Cell, bGuaranteeTeachingDrop);
	SyncMapTile(Cell);
	if (bBrokeBrick && WorldPresentation.IsValid())
	{
		WorldPresentation->Sync(Session, PlayerCell, EnemyCells, EnemyAlive);
	}
	if (bBrokeBrick && WorldPresentation.IsValid())
	{
		ENaiwaP3LootType DroppedItem;
		if (Session.TryGetItemAt(Cell, DroppedItem))
		{
			WorldPresentation->SetItemAt(Cell, DroppedItem);
		}
	}
	Session.RegisterFireAtCell(Cell);
	FlameExpiryTick.FindOrAdd(Cell) = Session.GetCurrentTick() + FlameDurationTicks;
	if (MapActor.IsValid())
	{
		MapActor->SetFlame(Cell.X, Cell.Y, true);
	}
	if (Cell == PlayerCell)
	{
		DamagePlayer(true, TEXT("炸弹命中"));
	}
	for (int32 EnemyIndex = 0; EnemyIndex < EnemyCells.Num(); ++EnemyIndex)
	{
		if (IsEnemyAlive(EnemyIndex) && EnemyCells[EnemyIndex] == Cell)
		{
			if (EnemyLootStates.IsValidIndex(EnemyIndex) &&
				EnemyLootStates[EnemyIndex].ShieldCharges > 0)
			{
				--EnemyLootStates[EnemyIndex].ShieldCharges;
				continue;
			}
			EnemyAlive[EnemyIndex] = 0;
			if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
				Audio->PlayP3Audio2D(TEXT("SFX_P3_Explode_Normal"), -6.0f, 0.89125f);
			if (EnemyActors.IsValidIndex(EnemyIndex) && EnemyActors[EnemyIndex].IsValid())
			{
				EnemyActors[EnemyIndex]->Destroy();
				EnemyActors[EnemyIndex] = nullptr;
			}
		}
	}
}

void UNaiwaP3M2RuntimeSubsystem::AdvanceFixedStep()
{
	TArray<FIntPoint> LivingEnemyCells;
	for (int32 EnemyIndex = 0; EnemyIndex < EnemyCells.Num(); ++EnemyIndex)
	{
		if (IsEnemyAlive(EnemyIndex)) { LivingEnemyCells.Add(EnemyCells[EnemyIndex]); }
	}
	Session.AdvanceTick(PlayerCell, LivingEnemyCells, bRageEscape);
	for (const FNaiwaP3M2MechanicPlacement& Mechanic : Session.GetLevel().Mechanics)
	{
		if (Mechanic.Kind == ENaiwaP3M2MechanicKind::Spike &&
			Session.GetMechanics().IsSpikeActive(Mechanic.Cell, Session.GetCurrentTick()) &&
			!Session.GetMechanics().IsSpikeActive(Mechanic.Cell, Session.GetCurrentTick() - 1))
		{
			if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
				Audio->PlayP3AudioAtLocation(TEXT("SFX_P3_SpikeOut"), GridToWorld(Mechanic.Cell), 1.0f);
		}
		if (Mechanic.Kind == ENaiwaP3M2MechanicKind::VaultGate &&
			Session.GetMechanics().IsGateOpen(Mechanic.Cell, Session.GetCurrentTick()) &&
			!Session.GetMechanics().IsGateOpen(Mechanic.Cell, Session.GetCurrentTick() - 1))
		{
			if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
				Audio->PlayP3AudioAtLocation(TEXT("SFX_P3_GateOpen"), GridToWorld(Mechanic.Cell), 4.0f);
		}
	}
	EnemyBombRequests.Reset();
	EnemyRangedAttackRequests.Reset();
	if (RunState != ENaiwaP3M2RunState::Playing)
	{
		ResolveDueBombs();
		ExpireFlames();
		UpdateFogVisibility();
		return;
	}
	for (int32 EnemyIndex = 0; EnemyIndex < EnemyCells.Num(); ++EnemyIndex)
	{
		if (IsEnemyAlive(EnemyIndex)) CollectEnemyItemAt(EnemyIndex, EnemyCells[EnemyIndex]);
	}
	for (int32 BombIndex = ActiveBombs.Num() - 1; BombIndex >= 0; --BombIndex)
	{
		FNaiwaP3M2BombForecastInput& Bomb = ActiveBombs[BombIndex];
		const int32 CurrentTick = Session.GetCurrentTick();
		if (Session.GetMechanics().ShouldDestroyBomb(Bomb.Cell, CurrentTick))
		{
			MechanicDestroyedBombCells.Add(Bomb.Cell);
			if (const TWeakObjectPtr<ANaiwaP3BombActor>* Visual = BombVisuals.Find(Bomb.PlacementOrder))
			{
				if (Visual->IsValid()) { Visual->Get()->Destroy(); }
			}
			BombVisuals.Remove(Bomb.PlacementOrder);
			BombNextConveyorTick.Remove(Bomb.PlacementOrder);
			ActiveBombs.RemoveAt(BombIndex, 1, EAllowShrinking::No);
			continue;
		}
		if (Session.GetMechanics().ShouldForceBombDetonation(Bomb.Cell, CurrentTick))
		{
			Bomb.FuseTick = FMath::Min(Bomb.FuseTick, CurrentTick);
		}

		FIntPoint ConveyorDirection;
		if (CurrentTick >= BombNextConveyorTick.FindRef(Bomb.PlacementOrder) &&
			Session.GetMechanics().TryGetConveyorDirection(Bomb.Cell, ConveyorDirection))
		{
			const FIntPoint TargetCell = Bomb.Cell + ConveyorDirection;
			bool bBlocked = !Session.GetLevel().IsInside(TargetCell) ||
				Session.GetLevel().GetCell(TargetCell) != ENaiwaP3M2Cell::Floor ||
				Session.GetMechanics().BlocksMovement(TargetCell, CurrentTick) ||
				TargetCell == PlayerCell || LivingEnemyCells.Contains(TargetCell);
			for (int32 OtherIndex = 0; OtherIndex < ActiveBombs.Num() && !bBlocked; ++OtherIndex)
			{
				if (OtherIndex != BombIndex && ActiveBombs[OtherIndex].Cell == TargetCell)
				{
					bBlocked = true;
				}
			}
			BombNextConveyorTick.Add(Bomb.PlacementOrder, CurrentTick + BombConveyorIntervalTicks);
			if (!bBlocked)
			{
				Bomb.Cell = TargetCell;
				if (const TWeakObjectPtr<ANaiwaP3BombActor>* Visual = BombVisuals.Find(Bomb.PlacementOrder))
				{
					if (Visual->IsValid()) { Visual->Get()->SetWorldPosition(GridToWorld(TargetCell)); }
				}
			}
		}
	}
	FNaiwaP3M2EnemyAI::BuildHazardForecast(Session.GetLevel(), ActiveBombs,
		ChainDelayTicks, FlameDurationTicks, HazardForecast);
	for (int32 EnemyIndex = 0; EnemyIndex < EnemyCells.Num(); ++EnemyIndex)
	{
		if (!IsEnemyAlive(EnemyIndex))
		{
			EnemyIntents[EnemyIndex] = FNaiwaP3M2EnemyIntent();
			continue;
		}
		TArray<FIntPoint> FetchItemCells;
		Session.GetEnemyFetchItemCells(FetchItemCells);
		const float EnemySpeedMultiplier = EnemyLootStates.IsValidIndex(EnemyIndex) ?
			EnemyLootStates[EnemyIndex].SpeedMultiplier : 1.0f;
		const FNaiwaP3M2EnemyIntent Intent = Session.ThinkEnemy(
			EnemyIndex, EnemyCells[EnemyIndex], PlayerCell, HazardForecast,
			FetchItemCells, EnemySpeedMultiplier, bRageEscape);
		const ENaiwaP3M2EnemyAIState PreviousState = EnemyIntents.IsValidIndex(EnemyIndex) ?
			EnemyIntents[EnemyIndex].State : ENaiwaP3M2EnemyAIState::Patrol;
		EnemyIntents[EnemyIndex] = Intent;
		if (Intent.State == ENaiwaP3M2EnemyAIState::Chase && PreviousState != ENaiwaP3M2EnemyAIState::Chase &&
			Session.GetLevel().Enemies.IsValidIndex(EnemyIndex))
		{
			if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
			{
				if (Session.GetLevel().Enemies[EnemyIndex].Kind == ENaiwaP3M2EnemyKind::NaiCha)
					Audio->PlayP3AudioAtLocation(TEXT("SFX_P2_Miss"), GridToWorld(EnemyCells[EnemyIndex]), -4.0f);
				else if (Session.GetLevel().Enemies[EnemyIndex].Kind == ENaiwaP3M2EnemyKind::NaiSha)
					Audio->PlayP3AudioAtLocation(TEXT("SFX_P3_EnemyCharge"), GridToWorld(EnemyCells[EnemyIndex]), 0.0f);
			}
		}
		if (Intent.bDropBomb)
		{
			int32 OwnedBombCount = 0;
			for (const FNaiwaP3M2BombForecastInput& Bomb : ActiveBombs)
			{
				OwnedBombCount += !Bomb.bPlayerOwned && Bomb.OwnerEnemyIndex == EnemyIndex ? 1 : 0;
			}
			const FNaiwaP3M2EnemyLootState& Loot = EnemyLootStates[EnemyIndex];
			if (OwnedBombCount < Loot.BombCapacity)
			{
				EnemyBombRequests.AddUnique(EnemyIndex);
				SpawnBombAt(EnemyCells[EnemyIndex], Session.GetCurrentTick() + BombFuseTicks,
					ENaiwaP3BombType::Standard, Loot.BombRange, true, EnemyIndex);
			}
		}
		if (Intent.bRangedAttack)
		{
			EnemyRangedAttackRequests.AddUnique(EnemyIndex);
		}
		if (bRageEscape && Session.GetLevel().Enemies[EnemyIndex].Kind == ENaiwaP3M2EnemyKind::NaiCha &&
			RageBombNextTicks.IsValidIndex(EnemyIndex) &&
			RageBombNextTicks[EnemyIndex] != INDEX_NONE &&
			Session.GetCurrentTick() >= RageBombNextTicks[EnemyIndex])
		{
			SpawnBombAt(EnemyCells[EnemyIndex], Session.GetCurrentTick() + BombFuseTicks,
				ENaiwaP3BombType::Standard, 1, true, EnemyIndex);
			RageBombNextTicks[EnemyIndex] = Session.GetCurrentTick() + 60;
		}
		if (Intent.bHasNextCell && IsEnemyDestinationOpen(EnemyIndex, Intent.NextCell))
		{
			const FIntPoint PreviousCell = EnemyCells[EnemyIndex];
			EnemyCells[EnemyIndex] = Intent.NextCell;
			EnemyVisualFromCells[EnemyIndex] = PreviousCell;
			EnemyVisualToCells[EnemyIndex] = Intent.NextCell;
			EnemyVisualProgress[EnemyIndex] = 0.0f;
			const ENaiwaP3M2EnemyKind Kind = Session.GetLevel().Enemies[EnemyIndex].Kind;
			const float BaseSpeed = Kind == ENaiwaP3M2EnemyKind::NaiSha ? 4.4f : 3.2f;
			const float Speed = BaseSpeed * EnemySpeedMultiplier *
				(bRageEscape && Kind == ENaiwaP3M2EnemyKind::NaiCha ? 1.3f : 1.0f);
			EnemyVisualDuration[EnemyIndex] = 1.0f / Speed;
			Session.NotifyEnemyCellReached(EnemyIndex, Intent.NextCell);
			CollectEnemyItemAt(EnemyIndex, Intent.NextCell);
		}
	}
	for (const int32 EnemyIndex : EnemyRangedAttackRequests)
	{
		if (!Session.GetLevel().Enemies.IsValidIndex(EnemyIndex) ||
			Session.GetLevel().Enemies[EnemyIndex].Kind != ENaiwaP3M2EnemyKind::NaiYu)
		{
			continue;
		}
		const bool bAlreadyTargeting = SlowSplashes.ContainsByPredicate(
			[EnemyIndex](const FNaiwaP3M2SlowSplashState& Splash)
			{
				return Splash.OwnerEnemyIndex == EnemyIndex && !Splash.bImpacted;
			});
		if (!bAlreadyTargeting)
		{
			FNaiwaP3M2SlowSplashState& Splash = SlowSplashes.AddDefaulted_GetRef();
			Splash.CenterCell = PlayerCell;
			Splash.ImpactTick = Session.GetCurrentTick() + 8;
			Splash.ClearTick = Splash.ImpactTick + 4;
			Splash.OwnerEnemyIndex = EnemyIndex;
		}
	}
	for (FNaiwaP3M2SlowSplashState& Splash : SlowSplashes)
	{
		if (!Splash.bImpacted && Session.GetCurrentTick() >= Splash.ImpactTick)
		{
			const int32 Distance = FMath::Abs(PlayerCell.X - Splash.CenterCell.X) +
				FMath::Abs(PlayerCell.Y - Splash.CenterCell.Y);
			if (Distance <= 1)
			{
				PlayerSlowedUntilTick = FMath::Max(PlayerSlowedUntilTick,
					Session.GetCurrentTick() + 30);
			}
			Splash.bImpacted = true;
		}
	}
	SlowSplashes.RemoveAll([this](const FNaiwaP3M2SlowSplashState& Splash)
	{
		return Session.GetCurrentTick() >= Splash.ClearTick;
	});
	bool bPlayerInsideNaiYuRain = false;
	for (const FNaiwaP3M2SlowSplashState& Splash : SlowSplashes)
	{
		const int32 Distance = FMath::Abs(PlayerCell.X - Splash.CenterCell.X) +
			FMath::Abs(PlayerCell.Y - Splash.CenterCell.Y);
		if (IsEnemyAlive(Splash.OwnerEnemyIndex) && Distance <= 1)
		{
			bPlayerInsideNaiYuRain = true;
			break;
		}
	}
	if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
		Audio->SetP3RainAudio(bPlayerInsideNaiYuRain);
	ResolveDueBombs();
	ExpireFlames();
	Session.GetFog().UpdateVisibility(Session.GetLevel(), PlayerCell, bRageEscape);
	UpdateFogVisibility();
}

void UNaiwaP3M2RuntimeSubsystem::AdvancePlayerMovement(const float DeltaTime)
{
	if (RunState != ENaiwaP3M2RunState::Playing || PlayerDirection == FIntPoint::ZeroValue || DeltaTime <= 0.0f)
	{
		return;
	}
	PlayerMoveProgress += DeltaTime * GetPlayerSpeedMultiplier();
	int32 MovesThisTick = 0;
	while (PlayerMoveProgress + KINDA_SMALL_NUMBER >= PlayerCellSeconds && MovesThisTick < 2)
	{
		FString Message;
		if (!TryMovePlayer(PlayerDirection, Message))
		{
			PlayerMoveProgress = 0.0f;
			return;
		}
		PlayerMoveProgress -= PlayerCellSeconds;
		++MovesThisTick;
	}
}

void UNaiwaP3M2RuntimeSubsystem::UpdatePresentation(const float DeltaTime)
{
	if (PlayerVisualProgress < 1.0f)
	{
		PlayerVisualProgress = FMath::Min(1.0f, PlayerVisualProgress +
			(DeltaTime > 0.0f ? DeltaTime / FMath::Max(PlayerVisualDuration, 0.01f) : 0.0f));
	}
	if (PlayerPawn.IsValid())
	{
		PlayerPawn->SetGridWorldPosition(FMath::Lerp(PlayerVisualFromWorld, PlayerVisualToWorld,
			PlayerVisualProgress));
		PlayerPawn->SetDamageFeedback(GetDamageFeedbackFraction(), bLastDamageShielded);
	}

	for (int32 Index = 0; Index < EnemyActors.Num(); ++Index)
	{
		if (!EnemyActors[Index].IsValid()) { continue; }
		if (!IsEnemyAlive(Index))
		{
			EnemyActors[Index]->Destroy();
			EnemyActors[Index] = nullptr;
			continue;
		}
		if (EnemyVisualProgress.IsValidIndex(Index) && EnemyVisualProgress[Index] < 1.0f)
		{
			EnemyVisualProgress[Index] = FMath::Min(1.0f, EnemyVisualProgress[Index] +
				(DeltaTime > 0.0f ? DeltaTime / FMath::Max(EnemyVisualDuration[Index], 0.01f) : 0.0f));
		}
		const FVector From = GridToWorld(EnemyVisualFromCells[Index]);
		const FVector To = GridToWorld(EnemyVisualToCells[Index]);
		EnemyActors[Index]->SetGridPosition(FMath::Lerp(From, To, EnemyVisualProgress[Index]));
		if (!Session.GetLevel().Enemies.IsValidIndex(Index) || !EnemyCells.IsValidIndex(Index)) continue;

		const FNaiwaP3M2EnemyPlacement& Placement = Session.GetLevel().Enemies[Index];
		const TCHAR* PortraitName = nullptr;
		if (Placement.Kind == ENaiwaP3M2EnemyKind::NaiCha)
		{
			const int32 Distance = FMath::Abs(EnemyCells[Index].X - PlayerCell.X) +
				FMath::Abs(EnemyCells[Index].Y - PlayerCell.Y);
			PortraitName = Distance <= 1 ? TEXT("SPR_Naicha_Catch_A") : TEXT("SPR_Naicha_Chase_A");
		}
		else if (Placement.Kind == ENaiwaP3M2EnemyKind::NaiYu)
		{
			PortraitName = GetNaiYuLockSeconds() >= 0.0f ?
				TEXT("SPR_Naiyu_Attack_A") : TEXT("SPR_Naiyu_Neutral_A");
		}
		if (PortraitName)
		{
			const FString PortraitPath = FString::Printf(
				TEXT("/Game/Naiwa/P3/Portraits/Processed/%s.%s"), PortraitName, PortraitName);
			EnemyActors[Index]->SetPortrait(LoadObject<UTexture2D>(nullptr, *PortraitPath));
		}
	}

	for (const FNaiwaP3M2BombForecastInput& Bomb : ActiveBombs)
	{
		if (const TWeakObjectPtr<ANaiwaP3BombActor>* Visual = BombVisuals.Find(Bomb.PlacementOrder))
		{
			if (Visual->IsValid())
			{
				const float RemainingTicks = static_cast<float>(Bomb.FuseTick - Session.GetCurrentTick());
				const int32 FuseDuration = Bomb.FuseDurationTicks > 0 ? Bomb.FuseDurationTicks : BombFuseTicks;
				Visual->Get()->SetFuseFraction(RemainingTicks / static_cast<float>(FuseDuration));
				if (!Visual->Get()->IsVisualSliding())
				{
					Visual->Get()->SetWorldPosition(GridToWorld(Bomb.Cell));
				}
			}
		}
	}
}

FVector UNaiwaP3M2RuntimeSubsystem::GridToWorld(const FIntPoint& Cell, const float Z) const
{
	const FNaiwaP3M2SeededLevel& Level = Session.GetLevel();
	return FVector((Cell.X - (Level.GridWidth - 1) * 0.5f) * 100.0f,
		(Cell.Y - (Level.GridHeight - 1) * 0.5f) * 100.0f, Z);
}

void UNaiwaP3M2RuntimeSubsystem::SyncMapTile(const FIntPoint& Cell)
{
	if (!MapActor.IsValid() || !Session.GetLevel().IsInside(Cell)) { return; }
	ENaiwaP3Tile VisualTile = ENaiwaP3Tile::Floor;
	switch (Session.GetLevel().GetCell(Cell))
	{
	case ENaiwaP3M2Cell::HardBrick: VisualTile = ENaiwaP3Tile::Hard; break;
	case ENaiwaP3M2Cell::SoftBrick: VisualTile = ENaiwaP3Tile::Soft; break;
	case ENaiwaP3M2Cell::ItemBrick: VisualTile = ENaiwaP3Tile::Item; break;
	case ENaiwaP3M2Cell::Floor: default: VisualTile = ENaiwaP3Tile::Floor; break;
	}
	MapActor->SetTile(Cell.X, Cell.Y, VisualTile);
}

void UNaiwaP3M2RuntimeSubsystem::UpdateFogVisibility()
{
	if (!bSessionActive)
	{
		return;
	}
	const FNaiwaP3FogOfWar& Fog = Session.GetFog();
	const int32 ExploredCount = Fog.GetExploredCellCount();
	if (Session.GetLevel().LevelId == 32 && ExploredCount > PreviousFogExploredCount)
	{
		if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
			Audio->PlayP3Audio2D(TEXT("SFX_P3_FogReveal"), -13.0f);
	}
	PreviousFogExploredCount = ExploredCount;
	if (MapActor.IsValid())
	{
		for (int32 Y = 0; Y < Session.GetLevel().GridHeight; ++Y)
		{
			for (int32 X = 0; X < Session.GetLevel().GridWidth; ++X)
			{
				const FIntPoint Cell(X, Y);
				MapActor->SetFogState(X, Y, Fog.IsVisible(Cell), Fog.IsRemembered(Cell));
			}
		}
	}
	for (int32 EnemyIndex = 0; EnemyIndex < EnemyActors.Num(); ++EnemyIndex)
	{
		if (EnemyActors[EnemyIndex].IsValid() && EnemyCells.IsValidIndex(EnemyIndex))
		{
			EnemyActors[EnemyIndex]->SetActorHiddenInGame(!Fog.IsVisible(EnemyCells[EnemyIndex]));
		}
	}
	for (const FNaiwaP3M2BombForecastInput& Bomb : ActiveBombs)
	{
		if (const TWeakObjectPtr<ANaiwaP3BombActor>* Visual = BombVisuals.Find(Bomb.PlacementOrder))
		{
			if (Visual->IsValid())
			{
				Visual->Get()->SetActorHiddenInGame(!Fog.IsVisible(Bomb.Cell));
			}
		}
	}
	if (WorldPresentation.IsValid())
	{
		WorldPresentation->Sync(Session, PlayerCell, EnemyCells, EnemyAlive);
		if (!SlowSplashes.IsEmpty())
		{
			const FNaiwaP3M2SlowSplashState& Splash = SlowSplashes.Last();
			WorldPresentation->SetSlowSplash(Splash.CenterCell, true, Splash.bImpacted, Fog);
		}
		else
		{
			WorldPresentation->SetSlowSplash(FIntPoint::ZeroValue, false, false, Fog);
		}
	}
}

void UNaiwaP3M2RuntimeSubsystem::ResolveDueBombs()
{
	while (true)
	{
		int32 ChosenIndex = INDEX_NONE;
		for (int32 Index = 0; Index < ActiveBombs.Num(); ++Index)
		{
			const FNaiwaP3M2BombForecastInput& Candidate = ActiveBombs[Index];
			if (Candidate.FuseTick > Session.GetCurrentTick()) { continue; }
			if (ChosenIndex == INDEX_NONE)
			{
				ChosenIndex = Index;
				continue;
			}
			const FNaiwaP3M2BombForecastInput& Chosen = ActiveBombs[ChosenIndex];
			if (Candidate.FuseTick < Chosen.FuseTick ||
				(Candidate.FuseTick == Chosen.FuseTick &&
				 (M2CellIndex(Candidate.Cell, Session.GetLevel().GridWidth) < M2CellIndex(Chosen.Cell, Session.GetLevel().GridWidth) ||
				  (M2CellIndex(Candidate.Cell, Session.GetLevel().GridWidth) == M2CellIndex(Chosen.Cell, Session.GetLevel().GridWidth) &&
				   Candidate.PlacementOrder < Chosen.PlacementOrder))))
			{
				ChosenIndex = Index;
			}
		}
		if (ChosenIndex == INDEX_NONE) { break; }
		ResolveBombAtIndex(ChosenIndex);
	}
}

void UNaiwaP3M2RuntimeSubsystem::ResolveBombAtIndex(const int32 BombIndex)
{
	if (!ActiveBombs.IsValidIndex(BombIndex)) { return; }
	const FNaiwaP3M2BombForecastInput Bomb = ActiveBombs[BombIndex];
	ActiveBombs.RemoveAt(BombIndex, 1, EAllowShrinking::No);
	ExplosionFlashUntilTick = FMath::Max(ExplosionFlashUntilTick, Session.GetCurrentTick() + 5);
	TriggerExplosionFeedback(Bomb.Cell);
	if (const TWeakObjectPtr<ANaiwaP3BombActor>* Visual = BombVisuals.Find(Bomb.PlacementOrder))
	{
		if (Visual->IsValid()) { Visual->Get()->Destroy(); }
	}
	BombVisuals.Remove(Bomb.PlacementOrder);
	BombNextConveyorTick.Remove(Bomb.PlacementOrder);
	if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
	{
		FName Sound = TEXT("SFX_P3_Explode_Normal");
		float VolumeDb = 0.0f;
		switch (Bomb.Type)
		{
		case ENaiwaP3BombType::SoyBigCross: Sound = TEXT("SFX_P3_Explode_Soy"); break;
		case ENaiwaP3BombType::TeaRadial: Sound = TEXT("SFX_P3_Explode_TeaRadial"); VolumeDb = 8.0f; break;
		case ENaiwaP3BombType::RatSplit: Sound = TEXT("SFX_P3_Explode_MouseSplit"); VolumeDb = -2.0f; break;
		case ENaiwaP3BombType::RatFragment: Sound = TEXT("SFX_P3_Explode_SmallPop"); VolumeDb = -1.0f; break;
		default: break;
		}
		Audio->PlayP3Audio2D(Sound, VolumeDb);
	}

	const int32 EnemiesAliveBeforeExplosion = GetAliveEnemyCount();
	const TArray<FIntPoint> FlameCells = CalculateBlastCells(Bomb);
	for (const FIntPoint& Cell : FlameCells)
	{
		const ENaiwaP3M2Cell TileBeforeFire = Session.GetLevel().GetCell(Cell);
		for (const FNaiwaP3M2MechanicPlacement& Mechanic : Session.GetLevel().Mechanics)
		{
			if (Mechanic.Kind == ENaiwaP3M2MechanicKind::CrackedFloor && Mechanic.Cell == Cell)
			{
				if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
					Audio->PlayP3AudioAtLocation(TEXT("SFX_P3_FloorCrack"), GridToWorld(Cell), -1.0f);
				break;
			}
		}
		ReportFireAtCell(Cell);
		if (!Bomb.bPlayerOwned && (TileBeforeFire == ENaiwaP3M2Cell::SoftBrick ||
			TileBeforeFire == ENaiwaP3M2Cell::ItemBrick))
		{
			if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
				Audio->PlayP3AudioAtLocation(TEXT("SFX_P3_EnemyBrick"), GridToWorld(Cell), 3.0f);
		}
	}
	const int32 EnemiesKilled = EnemiesAliveBeforeExplosion - GetAliveEnemyCount();
	if (EnemiesKilled >= 3)
	{
		if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
			Audio->PlayP3Audio2D(TEXT("SFX_P3_ComboKill"), -3.0f,
				1.0f + 0.02f * FMath::Min(4, EnemiesKilled - 3));
	}
	for (FNaiwaP3M2BombForecastInput& OtherBomb : ActiveBombs)
	{
		if (FlameCells.Contains(OtherBomb.Cell))
		{
			OtherBomb.FuseTick = FMath::Min(OtherBomb.FuseTick,
				Session.GetCurrentTick() + ChainDelayTicks);
		}
	}

	if (Bomb.Type == ENaiwaP3BombType::RatSplit)
	{
		static const FIntPoint SplitDirections[] = {
			FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0)
		};
		int32 SplitCount = 0;
		for (const FIntPoint& Direction : SplitDirections)
		{
			const FIntPoint Target = Bomb.Cell + Direction;
			if (!Session.GetLevel().IsInside(Target) ||
				Session.GetLevel().GetCell(Target) != ENaiwaP3M2Cell::Floor ||
				FlameExpiryTick.Contains(Target))
			{
				continue;
			}
			if (!SpawnBombAt(Target, Session.GetCurrentTick() + ChainDelayTicks,
				ENaiwaP3BombType::RatFragment, 1, true, Bomb.OwnerEnemyIndex, Bomb.Cell))
			{
				continue;
			}
			if (++SplitCount == 3) { break; }
		}
	}
}

TArray<FIntPoint> UNaiwaP3M2RuntimeSubsystem::CalculateBlastCells(
	const FNaiwaP3M2BombForecastInput& Bomb)
{
	TArray<FIntPoint> Result;
	Result.Add(Bomb.Cell);
	const auto AppendCross = [this, &Bomb, &Result](const int32 Radius)
	{
		static const FIntPoint Directions[] = {
			FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0)
		};
		for (const FIntPoint& Direction : Directions)
		{
			for (int32 Distance = 1; Distance <= Radius; ++Distance)
			{
				const FIntPoint Cell = Bomb.Cell + Direction * Distance;
				if (!Session.GetLevel().IsInside(Cell)) { break; }
				if (Session.GetMechanics().ShouldConsumeFire(Cell, Session.GetCurrentTick()) ||
					Session.GetMechanics().BlocksMovement(Cell, Session.GetCurrentTick())) { break; }
				const ENaiwaP3M2Cell Tile = Session.GetLevel().GetCell(Cell);
				if (Tile == ENaiwaP3M2Cell::HardBrick) { break; }
				Result.AddUnique(Cell);
				if (Tile == ENaiwaP3M2Cell::SoftBrick || Tile == ENaiwaP3M2Cell::ItemBrick) { break; }
			}
		}
	};

	if (Bomb.Type == ENaiwaP3BombType::Standard)
	{
		AppendCross(Bomb.Range);
	}
	else if (Bomb.Type == ENaiwaP3BombType::SoyBigCross)
	{
		AppendCross(FMath::Max(1, FMath::CeilToInt(static_cast<float>(Bomb.Range) * 1.9f)));
	}
	else if (Bomb.Type == ENaiwaP3BombType::RatFragment)
	{
		AppendCross(1);
	}
	else if (Bomb.Type == ENaiwaP3BombType::TeaRadial)
	{
		TSet<int32> SoftStops;
		const auto GreatestCommonDivisor = [](int32 A, int32 B)
		{
			A = FMath::Abs(A); B = FMath::Abs(B);
			while (B != 0) { const int32 Remainder = A % B; A = B; B = Remainder; }
			return FMath::Max(A, 1);
		};
		for (int32 Y = -2; Y <= 2; ++Y)
		{
			for (int32 X = -2; X <= 2; ++X)
			{
				if ((X == 0 && Y == 0) || X * X + Y * Y > 4) { continue; }
				const int32 StepCount = GreatestCommonDivisor(X, Y);
				const FIntPoint Step(X / StepCount, Y / StepCount);
				for (int32 StepIndex = 1; StepIndex <= StepCount; ++StepIndex)
				{
					const FIntPoint Cell = Bomb.Cell + Step * StepIndex;
					if (!Session.GetLevel().IsInside(Cell)) { break; }
					if (Session.GetMechanics().ShouldConsumeFire(Cell, Session.GetCurrentTick()) ||
						Session.GetMechanics().BlocksMovement(Cell, Session.GetCurrentTick())) { break; }
					const ENaiwaP3M2Cell Tile = Session.GetLevel().GetCell(Cell);
					if (Tile == ENaiwaP3M2Cell::HardBrick) { break; }
					Result.AddUnique(Cell);
					const int32 Index = M2CellIndex(Cell, Session.GetLevel().GridWidth);
					if (Tile == ENaiwaP3M2Cell::SoftBrick || Tile == ENaiwaP3M2Cell::ItemBrick ||
						SoftStops.Contains(Index))
					{
						SoftStops.Add(Index);
						break;
					}
				}
			}
		}
	}
	return Result;
}

void UNaiwaP3M2RuntimeSubsystem::ExpireFlames()
{
	TArray<FIntPoint> Expired;
	for (const TPair<FIntPoint, int32>& Flame : FlameExpiryTick)
	{
		if (Flame.Value <= Session.GetCurrentTick()) { Expired.Add(Flame.Key); }
	}
	for (const FIntPoint& Cell : Expired)
	{
		FlameExpiryTick.Remove(Cell);
		if (MapActor.IsValid()) { MapActor->SetFlame(Cell.X, Cell.Y, false); }
	}
}

bool UNaiwaP3M2RuntimeSubsystem::IsPlayerDestinationOpen(const FIntPoint& Cell) const
{
	if (!bSessionActive || !Session.GetLevel().IsInside(Cell) ||
		Session.GetLevel().GetCell(Cell) != ENaiwaP3M2Cell::Floor ||
		Session.GetMechanics().BlocksMovement(Cell, Session.GetCurrentTick()) ||
		FlameExpiryTick.Contains(Cell))
	{
		return false;
	}
	for (const FNaiwaP3M2BombForecastInput& Bomb : ActiveBombs)
	{
		if (Bomb.Cell == Cell) { return false; }
	}
	for (int32 EnemyIndex = 0; EnemyIndex < EnemyCells.Num(); ++EnemyIndex)
	{
		if (IsEnemyAlive(EnemyIndex) && EnemyCells[EnemyIndex] == Cell) { return false; }
	}
	return true;
}

bool UNaiwaP3M2RuntimeSubsystem::IsEnemyDestinationOpen(const int32 EnemyIndex,
	const FIntPoint& Cell) const
{
	if (!bSessionActive || !EnemyCells.IsValidIndex(EnemyIndex) ||
		!Session.GetLevel().IsInside(Cell) || Session.GetLevel().GetCell(Cell) != ENaiwaP3M2Cell::Floor ||
		Session.GetMechanics().BlocksMovement(Cell, Session.GetCurrentTick()) ||
		FlameExpiryTick.Contains(Cell) || Cell == PlayerCell)
	{
		return false;
	}
	for (int32 OtherIndex = 0; OtherIndex < EnemyCells.Num(); ++OtherIndex)
	{
		if (OtherIndex != EnemyIndex && IsEnemyAlive(OtherIndex) && EnemyCells[OtherIndex] == Cell) { return false; }
	}
	for (const FNaiwaP3M2BombForecastInput& Bomb : ActiveBombs)
	{
		if (Bomb.Cell == Cell) { return false; }
	}
	return true;
}

void UNaiwaP3M2RuntimeSubsystem::CollectEnemyItemAt(const int32 EnemyIndex, const FIntPoint& Cell)
{
	if (!EnemyLootStates.IsValidIndex(EnemyIndex) || !IsEnemyAlive(EnemyIndex)) return;
	ENaiwaP3LootType LootType;
	if (!Session.TryCollectItemForEnemyAt(Cell, LootType)) return;
	if (WorldPresentation.IsValid()) WorldPresentation->RemoveItemAt(Cell);
	FNaiwaP3M2EnemyLootState& Loot = EnemyLootStates[EnemyIndex];
	switch (LootType)
	{
	case ENaiwaP3LootType::BombCapacity:
		Loot.BombCapacity = FMath::Min(FNaiwaP3ItemEffects::MaxBombCapacity, Loot.BombCapacity + 1);
		break;
	case ENaiwaP3LootType::BombRange:
		Loot.BombRange = FMath::Min(FNaiwaP3ItemEffects::MaxBombRange, Loot.BombRange + 1);
		break;
	case ENaiwaP3LootType::SpeedBoost:
		Loot.SpeedMultiplier = FMath::Min(FNaiwaP3ItemEffects::MaxSpeedMultiplier,
			Loot.SpeedMultiplier + FNaiwaP3ItemEffects::SpeedUpgradeStep);
		break;
	case ENaiwaP3LootType::Shield:
		Loot.ShieldCharges = FMath::Min(FNaiwaP3ItemEffects::MaxShieldCharges,
			Loot.ShieldCharges + 1);
		break;
	default:
		break;
	}
}

void UNaiwaP3M2RuntimeSubsystem::ProcessPlayerLanding(const FIntPoint& Cell, FString& OutMessage)
{
	const int32 RemainingEggsBeforePickup = Session.GetRemainingEggCount();
	if (Session.TryCollectEggAt(Cell))
	{
		if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld())) Audio->PlayP3Audio2D(TEXT("SFX_P3_GoldEggPick"), -2.0f);
		if (WorldPresentation.IsValid()) WorldPresentation->MarkEggCollected(Cell);
		const FString EggMessage = FString::Printf(TEXT("金蛋 %d / %d"), Session.GetCollectedEggCount(),
			Session.GetCollectedEggCount() + Session.GetRemainingEggCount());
		OutMessage = OutMessage.IsEmpty() ? EggMessage : OutMessage + TEXT("；") + EggMessage;
		if (Session.GetLevel().LevelId == 32 && RemainingEggsBeforePickup > 0 &&
			Session.GetRemainingEggCount() == 0 && !bRageEscape)
		{
			bRageEscape = true;
			RageStartTick = Session.GetCurrentTick();
			for (int32 EnemyIndex = 0; EnemyIndex < Session.GetLevel().Enemies.Num(); ++EnemyIndex)
			{
				if (Session.GetLevel().Enemies[EnemyIndex].Kind == ENaiwaP3M2EnemyKind::NaiCha &&
					RageBombNextTicks.IsValidIndex(EnemyIndex))
				{
					RageBombNextTicks[EnemyIndex] = RageStartTick + 60;
				}
			}
			const FString RageMessage = TEXT("狂暴逃亡开始，奶察加速并开始投弹，返回出口");
			if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld())) Audio->SetP3RageAudio(true);
			OutMessage += TEXT("；") + RageMessage;
		}
	}
	FString ItemMessage;
	if (Session.TryCollectItemAt(Cell, ItemMessage))
	{
		if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld())) Audio->PlayP3Audio2D(TEXT("SFX_P3_GoldEggPick"), -2.0f, 1.02f);
		if (WorldPresentation.IsValid()) WorldPresentation->RemoveItemAt(Cell);
		OutMessage = OutMessage.IsEmpty() ? ItemMessage : OutMessage + TEXT("；") + ItemMessage;
	}
	if (Cell == Session.GetLevel().ExitCell)
	{
		if (Session.GetRemainingEggCount() == 0)
		{
			RunState = ENaiwaP3M2RunState::Won;
			if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
			{
				Audio->SetP3RainAudio(false);
				Audio->PlayP3LevelClearAudio();
			}
			const FString ExitMessage = TEXT("集齐金蛋，成功撤离");
			OutMessage = OutMessage.IsEmpty() ? ExitMessage : OutMessage + TEXT("；") + ExitMessage;
		}
		else
		{
			const FString ExitMessage = FString::Printf(TEXT("出口尚未开启，还差 %d 枚金蛋"),
				Session.GetRemainingEggCount());
			OutMessage = OutMessage.IsEmpty() ? ExitMessage : OutMessage + TEXT("；") + ExitMessage;
		}
	}
	if (!OutMessage.IsEmpty()) SetTransientEvent(OutMessage, 24);
}

void UNaiwaP3M2RuntimeSubsystem::DamagePlayer(const bool bCanUseShield, const TCHAR* SourceLabel)
{
	if (!bSessionActive)
	{
		return;
	}
	if (Session.GetCurrentTick() < PlayerInvulnerableUntilTick)
	{
		bLastDamageShielded = false;
		DamageFeedbackUntilTick = FMath::Max(DamageFeedbackUntilTick, Session.GetCurrentTick() + 5);
		DamageFeedbackMessage = FString::Printf(TEXT("%s · 闪避成功"), SourceLabel ? SourceLabel : TEXT("攻击"));
		return;
	}
	PlayerInvulnerableUntilTick = Session.GetCurrentTick() + 20;
	DamageFeedbackUntilTick = Session.GetCurrentTick() + 15;
	const bool bShieldBlocked = bCanUseShield && Session.GetPlayerItems().ConsumeExplosionShield();
	bLastDamageShielded = bShieldBlocked;
	if (bShieldBlocked)
	{
		DamageFeedbackMessage = FString::Printf(TEXT("护盾挡住%s"), SourceLabel ? SourceLabel : TEXT("攻击"));
		return;
	}
	if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld())) Audio->PlayP3Audio2D(TEXT("SFX_P3_PlayerHurt"), 0.0f);
	DamageFeedbackMessage = FString::Printf(TEXT("%s！生命 -1"), SourceLabel ? SourceLabel : TEXT("受到攻击"));
	++PlayerHitCount;
	PlayerLives = FMath::Max(0, PlayerLives - 1);
	if (PlayerLives == 0)
	{
		RunState = ENaiwaP3M2RunState::Defeated;
		if (ANaiwaP3M2GameMode* Audio = P3AudioGameMode(GetWorld()))
		{
			Audio->SetP3ConveyorAudio(false);
		Audio->SetP3RainAudio(false);
			Audio->SetP3RageAudio(false);
			Audio->PlayP3Audio2D(TEXT("SFX_P3_PlayerDie"), -8.0f);
			Audio->PlayP3FailureAudio();
		}
		PlayerDirection = FIntPoint::ZeroValue;
	}
}

int32 UNaiwaP3M2RuntimeSubsystem::GetAliveEnemyCount() const
{
	int32 Count = 0;
	for (const uint8 bAlive : EnemyAlive)
	{
		Count += bAlive ? 1 : 0;
	}
	return Count;
}
