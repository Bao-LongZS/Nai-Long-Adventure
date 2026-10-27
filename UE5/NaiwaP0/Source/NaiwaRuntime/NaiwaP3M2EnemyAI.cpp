#include "NaiwaP3M2EnemyAI.h"

namespace
{
	const FIntPoint Directions[] = {
		FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0)
	};

	int32 CellIndex(const FNaiwaP3M2SeededLevel& Level, const FIntPoint& Cell)
	{
		return Cell.Y * Level.GridWidth + Cell.X;
	}

	bool IsWalkable(const FNaiwaP3M2SeededLevel& Level, const FIntPoint& Cell)
	{
		return Level.IsInside(Cell) && Level.GetCell(Cell) == ENaiwaP3M2Cell::Floor;
	}
}

void FNaiwaP3M2EnemyAI::Initialize(const TArray<FNaiwaP3M2EnemyPlacement>& Enemies,
	const int32 ScriptedAISeed)
{
	Brains.Reset(Enemies.Num());
	ScriptedAIRandom.Initialize(ScriptedAISeed);
	for (const FNaiwaP3M2EnemyPlacement& Enemy : Enemies)
	{
		FBrain& Brain = Brains.AddDefaulted_GetRef();
		Brain.Placement = Enemy;
		Brain.CachedIntent.NextCell = Enemy.Cell;
		Brain.CachedIntent.State = Enemy.Kind == ENaiwaP3M2EnemyKind::NaiHao ||
			Enemy.Kind == ENaiwaP3M2EnemyKind::NaiYu ? ENaiwaP3M2EnemyAIState::Guard :
			ENaiwaP3M2EnemyAIState::Patrol;
		Brain.State = Brain.CachedIntent.State;
		Brain.NextAttackTick = ScriptedAIRandom.RandRange(0, 20);
	}
}

