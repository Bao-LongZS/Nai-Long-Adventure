#include "NaiwaP3GridSubsystem.h"

#include "HAL/PlatformMisc.h"
#include "Misc/Crc.h"
#include "NaiwaP3BombActor.h"
#include "NaiwaP3M1Settings.h"
#include "NaiwaP3PlayerPawn.h"

namespace
{
	constexpr int32 M1Columns = 15;
	constexpr int32 M1Rows = 11;
	const FIntPoint Directions[] = {
		FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0)
	};
}

void UNaiwaP3GridSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UNaiwaP3GridSubsystem::Deinitialize()
{
	ClearRuntimeState();
	bM1Initialized = false;
	Super::Deinitialize();
}

TStatId UNaiwaP3GridSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNaiwaP3GridSubsystem, STATGROUP_Tickables);
}

void UNaiwaP3GridSubsystem::InitializeM1()
{
	if (bM1Initialized)
	{
		return;
	}
	LoadM1Tuning();
	BuildM1Rows();
	BuildPatrolPath();
	Tiles.SetNum(M1Columns * M1Rows);
	for (int32 Y = 0; Y < M1Rows; ++Y)
	{
		for (int32 X = 0; X < M1Columns; ++X)
		{
			const TCHAR Code = Rows[Y][X];
			Tiles[Y * M1Columns + X] = Code == TCHAR('H') ? ENaiwaP3Tile::Hard :
				(Code == TCHAR('S') ? ENaiwaP3Tile::Soft : ENaiwaP3Tile::Floor);
		}
	}
	bM1Initialized = true;
	ResetM1();
}

void UNaiwaP3GridSubsystem::LoadM1Tuning()
{
	const UNaiwaP3M1Settings* Settings = GetDefault<UNaiwaP3M1Settings>();
	if (!Settings)
	{
		return;
	}
	PlayerCellSeconds = 1.0f / FMath::Clamp(Settings->PlayerCellsPerSecond, 0.5f, 10.0f);
	EnemyCellSeconds = 1.0f / FMath::Clamp(Settings->PatrolCellsPerSecond, 0.1f, 8.0f);
	MaxActiveBombs = FMath::Clamp(Settings->BombCapacity, 1, 5);
	BombPlacementIntervalTicks = FMath::Max(1, FMath::RoundToInt(
		FMath::Clamp(Settings->BombPlacementIntervalSeconds, 0.1f, 2.0f) / FixedStepSeconds));
	BombRange = FMath::Clamp(Settings->BombRange, 1, 8);
	BombFuseTicks = FMath::Max(1, FMath::RoundToInt(FMath::Clamp(Settings->BombFuseSeconds, 0.5f, 8.0f) / FixedStepSeconds));
	ChainDelayTicks = FMath::Max(1, FMath::RoundToInt(FMath::Clamp(Settings->ChainDelaySeconds, 0.1f, 1.0f) / FixedStepSeconds));
	FlameLifeTicks = FMath::Max(1, FMath::RoundToInt(FMath::Clamp(Settings->FlameDurationSeconds, 0.1f, 2.0f) / FixedStepSeconds));
}

void UNaiwaP3GridSubsystem::BuildM1Rows()
{
	Rows = {
		TEXT("HHHHHHHHHHHHHHH"),
		TEXT("H.............H"),
		TEXT("HSSS.......SSSH"),
		TEXT("HSSS.......SSSH"),
		TEXT("H.............H"),
		TEXT("H.....SSSS....H"),
		TEXT("H.............H"),
		TEXT("HSSS.......SSSH"),
		TEXT("HSSS.......SSSH"),
		TEXT("H.@.........S.H"),
		TEXT("HHHHHHHHHHHHHHH")
	};
}

void UNaiwaP3GridSubsystem::BuildPatrolPath()
{
	PatrolPath.Reset();
	for (int32 X = 4; X <= 10; ++X)
	{
		PatrolPath.Add(FIntPoint(X, 1));
	}
	for (int32 Y = 2; Y <= 9; ++Y)
	{
		PatrolPath.Add(FIntPoint(10, Y));
	}
	for (int32 X = 9; X >= 4; --X)
	{
		PatrolPath.Add(FIntPoint(X, 9));
	}
	for (int32 Y = 8; Y >= 2; --Y)
	{
		PatrolPath.Add(FIntPoint(4, Y));
	}
}

