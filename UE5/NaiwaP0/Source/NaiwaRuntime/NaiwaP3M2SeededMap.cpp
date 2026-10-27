#include "NaiwaP3M2SeededMap.h"

#include "Algo/Reverse.h"
#include "Misc/Crc.h"
#include "NaiwaP3M2Settings.h"

namespace
{
	constexpr int32 Level32Id = 32;
	constexpr int32 FirstAttempt = 0;
	constexpr uint32 MapStreamSalt = 0x4D415000;       // MAP\0
	constexpr uint32 LootStreamSalt = 0x4C4F4F54;     // LOOT
	constexpr uint32 ScriptedAIStreamSalt = 0x41495F53; // AI_S
	const FIntPoint Directions[] = {
		FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0)
	};

	int32 DeriveStreamSeed(const int32 MainSeed, const uint32 Salt)
	{
		uint32 Value = static_cast<uint32>(MainSeed) ^ Salt;
		Value ^= Value >> 16;
		Value *= 0x7FEB352D;
		Value ^= Value >> 15;
		Value *= 0x846CA68B;
		Value ^= Value >> 16;
		const uint32 PositiveSeed = Value & 0x7FFFFFFF;
		return static_cast<int32>(PositiveSeed == 0 ? 1 : PositiveSeed);
	}

	int32 CellIndex(const FIntPoint& Cell)
	{
		return Cell.Y * FNaiwaP3M2SeededLevel::Width + Cell.X;
	}

	bool IsInRect(const FIntPoint& Cell, const int32 MinX, const int32 MinY,
		const int32 MaxX, const int32 MaxY)
	{
		return Cell.X >= MinX && Cell.X <= MaxX && Cell.Y >= MinY && Cell.Y <= MaxY;
	}

	bool IsGateAt(const FNaiwaP3M2SeededLevel& Level, const FIntPoint& Cell)
	{
		for (const FNaiwaP3M2MechanicPlacement& Mechanic : Level.Mechanics)
		{
			if (Mechanic.Kind == ENaiwaP3M2MechanicKind::VaultGate && Mechanic.Cell == Cell)
			{
				return true;
			}
		}
		return false;
	}

	bool IsBlocked(const FNaiwaP3M2SeededLevel& Level, const FIntPoint& Cell,
		const bool bGatesOpen)
	{
		if (!Level.IsInside(Cell) || Level.GetCell(Cell) == ENaiwaP3M2Cell::HardBrick)
		{
			return true;
		}
		return !bGatesOpen && IsGateAt(Level, Cell);
	}

	FIntPoint PortalPartner(const FNaiwaP3M2SeededLevel& Level, const FIntPoint& Cell)
	{
		int32 PairId = INDEX_NONE;
		for (const FNaiwaP3M2MechanicPlacement& Mechanic : Level.Mechanics)
		{
			if (Mechanic.Kind == ENaiwaP3M2MechanicKind::Portal && Mechanic.Cell == Cell)
			{
				PairId = Mechanic.Variant;
				break;
			}
		}
		if (PairId == INDEX_NONE)
		{
			return FIntPoint(-1, -1);
		}
		for (const FNaiwaP3M2MechanicPlacement& Mechanic : Level.Mechanics)
		{
			if (Mechanic.Kind == ENaiwaP3M2MechanicKind::Portal &&
				Mechanic.Variant == PairId && Mechanic.Cell != Cell)
			{
				return Mechanic.Cell;
			}
		}
		return FIntPoint(-1, -1);
	}

	void AddMechanic(FNaiwaP3M2SeededLevel& Level, const ENaiwaP3M2MechanicKind Kind,
		const FIntPoint& Cell, const int32 Variant = 0)
	{
		if (Level.IsInside(Cell))
		{
			Level.Cells[CellIndex(Cell)] = ENaiwaP3M2Cell::Floor;
			FNaiwaP3M2MechanicPlacement& Placement = Level.Mechanics.AddDefaulted_GetRef();
			Placement.Kind = Kind;
			Placement.Cell = Cell;
			Placement.Variant = Variant;
		}
	}

	void AddEnemy(FNaiwaP3M2SeededLevel& Level, const ENaiwaP3M2EnemyKind Kind,
		const FIntPoint& Cell, const int32 RoamRadius = 0)
	{
		if (Level.IsInside(Cell))
		{
			Level.Cells[CellIndex(Cell)] = ENaiwaP3M2Cell::Floor;
			FNaiwaP3M2EnemyPlacement& Placement = Level.Enemies.AddDefaulted_GetRef();
			Placement.Kind = Kind;
			Placement.Cell = Cell;
			Placement.RoamRadius = RoamRadius;
		}
	}

	bool IsOccupiedByFixedPlacement(const FNaiwaP3M2SeededLevel& Level, const FIntPoint& Cell)
	{
		if (Cell == Level.SpawnCell || Cell == Level.ExitCell || Level.GoldenEggCells.Contains(Cell))
		{
			return true;
		}
		for (const FNaiwaP3M2MechanicPlacement& Mechanic : Level.Mechanics)
		{
			if (Mechanic.Cell == Cell)
			{
				return true;
			}
		}
		for (const FNaiwaP3M2EnemyPlacement& Enemy : Level.Enemies)
		{
			if (Enemy.Cell == Cell)
			{
				return true;
			}
		}
		return false;
	}
}