void FNaiwaP3M2EnemyAI::BuildHazardForecast(const FNaiwaP3M2SeededLevel& Level,
	const TArray<FNaiwaP3M2BombForecastInput>& Bombs, const int32 ChainDelayTicks,
	const int32 FlameDurationTicks, TArray<FNaiwaP3M2HazardForecast>& OutHazards)
{
	struct FPendingBomb
	{
		FNaiwaP3M2BombForecastInput Bomb;
		bool bResolved = false;
	};

	OutHazards.Reset();
	TArray<ENaiwaP3M2Cell> ForecastCells = Level.Cells;
	TArray<FPendingBomb> PendingBombs;
	PendingBombs.Reserve(Bombs.Num() + 12);
	uint64 NextPlacementOrder = 1;
	for (const FNaiwaP3M2BombForecastInput& Bomb : Bombs)
	{
		FPendingBomb& Pending = PendingBombs.AddDefaulted_GetRef();
		Pending.Bomb = Bomb;
		NextPlacementOrder = FMath::Max(NextPlacementOrder, Bomb.PlacementOrder + 1);
	}

	const int32 SafeChainDelay = FMath::Max(1, ChainDelayTicks);
	const int32 SafeFlameDuration = FMath::Max(1, FlameDurationTicks);
	while (true)
	{
		int32 ChosenIndex = INDEX_NONE;
		for (int32 Index = 0; Index < PendingBombs.Num(); ++Index)
		{
			const FPendingBomb& Candidate = PendingBombs[Index];
			if (Candidate.bResolved) { continue; }
			if (ChosenIndex == INDEX_NONE)
			{
				ChosenIndex = Index;
				continue;
			}
			const FNaiwaP3M2BombForecastInput& A = Candidate.Bomb;
			const FNaiwaP3M2BombForecastInput& B = PendingBombs[ChosenIndex].Bomb;
			if (A.FuseTick < B.FuseTick || (A.FuseTick == B.FuseTick &&
				(CellIndex(Level, A.Cell) < CellIndex(Level, B.Cell) ||
				 (CellIndex(Level, A.Cell) == CellIndex(Level, B.Cell) && A.PlacementOrder < B.PlacementOrder))))
			{
				ChosenIndex = Index;
			}
		}
		if (ChosenIndex == INDEX_NONE) { break; }

		FPendingBomb& Chosen = PendingBombs[ChosenIndex];
		Chosen.bResolved = true;
		const FNaiwaP3M2BombForecastInput Bomb = Chosen.Bomb;
		TArray<FIntPoint> FlameCells;
		FlameCells.Add(Bomb.Cell);
		auto GetForecastCell = [&Level, &ForecastCells](const FIntPoint& Cell)
		{
			return Level.IsInside(Cell) && ForecastCells.IsValidIndex(CellIndex(Level, Cell)) ?
				ForecastCells[CellIndex(Level, Cell)] : ENaiwaP3M2Cell::HardBrick;
		};
		auto SetForecastCell = [&Level, &ForecastCells](const FIntPoint& Cell, const ENaiwaP3M2Cell Tile)
		{
			if (Level.IsInside(Cell) && ForecastCells.IsValidIndex(CellIndex(Level, Cell)))
			{
				ForecastCells[CellIndex(Level, Cell)] = Tile;
			}
		};
		auto AppendCross = [&Level, &FlameCells, &GetForecastCell, &SetForecastCell,
			&Bomb](const int32 Radius)
		{
			for (const FIntPoint& Direction : Directions)
			{
				for (int32 Distance = 1; Distance <= Radius; ++Distance)
				{
					const FIntPoint Cell = Bomb.Cell + Direction * Distance;
					if (!Level.IsInside(Cell)) { break; }
					const ENaiwaP3M2Cell Tile = GetForecastCell(Cell);
					if (Tile == ENaiwaP3M2Cell::HardBrick) { break; }
					FlameCells.AddUnique(Cell);
					if (Tile == ENaiwaP3M2Cell::SoftBrick || Tile == ENaiwaP3M2Cell::ItemBrick)
					{
						SetForecastCell(Cell, ENaiwaP3M2Cell::Floor);
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
				A = FMath::Abs(A); B = FMath::Abs(B);
				while (B != 0) { const int32 R = A % B; A = B; B = R; }
				return FMath::Max(A, 1);
			};
			for (int32 Y = -2; Y <= 2; ++Y)
			{
				for (int32 X = -2; X <= 2; ++X)
				{
					if ((X == 0 && Y == 0) || X * X + Y * Y > 4) { continue; }
					const int32 Steps = GreatestCommonDivisor(X, Y);
					const FIntPoint Step(X / Steps, Y / Steps);
					for (int32 StepIndex = 1; StepIndex <= Steps; ++StepIndex)
					{
						const FIntPoint Cell = Bomb.Cell + Step * StepIndex;
						if (!Level.IsInside(Cell)) { break; }
						const ENaiwaP3M2Cell Tile = GetForecastCell(Cell);
						if (Tile == ENaiwaP3M2Cell::HardBrick) { break; }
						FlameCells.AddUnique(Cell);
						const int32 CellId = CellIndex(Level, Cell);
						if (Tile == ENaiwaP3M2Cell::SoftBrick || Tile == ENaiwaP3M2Cell::ItemBrick ||
							TeaSoftStops.Contains(CellId))
						{
							TeaSoftStops.Add(CellId);
							if (Tile != ENaiwaP3M2Cell::Floor) { SetForecastCell(Cell, ENaiwaP3M2Cell::Floor); }
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

		for (const FIntPoint& Cell : FlameCells)
		{
			FNaiwaP3M2HazardForecast& Hazard = OutHazards.AddDefaulted_GetRef();
			Hazard.Cell = Cell;
			Hazard.ImpactTick = Bomb.FuseTick;
			Hazard.ClearTick = Bomb.FuseTick + SafeFlameDuration;
			Hazard.OwnerEnemyIndex = Bomb.OwnerEnemyIndex;
		}
		for (FPendingBomb& Other : PendingBombs)
		{
			if (!Other.bResolved && FlameCells.Contains(Other.Bomb.Cell))
			{
				Other.Bomb.FuseTick = FMath::Min(Other.Bomb.FuseTick, Bomb.FuseTick + SafeChainDelay);
			}
		}

		if (Bomb.Type == ENaiwaP3BombType::RatSplit)
		{
			int32 SplitCount = 0;
			for (const FIntPoint& Direction : Directions)
			{
				const FIntPoint Target = Bomb.Cell + Direction;
				if (!Level.IsInside(Target) || GetForecastCell(Target) != ENaiwaP3M2Cell::Floor)
				{
					continue;
				}
			FPendingBomb& Child = PendingBombs.AddDefaulted_GetRef();
			Child.Bomb.Cell = Target;
			Child.Bomb.FuseTick = Bomb.FuseTick + SafeChainDelay;
			Child.Bomb.PlacementOrder = NextPlacementOrder++;
			Child.Bomb.Type = ENaiwaP3BombType::RatFragment;
			Child.Bomb.Range = 1;
			Child.Bomb.OwnerEnemyIndex = Bomb.OwnerEnemyIndex;
			Child.Bomb.bPlayerOwned = Bomb.bPlayerOwned;
			if (++SplitCount == 3) { break; }
			}
		}
	}

	OutHazards.Sort([&Level](const FNaiwaP3M2HazardForecast& A, const FNaiwaP3M2HazardForecast& B)
	{
		if (A.ImpactTick != B.ImpactTick) { return A.ImpactTick < B.ImpactTick; }
		return CellIndex(Level, A.Cell) < CellIndex(Level, B.Cell);
	});
}

FNaiwaP3M2EnemyIntent FNaiwaP3M2EnemyAI::Think(const int32 EnemyIndex,
	const FNaiwaP3M2SeededLevel& Level, const FIntPoint& EnemyCell,
	const FIntPoint& PlayerCell, const TArray<FNaiwaP3M2HazardForecast>& Hazards,
	const TArray<FIntPoint>& FetchItemCells, const int32 CurrentTick,
	const float SpeedMultiplier, const bool bRageEscape)
{
	if (!Brains.IsValidIndex(EnemyIndex))
	{
		return FNaiwaP3M2EnemyIntent();
	}

	FBrain& Brain = Brains[EnemyIndex];
	if (Brain.CachedIntent.bHasNextCell && Brain.CachedIntent.NextCell == EnemyCell)
	{
		Brain.CachedIntent.bHasNextCell = false;
	}
	if (Brain.CachedIntent.bHasNextCell && Brain.CachedIntent.NextCell != EnemyCell)
	{
		FNaiwaP3M2EnemyIntent Cached = Brain.CachedIntent;
		Cached.bDropBomb = false;
		Cached.bRangedAttack = false;
		return Cached;
	}
	if (CurrentTick < Brain.NextThinkTick)
	{
		FNaiwaP3M2EnemyIntent Cached = Brain.CachedIntent;
		Cached.bDropBomb = false;
		Cached.bRangedAttack = false;
		return Cached;
	}

	Brain.NextThinkTick = CurrentTick + 2;
	const ENaiwaP3M2EnemyKind Kind = Brain.Placement.Kind;
	const int32 ElapsedTicks = FMath::Max(0, CurrentTick - Brain.LastMoveCreditTick);
	Brain.MoveCredit = FMath::Min(1.0f, Brain.MoveCredit + ElapsedTicks * 0.1f *
		MoveSpeedCellsPerSecond(Kind) * FMath::Max(0.1f, Brain.Placement.SpeedMultiplier) *
		FMath::Max(1.0f, SpeedMultiplier) *
		((bRageEscape && Kind == ENaiwaP3M2EnemyKind::NaiCha) ? 1.3f : 1.0f));
	Brain.LastMoveCreditTick = CurrentTick;
	Brain.CachedIntent = FNaiwaP3M2EnemyIntent();
	Brain.CachedIntent.NextCell = EnemyCell;
	const bool bCanSeePlayer = CanSeePlayer(Level, EnemyCell, PlayerCell, VisionRadius(Kind));
	const bool bIsChaser = Kind == ENaiwaP3M2EnemyKind::NaiCha || Kind == ENaiwaP3M2EnemyKind::NaiSha;
	const bool bIgnoresHazards = Kind == ENaiwaP3M2EnemyKind::NaiSha;
	TArray<FNaiwaP3M2HazardForecast> RelevantHazards;
	RelevantHazards.Reserve(Hazards.Num());
	for (const FNaiwaP3M2HazardForecast& Hazard : Hazards)
	{
		if (Hazard.OwnerEnemyIndex != EnemyIndex)
		{
			RelevantHazards.Add(Hazard);
		}
	}

	if (bCanSeePlayer)
	{
		Brain.LastSeenPlayerCell = PlayerCell;
		Brain.bHasLastSeenPlayer = true;
		Brain.SearchUntilTick = CurrentTick + 50;
	}

	if (Kind == ENaiwaP3M2EnemyKind::NaiHao || Kind == ENaiwaP3M2EnemyKind::NaiYu)
	{
		Brain.State = ENaiwaP3M2EnemyAIState::Guard;
		const int32 Distance = FMath::Abs(EnemyCell.X - PlayerCell.X) + FMath::Abs(EnemyCell.Y - PlayerCell.Y);
		const int32 AttackRange = Kind == ENaiwaP3M2EnemyKind::NaiHao ? 3 : 6;
		if (bCanSeePlayer && Distance <= AttackRange && CurrentTick >= Brain.NextAttackTick)
		{
			if (Kind == ENaiwaP3M2EnemyKind::NaiHao)
			{
				Brain.CachedIntent.bDropBomb = true;
				Brain.NextAttackTick = CurrentTick + ScriptedAIRandom.RandRange(25, 35);
			}
			else
			{
				Brain.CachedIntent.bRangedAttack = true;
				Brain.NextAttackTick = CurrentTick + 80;
			}
		}
		Brain.CachedIntent.State = Brain.State;
		return Brain.CachedIntent;
	}

	if (!bIgnoresHazards && IsThreatened(EnemyCell, RelevantHazards,
		CurrentTick + MoveIntervalTicks(Kind)))
	{
		Brain.State = ENaiwaP3M2EnemyAIState::Flee;
		if (Brain.MoveCredit >= 1.0f)
		{
			Brain.CachedIntent.NextCell = FindFleeCell(Level, EnemyCell, RelevantHazards,
				CurrentTick + MoveIntervalTicks(Kind));
			Brain.CachedIntent.bHasNextCell = Brain.CachedIntent.NextCell != EnemyCell;
			if (Brain.CachedIntent.bHasNextCell) { Brain.MoveCredit -= 1.0f; }
		}
		Brain.CachedIntent.State = Brain.State;
		return Brain.CachedIntent;
	}

	FIntPoint FetchGoal = EnemyCell;
	int32 FetchGoalDistance = MAX_int32;
	bool bHasFetchGoal = false;
	if (bIsChaser && !bCanSeePlayer)
	{
		for (const FIntPoint& ItemCell : FetchItemCells)
		{
			if (!IsWalkable(Level, ItemCell) || ItemCell == EnemyCell ||
				FindNextPathCell(Level, EnemyCell, ItemCell) == EnemyCell)
			{
				continue;
			}
			const int32 Distance = FMath::Abs(ItemCell.X - EnemyCell.X) +
				FMath::Abs(ItemCell.Y - EnemyCell.Y);
			if (Distance < FetchGoalDistance)
			{
				FetchGoal = ItemCell;
				FetchGoalDistance = Distance;
				bHasFetchGoal = true;
			}
		}
	}

	if (bCanSeePlayer)
	{
		Brain.State = ENaiwaP3M2EnemyAIState::Chase;
	}
	else if (bHasFetchGoal)
	{
		Brain.State = ENaiwaP3M2EnemyAIState::Fetch;
	}
	else if (Brain.State == ENaiwaP3M2EnemyAIState::Chase && Brain.bHasLastSeenPlayer)
	{
		Brain.State = CurrentTick < Brain.SearchUntilTick ?
			ENaiwaP3M2EnemyAIState::Search : ENaiwaP3M2EnemyAIState::Return;
	}
	else if (Brain.State == ENaiwaP3M2EnemyAIState::Search && CurrentTick >= Brain.SearchUntilTick)
	{
		Brain.State = ENaiwaP3M2EnemyAIState::Return;
	}
	else if (Brain.State == ENaiwaP3M2EnemyAIState::Return && EnemyCell == Brain.Placement.Cell)
	{
		Brain.State = ENaiwaP3M2EnemyAIState::Patrol;
		Brain.bHasLastSeenPlayer = false;
	}
	else if (Brain.State != ENaiwaP3M2EnemyAIState::Search &&
		Brain.State != ENaiwaP3M2EnemyAIState::Return)
	{
		Brain.State = ENaiwaP3M2EnemyAIState::Patrol;
	}

	if (Brain.MoveCredit >= 1.0f)
	{
		FIntPoint Goal = EnemyCell;
		if (Brain.State == ENaiwaP3M2EnemyAIState::Chase)
		{
			Goal = PlayerCell;
		}
		else if (Brain.State == ENaiwaP3M2EnemyAIState::Search && Brain.bHasLastSeenPlayer)
		{
			Goal = Brain.LastSeenPlayerCell;
		}
		else if (Brain.State == ENaiwaP3M2EnemyAIState::Return)
		{
			Goal = Brain.Placement.Cell;
		}
		else if (Brain.State == ENaiwaP3M2EnemyAIState::Fetch)
		{
			Goal = FetchGoal;
		}

		if (Brain.State == ENaiwaP3M2EnemyAIState::Patrol)
		{
			Brain.CachedIntent.NextCell = ChoosePatrolCell(Brain, Level, EnemyCell);
		}
		else
		{
			Brain.CachedIntent.NextCell = FindNextPathCell(Level, EnemyCell, Goal);
		}
		Brain.CachedIntent.bHasNextCell = Brain.CachedIntent.NextCell != EnemyCell;
		if (Brain.CachedIntent.bHasNextCell) { Brain.MoveCredit -= 1.0f; }

		if (!Brain.CachedIntent.bHasNextCell && Kind == ENaiwaP3M2EnemyKind::NaiSha && bCanSeePlayer)
		{
			Brain.CachedIntent.bDropBomb = FindSoftBrickToBomb(Level, EnemyCell, PlayerCell) != FIntPoint::ZeroValue;
		}
	}
	Brain.CachedIntent.State = Brain.State;
	return Brain.CachedIntent;
}

void FNaiwaP3M2EnemyAI::NotifyCellReached(const int32 EnemyIndex, const FIntPoint& Cell)
{
	if (!Brains.IsValidIndex(EnemyIndex))
	{
		return;
	}
	FBrain& Brain = Brains[EnemyIndex];
	if (Brain.CachedIntent.NextCell == Cell)
	{
		Brain.CachedIntent.bHasNextCell = false;
	}
}

bool FNaiwaP3M2EnemyAI::CanSeePlayer(const FNaiwaP3M2SeededLevel& Level,
	const FIntPoint& EnemyCell, const FIntPoint& PlayerCell, const int32 VisionRadius)
{
	const int32 Distance = FMath::Abs(EnemyCell.X - PlayerCell.X) + FMath::Abs(EnemyCell.Y - PlayerCell.Y);
	return Distance <= VisionRadius && HasLineOfSight(Level, EnemyCell, PlayerCell);
}

bool FNaiwaP3M2EnemyAI::HasLineOfSight(const FNaiwaP3M2SeededLevel& Level,
	const FIntPoint& Start, const FIntPoint& Target)
{
	int32 X = Start.X;
	int32 Y = Start.Y;
	const int32 DeltaX = FMath::Abs(Target.X - Start.X);
	const int32 StepX = Start.X < Target.X ? 1 : -1;
	const int32 DeltaY = -FMath::Abs(Target.Y - Start.Y);
	const int32 StepY = Start.Y < Target.Y ? 1 : -1;
	int32 Error = DeltaX + DeltaY;
	while (X != Target.X || Y != Target.Y)
	{
		const int32 TwiceError = 2 * Error;
		if (TwiceError >= DeltaY) { Error += DeltaY; X += StepX; }
		if (TwiceError <= DeltaX) { Error += DeltaX; Y += StepY; }
		if (X == Target.X && Y == Target.Y) { return true; }
		const ENaiwaP3M2Cell Tile = Level.GetCell(FIntPoint(X, Y));
		if (Tile != ENaiwaP3M2Cell::Floor) { return false; }
	}
	return true;
}

FIntPoint FNaiwaP3M2EnemyAI::FindNextPathCell(const FNaiwaP3M2SeededLevel& Level,
	const FIntPoint& Start, const FIntPoint& Goal)
{
	if (Start == Goal || !Level.IsInside(Start) || !Level.IsInside(Goal)) { return Start; }
	TArray<int32> Parent;
	Parent.Init(INDEX_NONE, Level.GridWidth * Level.GridHeight);
	TArray<FIntPoint> Queue;
	Queue.Reserve(Parent.Num());
	Queue.Add(Start);
	Parent[CellIndex(Level, Start)] = CellIndex(Level, Start);
	int32 ReadIndex = 0;
	while (ReadIndex < Queue.Num() && Parent[CellIndex(Level, Goal)] == INDEX_NONE)
	{
		const FIntPoint Current = Queue[ReadIndex++];
		for (const FIntPoint& Direction : Directions)
		{
			const FIntPoint Next = Current + Direction;
			if (!IsWalkable(Level, Next) || Parent[CellIndex(Level, Next)] != INDEX_NONE) { continue; }
			Parent[CellIndex(Level, Next)] = CellIndex(Level, Current);
			Queue.Add(Next);
			if (Next == Goal) { break; }
		}
	}
	if (Parent[CellIndex(Level, Goal)] == INDEX_NONE) { return Start; }
	int32 StepIndex = CellIndex(Level, Goal);
	while (Parent[StepIndex] != CellIndex(Level, Start) && Parent[StepIndex] != StepIndex)
	{
		StepIndex = Parent[StepIndex];
	}
	return FIntPoint(StepIndex % Level.GridWidth, StepIndex / Level.GridWidth);
}

bool FNaiwaP3M2EnemyAI::IsThreatened(const FIntPoint& Cell,
	const TArray<FNaiwaP3M2HazardForecast>& Hazards, const int32 ArrivalTick)
{
	for (const FNaiwaP3M2HazardForecast& Hazard : Hazards)
	{
		if (Hazard.Cell == Cell && Hazard.ImpactTick <= ArrivalTick && Hazard.ClearTick > ArrivalTick)
		{
			return true;
		}
	}
	return false;
}

FIntPoint FNaiwaP3M2EnemyAI::FindFleeCell(const FNaiwaP3M2SeededLevel& Level,
	const FIntPoint& CurrentCell, const TArray<FNaiwaP3M2HazardForecast>& Hazards,
	const int32 ArrivalTick)
{
	FIntPoint BestCell = CurrentCell;
	int32 BestScore = MIN_int32;
	for (const FIntPoint& Direction : Directions)
	{
		const FIntPoint Candidate = CurrentCell + Direction;
		if (!IsWalkable(Level, Candidate) || IsThreatened(Candidate, Hazards, ArrivalTick)) { continue; }
		int32 NearestHazardDistance = MAX_int32;
		for (const FNaiwaP3M2HazardForecast& Hazard : Hazards)
		{
			if (Hazard.ImpactTick <= ArrivalTick && Hazard.ClearTick > ArrivalTick)
			{
				NearestHazardDistance = FMath::Min(NearestHazardDistance,
					FMath::Abs(Candidate.X - Hazard.Cell.X) + FMath::Abs(Candidate.Y - Hazard.Cell.Y));
			}
		}
		const int32 Score = NearestHazardDistance == MAX_int32 ? 1000 : NearestHazardDistance;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestCell = Candidate;
		}
	}
	return BestCell;
}

FIntPoint FNaiwaP3M2EnemyAI::ChoosePatrolCell(const FBrain& Brain,
	const FNaiwaP3M2SeededLevel& Level, const FIntPoint& CurrentCell)
{
	TArray<FIntPoint> Candidates;
	for (const FIntPoint& Direction : Directions)
	{
		const FIntPoint Candidate = CurrentCell + Direction;
		if (!IsWalkable(Level, Candidate)) { continue; }
		if (Brain.Placement.RoamRadius > 0 &&
			(FMath::Abs(Candidate.X - Brain.Placement.Cell.X) > Brain.Placement.RoamRadius ||
			 FMath::Abs(Candidate.Y - Brain.Placement.Cell.Y) > Brain.Placement.RoamRadius)) { continue; }
		if (Brain.Placement.RoamRadius == 0 && Brain.Placement.Kind == ENaiwaP3M2EnemyKind::NaiCha &&
			(Candidate.X < Level.OuterPatrolMin.X || Candidate.X > Level.OuterPatrolMax.X ||
			 Candidate.Y < Level.OuterPatrolMin.Y || Candidate.Y > Level.OuterPatrolMax.Y)) { continue; }
		Candidates.Add(Candidate);
	}
	if (Candidates.IsEmpty()) { return CurrentCell; }
	return Candidates[ScriptedAIRandom.RandRange(0, Candidates.Num() - 1)];
}

FIntPoint FNaiwaP3M2EnemyAI::FindSoftBrickToBomb(const FNaiwaP3M2SeededLevel& Level,
	const FIntPoint& CurrentCell, const FIntPoint& PlayerCell) const
{
	FIntPoint BestCell = FIntPoint::ZeroValue;
	int32 BestDistance = MAX_int32;
	for (const FIntPoint& Direction : Directions)
	{
		const FIntPoint Candidate = CurrentCell + Direction;
		const ENaiwaP3M2Cell Tile = Level.GetCell(Candidate);
		if (Tile != ENaiwaP3M2Cell::SoftBrick && Tile != ENaiwaP3M2Cell::ItemBrick) { continue; }
		const int32 Distance = FMath::Abs(Candidate.X - PlayerCell.X) + FMath::Abs(Candidate.Y - PlayerCell.Y);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestCell = Candidate;
		}
	}
	return BestCell;
}

int32 FNaiwaP3M2EnemyAI::MoveIntervalTicks(const ENaiwaP3M2EnemyKind Kind)
{
	switch (Kind)
	{
	case ENaiwaP3M2EnemyKind::NaiCha: return 4;
	case ENaiwaP3M2EnemyKind::NaiSha: return 3;
	default: return 1000000;
	}
}

float FNaiwaP3M2EnemyAI::MoveSpeedCellsPerSecond(const ENaiwaP3M2EnemyKind Kind)
{
	switch (Kind)
	{
	case ENaiwaP3M2EnemyKind::NaiCha: return 4.0f * 0.8f;
	case ENaiwaP3M2EnemyKind::NaiSha: return 4.0f * 1.1f;
	default: return 0.0f;
	}
}

int32 FNaiwaP3M2EnemyAI::VisionRadius(const ENaiwaP3M2EnemyKind Kind)
{
	switch (Kind)
	{
	case ENaiwaP3M2EnemyKind::NaiCha: return 4;
	case ENaiwaP3M2EnemyKind::NaiSha: return 5;
	case ENaiwaP3M2EnemyKind::NaiHao: return 3;
	case ENaiwaP3M2EnemyKind::NaiYu: return 6;
	default: return 0;
	}
}
