#include "NaiwaP3M2LevelSession.h"

namespace
{
	bool IsEnemyUsefulLoot(const ENaiwaP3LootType Loot)
	{
		return Loot == ENaiwaP3LootType::BombCapacity || Loot == ENaiwaP3LootType::BombRange ||
			Loot == ENaiwaP3LootType::SpeedBoost || Loot == ENaiwaP3LootType::Shield;
	}
}

bool FNaiwaP3M2LevelSession::Initialize(const int32 LevelId, const int32 StartAttemptCount,
	const bool bFogEnabled, FString& OutError)
{
	FNaiwaP3M2SeededLevel GeneratedLevel;
	const bool bGenerated = LevelId == 31 ?
		FNaiwaP3M2SeededMapGenerator::GenerateLevel31(GeneratedLevel, OutError) :
		FNaiwaP3M2SeededMapGenerator::GenerateLevel32(StartAttemptCount, GeneratedLevel, OutError);
	if (!bGenerated)
	{
		return false;
	}

	Level = MoveTemp(GeneratedLevel);
	Mechanics.Initialize(Level.Mechanics);
	EnemyAI.Initialize(Level.Enemies, Level.ScriptedAIStreamSeed);
	Fog.Initialize(Level, bFogEnabled);
	Fog.UpdateVisibility(Level, Level.SpawnCell);
	PlayerItems.Reset();
	LootStream = MakeUnique<FNaiwaP3LootRandomStream>(Level.LootStreamSeed);
	ItemDrops.Reset();
	CurrentTick = 0;
	CollectedEggCount = 0;
	OutError.Reset();
	return true;
}

void FNaiwaP3M2LevelSession::AdvanceTick(const FIntPoint& PlayerCell,
	const TArray<FIntPoint>& EnemyCells, const bool bRageEscape)
{
	++CurrentTick;
	TArray<FIntPoint> OccupiedCells = EnemyCells;
	OccupiedCells.AddUnique(PlayerCell);
	Mechanics.AdvanceTick(CurrentTick, OccupiedCells);
	Fog.UpdateVisibility(Level, PlayerCell, bRageEscape);
}

bool FNaiwaP3M2LevelSession::BreakBrickAt(const FIntPoint& Cell,
	const bool bGuaranteeTeachingDrop)
{
	if (!Level.IsInside(Cell))
	{
		return false;
	}

	const int32 Index = CellIndex(Cell);
	if (!Level.Cells.IsValidIndex(Index))
	{
		return false;
	}
	const ENaiwaP3M2Cell Tile = Level.Cells[Index];
	if (Tile != ENaiwaP3M2Cell::SoftBrick && Tile != ENaiwaP3M2Cell::ItemBrick)
	{
		return false;
	}

	Level.Cells[Index] = ENaiwaP3M2Cell::Floor;
	for (int32 HiddenIndex = Level.HiddenEggBricks.Num() - 1; HiddenIndex >= 0; --HiddenIndex)
	{
		if (Level.HiddenEggBricks[HiddenIndex].BrickCell == Cell)
		{
			Level.GoldenEggCells.AddUnique(Level.HiddenEggBricks[HiddenIndex].EggCell);
			Level.HiddenEggBricks.RemoveAt(HiddenIndex);
		}
	}
	if (Tile == ENaiwaP3M2Cell::ItemBrick && LootStream)
	{
		const ENaiwaP3LootType Loot = bGuaranteeTeachingDrop ?
			LootStream->RollGuaranteedTeachingDrop() : LootStream->RollDrop();
		ItemDrops.Add(Cell, Loot);
	}
	return true;
}

bool FNaiwaP3M2LevelSession::TryCollectItemAt(const FIntPoint& Cell, FString& OutMessage)
{
	const ENaiwaP3LootType* Loot = ItemDrops.Find(Cell);
	if (!Loot)
	{
		OutMessage.Reset();
		return false;
	}

	OutMessage = PlayerItems.Collect(*Loot, CurrentTick);
	ItemDrops.Remove(Cell);
	return true;
}

bool FNaiwaP3M2LevelSession::TryCollectEggAt(const FIntPoint& Cell)
{
	const int32 RemovedCount = Level.GoldenEggCells.Remove(Cell);
	if (RemovedCount <= 0)
	{
		return false;
	}
	CollectedEggCount += RemovedCount;
	return true;
}

bool FNaiwaP3M2LevelSession::TryGetItemAt(const FIntPoint& Cell, ENaiwaP3LootType& OutLoot) const
{
	const ENaiwaP3LootType* Loot = ItemDrops.Find(Cell);
	if (!Loot)
	{
		return false;
	}
	OutLoot = *Loot;
	return true;
}

bool FNaiwaP3M2LevelSession::TryCollectItemForEnemyAt(const FIntPoint& Cell,
	ENaiwaP3LootType& OutLoot)
{
	const ENaiwaP3LootType* Loot = ItemDrops.Find(Cell);
	if (!Loot || !IsEnemyUsefulLoot(*Loot)) return false;
	OutLoot = *Loot;
	ItemDrops.Remove(Cell);
	return true;
}

void FNaiwaP3M2LevelSession::GetEnemyFetchItemCells(TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();
	for (const TPair<FIntPoint, ENaiwaP3LootType>& Item : ItemDrops)
	{
		if (IsEnemyUsefulLoot(Item.Value)) OutCells.Add(Item.Key);
	}
	OutCells.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return A.Y != B.Y ? A.Y < B.Y : A.X < B.X;
	});
}

void FNaiwaP3M2LevelSession::RegisterFireAtCell(const FIntPoint& Cell)
{
	Mechanics.RegisterFireAtCell(Cell, CurrentTick);
}

FNaiwaP3M2EnemyIntent FNaiwaP3M2LevelSession::ThinkEnemy(const int32 EnemyIndex,
	const FIntPoint& EnemyCell, const FIntPoint& PlayerCell,
	const TArray<FNaiwaP3M2HazardForecast>& Hazards,
	const TArray<FIntPoint>& FetchItemCells, const float SpeedMultiplier,
	const bool bRageEscape)
{
	return EnemyAI.Think(EnemyIndex, Level, EnemyCell, PlayerCell, Hazards, FetchItemCells,
		CurrentTick, SpeedMultiplier, bRageEscape);
}

void FNaiwaP3M2LevelSession::NotifyEnemyCellReached(const int32 EnemyIndex, const FIntPoint& Cell)
{
	EnemyAI.NotifyCellReached(EnemyIndex, Cell);
}

uint32 FNaiwaP3M2LevelSession::GetLootDrawCount() const
{
	return LootStream ? LootStream->GetDrawCount() : 0;
}

int32 FNaiwaP3M2LevelSession::CellIndex(const FIntPoint& Cell) const
{
	return Cell.Y * Level.GridWidth + Cell.X;
}