bool FNaiwaP3M2SeededLevel::IsInside(const FIntPoint& Cell) const
{
	return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < GridWidth && Cell.Y < GridHeight;
}

ENaiwaP3M2Cell FNaiwaP3M2SeededLevel::GetCell(const FIntPoint& Cell) const
{
	const int32 Index = Cell.Y * GridWidth + Cell.X;
	return IsInside(Cell) && Cells.IsValidIndex(Index) ? Cells[Index] : ENaiwaP3M2Cell::HardBrick;
}

int32 FNaiwaP3M2SeededMapGenerator::MakeLevelSeed(const int32 LevelId, const int32 AttemptCount)
{
	return LevelId * 10000 + AttemptCount;
}

bool FNaiwaP3M2SeededMapGenerator::GenerateLevel31(FNaiwaP3M2SeededLevel& OutLevel,
	FString& OutError)
{
	static const TCHAR* Rows[] = {
		TEXT("P....G....E"),
		TEXT("HHHHHHHHHHH"),
		TEXT(".....B....."),
		TEXT(".H.H.H.HSH."),
		TEXT(".HgH.H.HBH."),
		TEXT(".H.H.H.HSH."),
		TEXT(".....>>>>>Q"),
		TEXT(".H.HSHSHSH."),
		TEXT("@.SSS...G..")
	};
	OutLevel = FNaiwaP3M2SeededLevel();
	OutLevel.LevelId = 31;
	OutLevel.AttemptCount = 0;
	OutLevel.Seed = MakeLevelSeed(31, 0);
	OutLevel.GridWidth = 11;
	OutLevel.GridHeight = UE_ARRAY_COUNT(Rows);
	OutLevel.Cells.Init(ENaiwaP3M2Cell::HardBrick, OutLevel.GridWidth * OutLevel.GridHeight);
	OutLevel.SpawnCell = FIntPoint(0, 8);
	OutLevel.ExitCell = FIntPoint(10, 0);
	OutLevel.OuterPatrolMin = FIntPoint(5, 6);
	OutLevel.OuterPatrolMax = FIntPoint(9, 6);
	OutLevel.InnerPatrolMin = FIntPoint(5, 6);
	OutLevel.InnerPatrolMax = FIntPoint(9, 6);

	for (int32 Y = 0; Y < OutLevel.GridHeight; ++Y)
	{
		if (FCString::Strlen(Rows[Y]) != OutLevel.GridWidth)
		{
			OutError = FString::Printf(TEXT("3-1 固定地图第 %d 行宽度错误"), Y);
			return false;
		}
		for (int32 X = 0; X < OutLevel.GridWidth; ++X)
		{
			const FIntPoint Cell(X, Y);
			const TCHAR Symbol = Rows[Y][X];
			const int32 Index = Y * OutLevel.GridWidth + X;
			OutLevel.Cells[Index] = Symbol == TEXT('H') ? ENaiwaP3M2Cell::HardBrick :
				(Symbol == TEXT('S') || Symbol == TEXT('g')) ? ENaiwaP3M2Cell::SoftBrick :
				(Symbol == TEXT('B')) ? ENaiwaP3M2Cell::ItemBrick : ENaiwaP3M2Cell::Floor;
			switch (Symbol)
			{
			case TEXT('@'): OutLevel.SpawnCell = Cell; break;
			case TEXT('E'): OutLevel.ExitCell = Cell; break;
			case TEXT('G'): OutLevel.GoldenEggCells.Add(Cell); break;
			case TEXT('g'):
			{
				FNaiwaP3HiddenEggPlacement& Hidden = OutLevel.HiddenEggBricks.AddDefaulted_GetRef();
				Hidden.BrickCell = Cell;
				Hidden.EggCell = Cell;
				break;
			}
			case TEXT('P'):
			case TEXT('Q'):
			{
				FNaiwaP3M2MechanicPlacement& Portal = OutLevel.Mechanics.AddDefaulted_GetRef();
				Portal.Kind = ENaiwaP3M2MechanicKind::Portal;
				Portal.Cell = Cell;
				Portal.Variant = 1;
				break;
			}
			case TEXT('>'): break; // fixed patrol lane marker in the design grid
			default: break;
			}
		}
	}
	FNaiwaP3M2EnemyPlacement& PatrolEnemy = OutLevel.Enemies.AddDefaulted_GetRef();
	PatrolEnemy.Kind = ENaiwaP3M2EnemyKind::NaiCha;
	PatrolEnemy.Cell = FIntPoint(5, 6);
	PatrolEnemy.SpeedMultiplier = 0.5f;
	PatrolEnemy.RoamRadius = 0;
	OutLevel.LayoutHash = FString::Printf(TEXT("%08X"), CalculateLayoutHash(OutLevel));
	OutError.Reset();
	return true;
}

