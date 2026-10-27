#pragma once

#include "CoreMinimal.h"
#include "NaiwaP3FogOfWar.h"
#include "NaiwaP3ItemEffects.h"
#include "NaiwaP3LootTable.h"
#include "NaiwaP3Mechanics.h"
#include "NaiwaP3M2SeededMap.h"
#include "NaiwaP3M2EnemyAI.h"

// M2 地图逻辑会话：将种子地图、机关、掉落、玩家状态和迷雾放在同一确定性 tick 上。
class NAIWARUNTIME_API FNaiwaP3M2LevelSession final
{
public:
	bool Initialize(int32 LevelId, int32 StartAttemptCount, bool bFogEnabled, FString& OutError);
	void AdvanceTick(const FIntPoint& PlayerCell, const TArray<FIntPoint>& EnemyCells,
		bool bRageEscape = false);

	bool BreakBrickAt(const FIntPoint& Cell, bool bGuaranteeTeachingDrop = false);
	bool TryCollectItemAt(const FIntPoint& Cell, FString& OutMessage);
	bool TryCollectItemForEnemyAt(const FIntPoint& Cell, ENaiwaP3LootType& OutLoot);
	bool TryCollectEggAt(const FIntPoint& Cell);
	bool TryGetItemAt(const FIntPoint& Cell, ENaiwaP3LootType& OutLoot) const;
	void GetEnemyFetchItemCells(TArray<FIntPoint>& OutCells) const;
	void RegisterFireAtCell(const FIntPoint& Cell);
	FNaiwaP3M2EnemyIntent ThinkEnemy(int32 EnemyIndex, const FIntPoint& EnemyCell,
		const FIntPoint& PlayerCell, const TArray<FNaiwaP3M2HazardForecast>& Hazards,
		const TArray<FIntPoint>& FetchItemCells, float SpeedMultiplier,
		bool bRageEscape = false);
	void NotifyEnemyCellReached(int32 EnemyIndex, const FIntPoint& Cell);

	int32 GetCurrentTick() const { return CurrentTick; }
	int32 GetCollectedEggCount() const { return CollectedEggCount; }
	int32 GetRemainingEggCount() const { return Level.GoldenEggCells.Num() + Level.HiddenEggBricks.Num(); }
	const FNaiwaP3M2SeededLevel& GetLevel() const { return Level; }
	FNaiwaP3FogOfWar& GetFog() { return Fog; }
	const FNaiwaP3FogOfWar& GetFog() const { return Fog; }
	FNaiwaP3MechanicsRuntime& GetMechanics() { return Mechanics; }
	const FNaiwaP3MechanicsRuntime& GetMechanics() const { return Mechanics; }
	FNaiwaP3M2EnemyAI& GetEnemyAI() { return EnemyAI; }
	const FNaiwaP3M2EnemyAI& GetEnemyAI() const { return EnemyAI; }
	FNaiwaP3ItemEffects& GetPlayerItems() { return PlayerItems; }
	const FNaiwaP3ItemEffects& GetPlayerItems() const { return PlayerItems; }
	uint32 GetLootDrawCount() const;

private:
	int32 CellIndex(const FIntPoint& Cell) const;

	FNaiwaP3M2SeededLevel Level;
	FNaiwaP3MechanicsRuntime Mechanics;
	FNaiwaP3M2EnemyAI EnemyAI;
	FNaiwaP3FogOfWar Fog;
	FNaiwaP3ItemEffects PlayerItems;
	TUniquePtr<FNaiwaP3LootRandomStream> LootStream;
	TMap<FIntPoint, ENaiwaP3LootType> ItemDrops;
	int32 CurrentTick = 0;
	int32 CollectedEggCount = 0;
};