void UNaiwaP3GridSubsystem::ResetM1()
{
	if (!bM1Initialized || !GetWorld())
	{
		return;
	}
	ClearRuntimeState();
	for (int32 Y = 0; Y < M1Rows; ++Y)
	{
		for (int32 X = 0; X < M1Columns; ++X)
		{
			const TCHAR Code = Rows[Y][X];
			Tiles[Y * M1Columns + X] = Code == TCHAR('H') ? ENaiwaP3Tile::Hard :
				(Code == TCHAR('S') ? ENaiwaP3Tile::Soft : ENaiwaP3Tile::Floor);
		}
	}
	MapActor = GetWorld()->SpawnActor<ANaiwaP3M1MapActor>();
	if (MapActor)
	{
		MapActor->BuildMap(Rows);
	}
	EnemyActor = GetWorld()->SpawnActor<ANaiwaP3M1EnemyActor>();
	PlayerCell = SpawnCell;
	PlayerTargetCell = SpawnCell;
	EnemyCell = PatrolPath.IsEmpty() ? FIntPoint(4, 1) : PatrolPath[0];
	EnemyTargetCell = EnemyCell;
	if (PlayerPawn.IsValid())
	{
		PlayerPawn->SetGridWorldPosition(GridToWorld(PlayerCell));
	}
	if (EnemyActor)
	{
		EnemyActor->SetGridPosition(GridToWorld(EnemyCell, 0.0f));
	}
}

void UNaiwaP3GridSubsystem::ClearRuntimeState()
{
	for (const FNaiwaP3ActiveBomb& Bomb : ActiveBombs)
	{
		if (Bomb.Visual.IsValid())
		{
			Bomb.Visual->Destroy();
		}
	}
	ActiveBombs.Reset();
	FlameExpiryTicks.Reset();
	if (MapActor)
	{
		MapActor->Destroy();
		MapActor = nullptr;
	}
	if (EnemyActor)
	{
		EnemyActor->Destroy();
		EnemyActor = nullptr;
	}
	CurrentTick = 0;
	FixedAccumulator = 0.0f;
	PlayerMoveProgress = 0.0f;
	EnemyMoveProgress = 0.0f;
	PatrolIndex = 0;
	PlayerHitCount = 0;
	EnemyHitCount = 0;
	NextBombOrder = 1;
	NextPlayerBombPlacementTick = CurrentTick;
	SelectedBombType = ENaiwaP3BombType::Standard;
	bPlayerMoving = false;
	bEnemyMoving = false;
	PlayerDirection = FIntPoint::ZeroValue;
}

void UNaiwaP3GridSubsystem::RegisterPlayerPawn(ANaiwaP3PlayerPawn* Pawn)
{
	PlayerPawn = Pawn;
	if (Pawn && bM1Initialized)
	{
		Pawn->SetGridWorldPosition(GridToWorld(PlayerCell));
	}
}

void UNaiwaP3GridSubsystem::SetPlayerDirection(const FIntPoint& Direction)
{
	if (FMath::Abs(Direction.X) + FMath::Abs(Direction.Y) <= 1)
	{
		PlayerDirection = Direction;
	}
}

void UNaiwaP3GridSubsystem::SelectPlayerBombType(const ENaiwaP3BombType Type)
{
	SelectedBombType = Type == ENaiwaP3BombType::RatFragment ? ENaiwaP3BombType::Standard : Type;
}

bool UNaiwaP3GridSubsystem::PlacePlayerBomb()
{
	if (!bM1Initialized || CurrentTick < NextPlayerBombPlacementTick ||
		!IsInside(PlayerCell) || GetTile(PlayerCell) != ENaiwaP3Tile::Floor ||
		FlameExpiryTicks.Contains(CellIndex(PlayerCell)))
	{
		return false;
	}
	if (!SpawnBombAt(PlayerCell, CurrentTick + BombFuseTicks, false, SelectedBombType))
	{
		return false;
	}
	NextPlayerBombPlacementTick = CurrentTick + BombPlacementIntervalTicks;
	return true;
}

