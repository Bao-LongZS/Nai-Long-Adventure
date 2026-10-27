#pragma once

#include "CoreMinimal.h"
#include "NaiwaP3BombTypes.h"
#include "NaiwaP3M2SeededMap.h"

enum class ENaiwaP3M2EnemyAIState : uint8
{
	Patrol,
	Chase,
	Search,
	Return,
	Flee,
	Fetch,
	Guard
};

struct FNaiwaP3M2HazardForecast
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	int32 ImpactTick = 0;
	int32 ClearTick = 0;
	int32 OwnerEnemyIndex = INDEX_NONE;
};

struct FNaiwaP3M2BombForecastInput
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	int32 FuseTick = 0;
	int32 FuseDurationTicks = 0;
	uint64 PlacementOrder = 0;
	ENaiwaP3BombType Type = ENaiwaP3BombType::Standard;
	int32 Range = 1;
	bool bPlayerOwned = true;
	int32 OwnerEnemyIndex = INDEX_NONE;
};

struct FNaiwaP3M2EnemyIntent
{
	FIntPoint NextCell = FIntPoint::ZeroValue;
	ENaiwaP3M2EnemyAIState State = ENaiwaP3M2EnemyAIState::Patrol;
	bool bHasNextCell = false;
	bool bDropBomb = false;
	bool bRangedAttack = false;
};

// 确定性敌人决策器。实体系统读取移动/攻击意图并负责表现与执行。
class NAIWARUNTIME_API FNaiwaP3M2EnemyAI final
{
public:
	void Initialize(const TArray<FNaiwaP3M2EnemyPlacement>& Enemies, int32 ScriptedAISeed);
	static void BuildHazardForecast(const FNaiwaP3M2SeededLevel& Level,
		const TArray<FNaiwaP3M2BombForecastInput>& Bombs, int32 ChainDelayTicks,
		int32 FlameDurationTicks, TArray<FNaiwaP3M2HazardForecast>& OutHazards);
	FNaiwaP3M2EnemyIntent Think(int32 EnemyIndex, const FNaiwaP3M2SeededLevel& Level,
		const FIntPoint& EnemyCell, const FIntPoint& PlayerCell,
		const TArray<FNaiwaP3M2HazardForecast>& Hazards,
		const TArray<FIntPoint>& FetchItemCells, int32 CurrentTick,
		float SpeedMultiplier, bool bRageEscape = false);
	void NotifyCellReached(int32 EnemyIndex, const FIntPoint& Cell);
	int32 Num() const { return Brains.Num(); }

private:
	struct FBrain
	{
		FNaiwaP3M2EnemyPlacement Placement;
		ENaiwaP3M2EnemyAIState State = ENaiwaP3M2EnemyAIState::Patrol;
		FNaiwaP3M2EnemyIntent CachedIntent;
		FIntPoint LastSeenPlayerCell = FIntPoint::ZeroValue;
		int32 NextThinkTick = 0;
		int32 LastMoveCreditTick = 0;
		int32 NextAttackTick = 0;
		int32 SearchUntilTick = 0;
		float MoveCredit = 0.0f;
		bool bHasLastSeenPlayer = false;
	};

	static bool CanSeePlayer(const FNaiwaP3M2SeededLevel& Level,
		const FIntPoint& EnemyCell, const FIntPoint& PlayerCell, int32 VisionRadius);
	static bool HasLineOfSight(const FNaiwaP3M2SeededLevel& Level,
		const FIntPoint& Start, const FIntPoint& Target);
	static FIntPoint FindNextPathCell(const FNaiwaP3M2SeededLevel& Level,
		const FIntPoint& Start, const FIntPoint& Goal);
	static bool IsThreatened(const FIntPoint& Cell,
		const TArray<FNaiwaP3M2HazardForecast>& Hazards, int32 ArrivalTick);
	static FIntPoint FindFleeCell(const FNaiwaP3M2SeededLevel& Level,
		const FIntPoint& CurrentCell, const TArray<FNaiwaP3M2HazardForecast>& Hazards,
		int32 ArrivalTick);
	FIntPoint ChoosePatrolCell(const FBrain& Brain, const FNaiwaP3M2SeededLevel& Level,
		const FIntPoint& CurrentCell);
	FIntPoint FindSoftBrickToBomb(const FNaiwaP3M2SeededLevel& Level,
		const FIntPoint& CurrentCell, const FIntPoint& PlayerCell) const;
	static int32 MoveIntervalTicks(ENaiwaP3M2EnemyKind Kind);
	static float MoveSpeedCellsPerSecond(ENaiwaP3M2EnemyKind Kind);
	static int32 VisionRadius(ENaiwaP3M2EnemyKind Kind);

	TArray<FBrain> Brains;
	FRandomStream ScriptedAIRandom;
};