bool FNaiwaP3M2SeededMapGenerator::GenerateLevel32(const int32 StartAttemptCount,
	FNaiwaP3M2SeededLevel& OutLevel, FString& OutError)
{
	const UNaiwaP3M2Settings* Settings = GetDefault<UNaiwaP3M2Settings>();
	FNaiwaP3M2GenerationTuning Tuning;
	if (Settings)
	{
		Tuning.OuterSoftBrickRatio = FMath::Clamp(Settings->OuterSoftBrickRatio, 0.0f, 1.0f);
		Tuning.BeltSoftBrickRatio = FMath::Clamp(Settings->BeltSoftBrickRatio, 0.0f, 1.0f);
		Tuning.VaultSoftBrickRatio = FMath::Clamp(Settings->VaultSoftBrickRatio, 0.0f, 1.0f);
		Tuning.ItemBrickRatio = FMath::Clamp(Settings->ItemBrickRatio, 0.0f, 1.0f);
		Tuning.MaxGenerationAttempts = FMath::Clamp(Settings->MaxGenerationAttempts, 1, 64);
	}
	Tuning.MaxGenerationAttempts = FMath::Clamp(Tuning.MaxGenerationAttempts, 1, 64);

	const int32 FirstCandidate = FMath::Max(FirstAttempt, StartAttemptCount);
	FNaiwaP3M2SeededLevel LastCandidate;
	FString LastError = TEXT("没有生成候选地图");
	for (int32 Attempt = FirstCandidate; Attempt < FirstCandidate + Tuning.MaxGenerationAttempts; ++Attempt)
	{
		FNaiwaP3M2SeededLevel Candidate;
		if (!GenerateCandidate(Attempt, Tuning, Candidate))
		{
			LastError = TEXT("候选地图数据不完整");
			continue;
		}
		if (ValidateConnectivity(Candidate, LastError))
		{
			Candidate.LayoutHash = FString::Printf(TEXT("%08X"), CalculateLayoutHash(Candidate));
			OutLevel = MoveTemp(Candidate);
			OutError.Reset();
			return true;
		}
		LastCandidate = MoveTemp(Candidate);
	}

	if (LastCandidate.Cells.Num() == FNaiwaP3M2SeededLevel::Width * FNaiwaP3M2SeededLevel::Height &&
		CarveRoutesToObjectives(LastCandidate) && ValidateConnectivity(LastCandidate, LastError))
	{
		LastCandidate.bUsedConnectivityCarve = true;
		LastCandidate.LayoutHash = FString::Printf(TEXT("%08X"), CalculateLayoutHash(LastCandidate));
		OutLevel = MoveTemp(LastCandidate);
		OutError.Reset();
		return true;
	}

	OutError = FString::Printf(TEXT("3-2 地图生成失败（从候选 %d 开始，最多尝试 %d 次）：%s"),
		FirstCandidate, Tuning.MaxGenerationAttempts, *LastError);
	return false;
}