bool UNaiwaP3GridSubsystem::StageChainDrill()
{
	if (!bM1Initialized || !GetWorld())
	{
		return false;
	}
	ResetM1();
	for (int32 X = 6; X <= 9; ++X)
	{
		SetTile(FIntPoint(X, 5), ENaiwaP3Tile::Floor);
	}
	for (int32 X = 6; X <= 9; ++X)
	{
		const int32 FuseTick = X == 6 ? CurrentTick + BombFuseTicks : CurrentTick + BombFuseTicks * 100;
		if (!SpawnBombAt(FIntPoint(X, 5), FuseTick, true))
		{
			return false;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("NAIWA_P3_M1_CHAIN_DRILL staged=4 first_fuse_ticks=%d chain_delay_ticks=%d"),
		BombFuseTicks, ChainDelayTicks);
	return true;
}

bool UNaiwaP3GridSubsystem::SpawnBombAt(const FIntPoint& Cell, const int32 FuseTick,
	const bool bIgnorePlayerLimit, const ENaiwaP3BombType Type, const int32 RangeOverride)
{
	if (!IsInside(Cell) || GetTile(Cell) != ENaiwaP3Tile::Floor ||
		FlameExpiryTicks.Contains(CellIndex(Cell)) ||
		(!bIgnorePlayerLimit && ActiveBombs.Num() >= MaxActiveBombs))
	{
		return false;
	}
	for (const FNaiwaP3ActiveBomb& Existing : ActiveBombs)
	{
		if (Existing.Cell == Cell)
		{
			return false;
		}
	}
	ANaiwaP3BombActor* BombActor = GetWorld()->SpawnActor<ANaiwaP3BombActor>();
	if (!BombActor)
	{
		return false;
	}
	BombActor->SetWorldPosition(GridToWorld(Cell));
	BombActor->SetBombType(Type);
	FNaiwaP3ActiveBomb& Bomb = ActiveBombs.AddDefaulted_GetRef();
	Bomb.Cell = Cell;
	Bomb.FuseTick = FuseTick;
	Bomb.PlacementOrder = NextBombOrder++;
	Bomb.Type = Type;
	Bomb.Range = RangeOverride == INDEX_NONE ? BombRange : FMath::Max(1, RangeOverride);
	Bomb.Visual = BombActor;
	return true;
}

void UNaiwaP3GridSubsystem::Tick(const float DeltaTime)
{
	if (!bM1Initialized || DeltaTime <= 0.0f)
	{
		return;
	}
	FixedAccumulator += DeltaTime;
	while (FixedAccumulator >= FixedStepSeconds)
	{
		FixedAccumulator -= FixedStepSeconds;
		AdvanceFixedStep();
	}
	UpdatePresentation(FixedAccumulator / FixedStepSeconds);
}

void UNaiwaP3GridSubsystem::AdvanceFixedStep()
{
	++CurrentTick;
	AdvancePlayer(FixedStepSeconds);
	AdvanceEnemy(FixedStepSeconds);
	ResolveDueBombs();
	ExpireFlames();
}

void UNaiwaP3GridSubsystem::AdvancePlayer(const float StepSeconds)
{
	float Remaining = StepSeconds;
	while (Remaining > KINDA_SMALL_NUMBER)
	{
		if (!bPlayerMoving)
		{
			if (PlayerDirection.X == 0 && PlayerDirection.Y == 0)
			{
				return;
			}
			const FIntPoint Candidate = PlayerCell + PlayerDirection;
			if (!CanEnterPlayerCell(Candidate))
			{
				return;
			}
			PlayerTargetCell = Candidate;
			PlayerMoveProgress = 0.0f;
			bPlayerMoving = true;
		}

		const float TimeToCell = PlayerCellSeconds - PlayerMoveProgress;
		const float Travel = FMath::Min(Remaining, TimeToCell);
		PlayerMoveProgress += Travel;
		Remaining -= Travel;
		if (PlayerMoveProgress + KINDA_SMALL_NUMBER >= PlayerCellSeconds)
		{
			PlayerCell = PlayerTargetCell;
			PlayerMoveProgress = 0.0f;
			bPlayerMoving = false;
		}
	}
}

void UNaiwaP3GridSubsystem::AdvanceEnemy(const float StepSeconds)
{
	if (PatrolPath.Num() < 2)
	{
		return;
	}
	float Remaining = StepSeconds;
	while (Remaining > KINDA_SMALL_NUMBER)
	{
		if (!bEnemyMoving)
		{
			const int32 NextIndex = (PatrolIndex + 1) % PatrolPath.Num();
			const FIntPoint Candidate = PatrolPath[NextIndex];
			if (GetTile(Candidate) != ENaiwaP3Tile::Floor ||
				FlameExpiryTicks.Contains(CellIndex(Candidate)))
			{
				return;
			}
			EnemyTargetCell = Candidate;
			EnemyMoveProgress = 0.0f;
			bEnemyMoving = true;
		}

		const float TimeToCell = EnemyCellSeconds - EnemyMoveProgress;
		const float Travel = FMath::Min(Remaining, TimeToCell);
		EnemyMoveProgress += Travel;
		Remaining -= Travel;
		if (EnemyMoveProgress + KINDA_SMALL_NUMBER >= EnemyCellSeconds)
		{
			EnemyCell = EnemyTargetCell;
			PatrolIndex = (PatrolIndex + 1) % PatrolPath.Num();
			EnemyMoveProgress = 0.0f;
			bEnemyMoving = false;
		}
	}
}

void UNaiwaP3GridSubsystem::ResolveDueBombs()
{
	while (true)
	{
		int32 ChosenIndex = INDEX_NONE;
		for (int32 Index = 0; Index < ActiveBombs.Num(); ++Index)
		{
			const FNaiwaP3ActiveBomb& Candidate = ActiveBombs[Index];
			if (Candidate.FuseTick > CurrentTick)
			{
				continue;
			}
			if (ChosenIndex == INDEX_NONE)
			{
				ChosenIndex = Index;
				continue;
			}
			const FNaiwaP3ActiveBomb& Chosen = ActiveBombs[ChosenIndex];
			const int32 CandidateCell = CellIndex(Candidate.Cell);
			const int32 ChosenCell = CellIndex(Chosen.Cell);
			if (Candidate.FuseTick < Chosen.FuseTick ||
				(Candidate.FuseTick == Chosen.FuseTick && CandidateCell < ChosenCell) ||
				(Candidate.FuseTick == Chosen.FuseTick && CandidateCell == ChosenCell &&
					Candidate.PlacementOrder < Chosen.PlacementOrder))
			{
				ChosenIndex = Index;
			}
		}
		if (ChosenIndex == INDEX_NONE)
		{
			break;
		}
		ResolveBombAtIndex(ChosenIndex);
	}
}

void UNaiwaP3GridSubsystem::ResolveBombAtIndex(const int32 BombIndex)
{
	if (!ActiveBombs.IsValidIndex(BombIndex))
	{
		return;
	}
	const FNaiwaP3ActiveBomb Bomb = ActiveBombs[BombIndex];
	ActiveBombs.RemoveAt(BombIndex, 1, EAllowShrinking::No);
	if (Bomb.Visual.IsValid())
	{
		Bomb.Visual->Destroy();
	}

	TArray<FIntPoint> FlameCells;
	FlameCells.Add(Bomb.Cell);
	const auto AppendCross = [this, &Bomb, &FlameCells](const int32 Radius)
	{
		for (const FIntPoint& Direction : Directions)
		{
			for (int32 Distance = 1; Distance <= Radius; ++Distance)
			{
				const FIntPoint Cell = Bomb.Cell + Direction * Distance;
				if (!IsInside(Cell))
				{
					break;
				}
				const ENaiwaP3Tile Tile = GetTile(Cell);
				if (Tile == ENaiwaP3Tile::Hard)
				{
					break;
				}
				FlameCells.Add(Cell);
				if (Tile == ENaiwaP3Tile::Soft)
				{
					SetTile(Cell, ENaiwaP3Tile::Floor);
					break;
				}
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
	else if (Bomb.Type == ENaiwaP3BombType::TeaRadial)
	{
		TSet<int32> TeaSoftStops;
		const auto GreatestCommonDivisor = [](int32 A, int32 B)
		{
			A = FMath::Abs(A);
			B = FMath::Abs(B);
			while (B != 0)
			{
				const int32 Remainder = A % B;
				A = B;
				B = Remainder;
			}
			return FMath::Max(A, 1);
		};
		for (int32 Y = -2; Y <= 2; ++Y)
		{
			for (int32 X = -2; X <= 2; ++X)
			{
				if ((X == 0 && Y == 0) || X * X + Y * Y > 4)
				{
					continue;
				}
				const int32 StepCount = GreatestCommonDivisor(X, Y);
				const FIntPoint Step(X / StepCount, Y / StepCount);
				for (int32 StepIndex = 1; StepIndex <= StepCount; ++StepIndex)
				{
					const FIntPoint Cell = Bomb.Cell + Step * StepIndex;
					if (!IsInside(Cell))
					{
						break;
					}
					const ENaiwaP3Tile Tile = GetTile(Cell);
					if (Tile == ENaiwaP3Tile::Hard)
					{
						break;
					}
					FlameCells.AddUnique(Cell);
					const int32 CellId = CellIndex(Cell);
					if (Tile == ENaiwaP3Tile::Soft || TeaSoftStops.Contains(CellId))
					{
						TeaSoftStops.Add(CellId);
						if (Tile == ENaiwaP3Tile::Soft)
						{
							SetTile(Cell, ENaiwaP3Tile::Floor);
						}
						break;
					}
				}
			}
		}
	}
	else if (Bomb.Type == ENaiwaP3BombType::RatFragment)
	{
		AppendCross(1);
	}
	// 分裂弹先点燃中心格，再按固定的上、右、下、左顺序生成最多三枚小弹。
	// 子弹在连锁延迟后爆炸，因此同 seed、同 tick 的结果保持稳定。

	TSet<int32> FlameIndices;
	for (const FIntPoint& FlameCell : FlameCells)
	{
		const int32 Index = CellIndex(FlameCell);
		FlameIndices.Add(Index);
		FlameExpiryTicks.FindOrAdd(Index) = CurrentTick + FlameLifeTicks;
		if (MapActor)
		{
			MapActor->SetFlame(FlameCell.X, FlameCell.Y, true);
		}
	}

	for (FNaiwaP3ActiveBomb& OtherBomb : ActiveBombs)
	{
		if (FlameIndices.Contains(CellIndex(OtherBomb.Cell)))
		{
			OtherBomb.FuseTick = FMath::Min(OtherBomb.FuseTick, CurrentTick + ChainDelayTicks);
		}
	}
	if (Bomb.Type == ENaiwaP3BombType::RatSplit)
	{
		int32 SplitCount = 0;
		for (const FIntPoint& Direction : Directions)
		{
			const FIntPoint Target = Bomb.Cell + Direction;
			if (!IsInside(Target) || GetTile(Target) != ENaiwaP3Tile::Floor ||
				FlameExpiryTicks.Contains(CellIndex(Target)))
			{
				continue;
			}
			if (!SpawnBombAt(Target, CurrentTick + ChainDelayTicks, true,
				ENaiwaP3BombType::RatFragment, 1))
			{
				continue;
			}
			if (++SplitCount == 3)
			{
				break;
			}
		}
	}
	if (FlameIndices.Contains(CellIndex(PlayerCell)))
	{
		++PlayerHitCount;
	}
	if (FlameIndices.Contains(CellIndex(EnemyCell)))
	{
		++EnemyHitCount;
	}
	UE_LOG(LogTemp, Display, TEXT("NAIWA_P3_BOMB_EXPLODE tick=%d type=%s cell=%d,%d flame_cells=%d"),
		CurrentTick, GetNaiwaP3BombDisplayName(Bomb.Type), Bomb.Cell.X, Bomb.Cell.Y, FlameCells.Num());
}

void UNaiwaP3GridSubsystem::ExpireFlames()
{
	TArray<int32> Expired;
	for (const TPair<int32, int32>& Flame : FlameExpiryTicks)
	{
		if (Flame.Value <= CurrentTick)
		{
			Expired.Add(Flame.Key);
		}
	}
	Expired.Sort();
	for (const int32 Index : Expired)
	{
		if (Index >= 0 && Index < M1Columns * M1Rows && MapActor)
		{
			MapActor->SetFlame(Index % M1Columns, Index / M1Columns, false);
		}
		FlameExpiryTicks.Remove(Index);
	}
}

bool UNaiwaP3GridSubsystem::CanEnterPlayerCell(const FIntPoint& Cell) const
{
	if (!IsInside(Cell) || GetTile(Cell) != ENaiwaP3Tile::Floor)
	{
		return false;
	}
	if (FlameExpiryTicks.Contains(CellIndex(Cell)))
	{
		return false;
	}
	for (const FNaiwaP3ActiveBomb& Bomb : ActiveBombs)
	{
		if (Bomb.Cell == Cell)
		{
			return false;
		}
	}
	return true;
}

bool UNaiwaP3GridSubsystem::IsInside(const FIntPoint& Cell) const
{
	return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < M1Columns && Cell.Y < M1Rows;
}

int32 UNaiwaP3GridSubsystem::CellIndex(const FIntPoint& Cell) const
{
	return Cell.Y * M1Columns + Cell.X;
}

ENaiwaP3Tile UNaiwaP3GridSubsystem::GetTile(const FIntPoint& Cell) const
{
	return IsInside(Cell) && Tiles.IsValidIndex(CellIndex(Cell)) ? Tiles[CellIndex(Cell)] : ENaiwaP3Tile::Hard;
}

void UNaiwaP3GridSubsystem::SetTile(const FIntPoint& Cell, const ENaiwaP3Tile Tile)
{
	if (!IsInside(Cell) || !Tiles.IsValidIndex(CellIndex(Cell)))
	{
		return;
	}
	Tiles[CellIndex(Cell)] = Tile;
	if (MapActor)
	{
		MapActor->SetTile(Cell.X, Cell.Y, Tile);
	}
}

FVector UNaiwaP3GridSubsystem::GridToWorld(const FIntPoint& Cell, const float Z) const
{
	return FVector((Cell.X - (M1Columns - 1) * 0.5f) * CellSize,
		(Cell.Y - (M1Rows - 1) * 0.5f) * CellSize, Z);
}

void UNaiwaP3GridSubsystem::UpdatePresentation(const float InterpolationAlpha)
{
	if (PlayerPawn.IsValid())
	{
		const FVector From = GridToWorld(PlayerCell);
		const FVector To = GridToWorld(bPlayerMoving ? PlayerTargetCell : PlayerCell);
		const float Alpha = bPlayerMoving ? FMath::Clamp(
			(PlayerMoveProgress + InterpolationAlpha * FixedStepSeconds) / PlayerCellSeconds, 0.0f, 1.0f) : 0.0f;
		PlayerPawn->SetGridWorldPosition(FMath::Lerp(From, To, Alpha));
	}
	if (EnemyActor)
	{
		const FVector From = GridToWorld(EnemyCell);
		const FVector To = GridToWorld(bEnemyMoving ? EnemyTargetCell : EnemyCell);
		const float Alpha = bEnemyMoving ? FMath::Clamp(
			(EnemyMoveProgress + InterpolationAlpha * FixedStepSeconds) / EnemyCellSeconds, 0.0f, 1.0f) : 0.0f;
		EnemyActor->SetGridPosition(FMath::Lerp(From, To, Alpha));
	}
	for (FNaiwaP3ActiveBomb& Bomb : ActiveBombs)
	{
		if (Bomb.Visual.IsValid())
		{
			const float RemainingTicks = static_cast<float>(Bomb.FuseTick - CurrentTick) - InterpolationAlpha;
			Bomb.Visual->SetFuseFraction(RemainingTicks / static_cast<float>(BombFuseTicks));
		}
	}
}

FString UNaiwaP3GridSubsystem::GetMapHash() const
{
	FString Joined;
	for (const FString& Row : Rows)
	{
		Joined += Row;
		Joined += TEXT("\n");
	}
	return FString::Printf(TEXT("%08X"), FCrc::StrCrc32(*Joined));
}

void UNaiwaP3GridSubsystem::SetPlayerCellForTest(const FIntPoint& Cell)
{
	PlayerCell = Cell;
	PlayerTargetCell = Cell;
	PlayerMoveProgress = 0.0f;
	bPlayerMoving = false;
	if (PlayerPawn.IsValid())
	{
		PlayerPawn->SetGridWorldPosition(GridToWorld(Cell));
	}
}

bool UNaiwaP3GridSubsystem::RunM1SelfTest(FString& OutDetails)
{
	TArray<FString> Checks;
	const auto Fail = [&OutDetails, this](const TCHAR* Reason)
	{
		ResetM1();
		OutDetails = FString::Printf(TEXT("FAIL %s mapHash=%s"), Reason, *GetMapHash());
		return false;
	};

	if (!bM1Initialized || Rows.Num() != M1Rows || PlayerPawn.IsStale())
	{
		return Fail(TEXT("startup_state"));
	}
	for (const FString& Row : Rows)
	{
		if (Row.Len() != M1Columns)
		{
			return Fail(TEXT("map_dimensions"));
		}
	}
	for (const FIntPoint& Cell : PatrolPath)
	{
		if (GetTile(Cell) != ENaiwaP3Tile::Floor)
		{
			return Fail(TEXT("patrol_path_blocked"));
		}
	}
	Checks.Add(TEXT("layout_and_patrol_clear"));

	ResetM1();
	SetPlayerDirection(FIntPoint(1, 0));
	const int32 PlayerMoveTicks = FMath::CeilToInt(PlayerCellSeconds / FixedStepSeconds);
	for (int32 Step = 0; Step < PlayerMoveTicks; ++Step) AdvanceFixedStep();
	SetPlayerDirection(FIntPoint::ZeroValue);
	if (PlayerCell != FIntPoint(3, 9))
	{
		return Fail(TEXT("grid_movement"));
	}
	Checks.Add(TEXT("player_move_matches_configured_speed"));
	SetPlayerCellForTest(FIntPoint(13, 5));
	SetPlayerDirection(FIntPoint(1, 0));
	for (int32 Step = 0; Step < PlayerMoveTicks; ++Step) AdvanceFixedStep();
	SetPlayerDirection(FIntPoint::ZeroValue);
	if (PlayerCell != FIntPoint(13, 5))
	{
		return Fail(TEXT("hard_wall_blocks_player"));
	}
	Checks.Add(TEXT("hard_wall_blocks_player"));

	ResetM1();
	const FIntPoint TestBombCell(FMath::Max(1, 6 - BombRange), 5);
	SetPlayerCellForTest(TestBombCell);
	if (!SpawnBombAt(TestBombCell, CurrentTick + BombFuseTicks, true))
	{
		return Fail(TEXT("bomb_placement"));
	}
	for (int32 Step = 0; Step < BombFuseTicks; ++Step) AdvanceFixedStep();
	if (GetTile(FIntPoint(6, 5)) != ENaiwaP3Tile::Floor ||
		GetTile(FIntPoint(7, 5)) != ENaiwaP3Tile::Soft || !FlameExpiryTicks.Contains(CellIndex(FIntPoint(5, 5))))
	{
		return Fail(TEXT("soft_brick_or_flame"));
	}
	Checks.Add(TEXT("bomb_fuse_and_soft_brick_stop"));
	for (int32 Step = 0; Step < FlameLifeTicks; ++Step) AdvanceFixedStep();
	if (!FlameExpiryTicks.IsEmpty())
	{
		return Fail(TEXT("flame_expiry"));
	}
	Checks.Add(TEXT("flame_expires_after_configured_duration"));

	ResetM1();
	for (int32 X = 6; X <= 9; ++X) SetTile(FIntPoint(X, 5), ENaiwaP3Tile::Floor);
	if (!SpawnBombAt(FIntPoint(6, 5), CurrentTick + 1, true) ||
		!SpawnBombAt(FIntPoint(6 + FMath::Min(BombRange, 2), 5), CurrentTick + BombFuseTicks, true))
	{
		return Fail(TEXT("chain_setup"));
	}
	AdvanceFixedStep();
	if (ActiveBombs.Num() != 1 || ActiveBombs[0].FuseTick != CurrentTick + ChainDelayTicks)
	{
		return Fail(TEXT("chain_scheduled_for_0_2s"));
	}
	for (int32 Step = 1; Step < ChainDelayTicks; ++Step) AdvanceFixedStep();
	if (ActiveBombs.Num() != 1)
	{
		return Fail(TEXT("chain_fired_early"));
	}
	AdvanceFixedStep();
	if (ActiveBombs.Num() != 0)
	{
		return Fail(TEXT("chain_did_not_fire"));
	}
	Checks.Add(TEXT("chain_detonates_after_configured_delay"));

	ResetM1();
	const int32 PatrolMoveTicks = FMath::CeilToInt(EnemyCellSeconds / FixedStepSeconds);
	for (int32 Step = 0; Step < PatrolMoveTicks; ++Step) AdvanceFixedStep();
	if (EnemyCell != PatrolPath[1])
	{
		return Fail(TEXT("enemy_patrol_step"));
	}
	Checks.Add(TEXT("enemy_patrol_moves_on_fixed_tick"));
	const FString MapHash = GetMapHash();
	ResetM1();
	OutDetails = FString::Printf(TEXT("PASS mapHash=%s checks=%s"), *MapHash,
		*FString::Join(Checks, TEXT(",")));
	return true;
}