bool FNaiwaP3M2SeededMapGenerator::GenerateCandidate(const int32 AttemptCount,
	const FNaiwaP3M2GenerationTuning& Tuning, FNaiwaP3M2SeededLevel& OutLevel)
{
	OutLevel = FNaiwaP3M2SeededLevel();
	OutLevel.LevelId = Level32Id;
	OutLevel.AttemptCount = AttemptCount;
	OutLevel.Seed = MakeLevelSeed(Level32Id, AttemptCount);
	OutLevel.MapStreamSeed = DeriveStreamSeed(OutLevel.Seed, MapStreamSalt);
	OutLevel.LootStreamSeed = DeriveStreamSeed(OutLevel.Seed, LootStreamSalt);
	OutLevel.ScriptedAIStreamSeed = DeriveStreamSeed(OutLevel.Seed, ScriptedAIStreamSalt);
	OutLevel.Cells.SetNumZeroed(FNaiwaP3M2SeededLevel::Width * FNaiwaP3M2SeededLevel::Height);

	FRandomStream MapRandom(OutLevel.MapStreamSeed);
	FRandomStream AIRandom(OutLevel.ScriptedAIStreamSeed);
	for (int32 Y = 0; Y < FNaiwaP3M2SeededLevel::Height; ++Y)
	{
		for (int32 X = 0; X < FNaiwaP3M2SeededLevel::Width; ++X)
		{
			const FIntPoint Cell(X, Y);
			const bool bBorder = X == 0 || X == FNaiwaP3M2SeededLevel::Width - 1 ||
				Y == 0 || Y == FNaiwaP3M2SeededLevel::Height - 1;
			const bool bHardCheckerboard = X > 1 && X < 22 && Y > 1 && Y < 16 &&
				(X % 2) == 1 && (Y % 2) == 1;
			OutLevel.Cells[CellIndex(Cell)] = (bBorder || bHardCheckerboard) ?
				ENaiwaP3M2Cell::HardBrick : ENaiwaP3M2Cell::Floor;
		}
	}

	// 金库硬壳优先于软砖随机铺设；两处门洞保留为可控机关格。
	for (int32 X = 8; X <= 15; ++X)
	{
		for (int32 Y = 5; Y <= 11; ++Y)
		{
			if (X == 8 || X == 15 || Y == 5 || Y == 11)
			{
				OutLevel.Cells[CellIndex(FIntPoint(X, Y))] = ENaiwaP3M2Cell::HardBrick;
			}
		}
	}
	OutLevel.Cells[CellIndex(FIntPoint(11, 5))] = ENaiwaP3M2Cell::Floor;
	OutLevel.Cells[CellIndex(FIntPoint(11, 11))] = ENaiwaP3M2Cell::Floor;

	for (int32 Y = 1; Y < FNaiwaP3M2SeededLevel::Height - 1; ++Y)
	{
		for (int32 X = 1; X < FNaiwaP3M2SeededLevel::Width - 1; ++X)
		{
			const FIntPoint Cell(X, Y);
			if (OutLevel.GetCell(Cell) != ENaiwaP3M2Cell::Floor)
			{
				continue;
			}
			const float SoftRatio = IsInRect(Cell, 8, 5, 15, 11) ? Tuning.VaultSoftBrickRatio :
				(IsInRect(Cell, 5, 3, 17, 13) ? Tuning.BeltSoftBrickRatio : Tuning.OuterSoftBrickRatio);
			if (MapRandom.FRand() < SoftRatio)
			{
				OutLevel.Cells[CellIndex(Cell)] = MapRandom.FRand() < Tuning.ItemBrickRatio ?
					ENaiwaP3M2Cell::ItemBrick : ENaiwaP3M2Cell::SoftBrick;
			}
		}
	}

	// 固定目标与机关格必须可站立；出生点周围强制清出 3×3 安全区。
	for (int32 Y = 13; Y <= 15; ++Y)
	{
		for (int32 X = 1; X <= 3; ++X)
		{
			OutLevel.Cells[CellIndex(FIntPoint(X, Y))] = ENaiwaP3M2Cell::Floor;
		}
	}
	OutLevel.Cells[CellIndex(OutLevel.ExitCell)] = ENaiwaP3M2Cell::Floor;

	OutLevel.GoldenEggCells = {
		FIntPoint(20, 14), FIntPoint(5, 4), FIntPoint(17, 12), FIntPoint(10, 7), FIntPoint(12, 9)
	};
	for (const FIntPoint& Egg : OutLevel.GoldenEggCells)
	{
		OutLevel.Cells[CellIndex(Egg)] = ENaiwaP3M2Cell::Floor;
	}

	AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::PressurePlate, FIntPoint(2, 8), 0);
	AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::PressurePlate, FIntPoint(20, 8), 1);
	AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::VaultGate, FIntPoint(11, 11), 0); // 西踩板控制
	AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::VaultGate, FIntPoint(11, 5), 1);  // 东踩板控制
	for (int32 X = 3; X <= 5; ++X)
	{
		AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::Spike, FIntPoint(X, 8));
	}
	for (int32 X = 17; X <= 19; ++X)
	{
		AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::Spike, FIntPoint(X, 8));
	}
	for (int32 X = 8; X <= 14; ++X)
	{
		AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::Conveyor, FIntPoint(X, 6), -1);
		AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::Conveyor, FIntPoint(X, 10), 1);
	}
	AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::CrackedFloor, FIntPoint(4, 12));
	AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::CrackedFloor, FIntPoint(5, 12));
	AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::CrackedFloor, FIntPoint(18, 4));
	AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::CrackedFloor, FIntPoint(19, 4));
	AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::Portal, FIntPoint(20, 2), 0); // C 对
	AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::Portal, FIntPoint(2, 12), 0);
	AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::Portal, FIntPoint(2, 2), 1);  // D 对
	AddMechanic(OutLevel, ENaiwaP3M2MechanicKind::Portal, FIntPoint(20, 12), 1);

	AddEnemy(OutLevel, ENaiwaP3M2EnemyKind::NaiSha, FIntPoint(7, 10), 3);
	AddEnemy(OutLevel, ENaiwaP3M2EnemyKind::NaiHao, FIntPoint(11, 8), 3);
	AddEnemy(OutLevel, ENaiwaP3M2EnemyKind::NaiYu, FIntPoint(11, 3));

	TArray<FIntPoint> ChaCandidates;
	for (int32 Y = 2; Y <= 14; ++Y)
	{
		for (int32 X = 2; X <= 20; ++X)
		{
			const FIntPoint Cell(X, Y);
			if (OutLevel.GetCell(Cell) == ENaiwaP3M2Cell::Floor &&
				!IsOccupiedByFixedPlacement(OutLevel, Cell) &&
				!(FMath::Abs(X - OutLevel.SpawnCell.X) <= 2 && FMath::Abs(Y - OutLevel.SpawnCell.Y) <= 2))
			{
				ChaCandidates.Add(Cell);
			}
		}
	}
	for (int32 ChaIndex = 0; ChaIndex < 2 && !ChaCandidates.IsEmpty(); ++ChaIndex)
	{
		const int32 ChosenIndex = AIRandom.RandRange(0, ChaCandidates.Num() - 1);
		const FIntPoint Spawn = ChaCandidates[ChosenIndex];
		ChaCandidates.RemoveAtSwap(ChosenIndex, 1, EAllowShrinking::No);
		AddEnemy(OutLevel, ENaiwaP3M2EnemyKind::NaiCha, Spawn);
	}

	// 巡逻矩形为导航范围，不把范围线误当成阻挡地形。
	return OutLevel.Cells.Num() == FNaiwaP3M2SeededLevel::Width * FNaiwaP3M2SeededLevel::Height &&
		OutLevel.GoldenEggCells.Num() == 5 && OutLevel.Enemies.Num() == 5;
}

bool FNaiwaP3M2SeededMapGenerator::ValidateConnectivity(const FNaiwaP3M2SeededLevel& Level,
	FString& OutError)
{
	if (Level.GoldenEggCells.Num() != 5)
	{
		OutError = TEXT("金蛋数量不是 5 枚");
		return false;
	}
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FIntPoint Plate = Index == 0 ? FIntPoint(2, 8) : FIntPoint(20, 8);
		if (!IsReachable(Level, Level.SpawnCell, Plate, false))
		{
			OutError = FString::Printf(TEXT("出生点无法到达第 %d 块踩板"), Index + 1);
			return false;
		}
	}

	// 踩板已可达后，按计划把对应金库门纳入可达图，检查金库目标及回程出口。
	for (const FIntPoint& Egg : Level.GoldenEggCells)
	{
		if (!IsReachable(Level, Level.SpawnCell, Egg, true))
		{
			OutError = FString::Printf(TEXT("出生点无法经可达踩板到达金蛋 (%d,%d)"), Egg.X, Egg.Y);
			return false;
		}
	}
	if (!IsReachable(Level, Level.SpawnCell, Level.ExitCell, true))
	{
		OutError = TEXT("集齐金蛋后无法回到出口");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FNaiwaP3M2SeededMapGenerator::CarveRoutesToObjectives(FNaiwaP3M2SeededLevel& Level)
{
	TArray<FIntPoint> Targets = Level.GoldenEggCells;
	Targets.Add(FIntPoint(2, 8));
	Targets.Add(FIntPoint(20, 8));
	Targets.Add(Level.ExitCell);
	for (const FIntPoint& Target : Targets)
	{
		TArray<FIntPoint> Path;
		if (!FindPath(Level, Level.SpawnCell, Target, true, Path))
		{
			return false;
		}
		for (const FIntPoint& Cell : Path)
		{
			const int32 Index = CellIndex(Cell);
			if (Level.Cells.IsValidIndex(Index) &&
				(Level.Cells[Index] == ENaiwaP3M2Cell::SoftBrick || Level.Cells[Index] == ENaiwaP3M2Cell::ItemBrick))
			{
				Level.Cells[Index] = ENaiwaP3M2Cell::Floor;
			}
		}
	}
	return true;
}

bool FNaiwaP3M2SeededMapGenerator::FindPath(const FNaiwaP3M2SeededLevel& Level,
	const FIntPoint& Start, const FIntPoint& Goal, const bool bGatesOpen, TArray<FIntPoint>& OutPath)
{
	OutPath.Reset();
	if (IsBlocked(Level, Start, bGatesOpen) || IsBlocked(Level, Goal, bGatesOpen))
	{
		return false;
	}
	const int32 CellCount = FNaiwaP3M2SeededLevel::Width * FNaiwaP3M2SeededLevel::Height;
	TArray<uint8> Visited;
	Visited.SetNumZeroed(CellCount);
	TArray<int32> Previous;
	Previous.Init(INDEX_NONE, CellCount);
	TArray<FIntPoint> Queue;
	Queue.Reserve(CellCount);
	Queue.Add(Start);
	Visited[CellIndex(Start)] = 1;

	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const FIntPoint Current = Queue[Head];
		if (Current == Goal)
		{
			int32 PathIndex = CellIndex(Goal);
			while (PathIndex != INDEX_NONE)
			{
				OutPath.Add(FIntPoint(PathIndex % FNaiwaP3M2SeededLevel::Width,
					PathIndex / FNaiwaP3M2SeededLevel::Width));
				if (PathIndex == CellIndex(Start))
				{
					break;
				}
				PathIndex = Previous[PathIndex];
			}
			Algo::Reverse(OutPath);
			return true;
		}

		const auto TryVisit = [&Level, &Visited, &Previous, &Queue, bGatesOpen, Current](const FIntPoint& Next)
		{
			if (IsBlocked(Level, Next, bGatesOpen))
			{
				return;
			}
			const int32 NextIndex = CellIndex(Next);
			if (Visited[NextIndex] != 0)
			{
				return;
			}
			Visited[NextIndex] = 1;
			Previous[NextIndex] = CellIndex(Current);
			Queue.Add(Next);
		};

		for (const FIntPoint& Direction : Directions)
		{
			TryVisit(Current + Direction);
		}
		const FIntPoint Partner = PortalPartner(Level, Current);
		if (Partner.X >= 0)
		{
			TryVisit(Partner);
		}
	}
	return false;
}

bool FNaiwaP3M2SeededMapGenerator::IsReachable(const FNaiwaP3M2SeededLevel& Level,
	const FIntPoint& Start, const FIntPoint& Goal, const bool bGatesOpen)
{
	TArray<FIntPoint> Path;
	return FindPath(Level, Start, Goal, bGatesOpen, Path);
}

uint32 FNaiwaP3M2SeededMapGenerator::CalculateLayoutHash(const FNaiwaP3M2SeededLevel& Level)
{
	FString Serialized = FString::Printf(TEXT("L%d|A%d|S%d|W%d|H%d|M%d|L%d|AI%d|"), Level.LevelId,
		Level.AttemptCount, Level.Seed, Level.GridWidth, Level.GridHeight,
		Level.MapStreamSeed, Level.LootStreamSeed, Level.ScriptedAIStreamSeed);
	for (int32 Y = 0; Y < Level.GridHeight; ++Y)
	{
		for (int32 X = 0; X < Level.GridWidth; ++X)
		{
			Serialized.AppendChar(static_cast<TCHAR>('0' + static_cast<uint8>(Level.GetCell(FIntPoint(X, Y)))));
		}
		Serialized.AppendChar(TEXT('\n'));
	}
	for (const FNaiwaP3M2MechanicPlacement& Mechanic : Level.Mechanics)
	{
		Serialized += FString::Printf(TEXT("M%d:%d,%d:%d|"), static_cast<uint8>(Mechanic.Kind),
			Mechanic.Cell.X, Mechanic.Cell.Y, Mechanic.Variant);
	}
	for (const FNaiwaP3M2EnemyPlacement& Enemy : Level.Enemies)
	{
		Serialized += FString::Printf(TEXT("E%d:%d,%d:%d|"), static_cast<uint8>(Enemy.Kind),
			Enemy.Cell.X, Enemy.Cell.Y, Enemy.RoamRadius);
	}
	return FCrc::StrCrc32(*Serialized);
}
