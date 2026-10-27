#pragma once

#include "CoreMinimal.h"

enum class ENaiwaP3M2Cell : uint8
{
	Floor,
	HardBrick,
	SoftBrick,
	ItemBrick
};

enum class ENaiwaP3M2MechanicKind : uint8
{
	PressurePlate,
	VaultGate,
	Spike,
	Conveyor,
	CrackedFloor,
	Portal
};

enum class ENaiwaP3M2EnemyKind : uint8
{
	NaiCha,
	NaiSha,
	NaiHao,
	NaiYu
};

struct FNaiwaP3M2MechanicPlacement
{
	ENaiwaP3M2MechanicKind Kind = ENaiwaP3M2MechanicKind::PressurePlate;
	FIntPoint Cell = FIntPoint::ZeroValue;
	int32 Variant = 0;
};

struct FNaiwaP3M2EnemyPlacement
{
	ENaiwaP3M2EnemyKind Kind = ENaiwaP3M2EnemyKind::NaiCha;
	FIntPoint Cell = FIntPoint::ZeroValue;
	int32 RoamRadius = 0;
	float SpeedMultiplier = 1.0f;
};

struct FNaiwaP3HiddenEggPlacement
{
	FIntPoint BrickCell = FIntPoint::ZeroValue;
	FIntPoint EggCell = FIntPoint::ZeroValue;
};

struct FNaiwaP3M2SeededLevel
{
	static constexpr int32 Width = 23;
	static constexpr int32 Height = 17;
	int32 GridWidth = Width;
	int32 GridHeight = Height;

	int32 LevelId = 32;
	int32 AttemptCount = 0;
	int32 Seed = 320000;
	int32 MapStreamSeed = 0;
	int32 LootStreamSeed = 0;
	int32 ScriptedAIStreamSeed = 0;
	TArray<ENaiwaP3M2Cell> Cells;
	FIntPoint SpawnCell = FIntPoint(2, 14);
	FIntPoint ExitCell = FIntPoint(4, 14);
	TArray<FIntPoint> GoldenEggCells;
	TArray<FNaiwaP3HiddenEggPlacement> HiddenEggBricks;
	TArray<FNaiwaP3M2MechanicPlacement> Mechanics;
	TArray<FNaiwaP3M2EnemyPlacement> Enemies;
	FIntPoint OuterPatrolMin = FIntPoint(1, 1);
	FIntPoint OuterPatrolMax = FIntPoint(21, 15);
	FIntPoint InnerPatrolMin = FIntPoint(6, 3);
	FIntPoint InnerPatrolMax = FIntPoint(16, 13);
	bool bUsedConnectivityCarve = false;
	FString LayoutHash;

	bool IsInside(const FIntPoint& Cell) const;
	ENaiwaP3M2Cell GetCell(const FIntPoint& Cell) const;
};

struct FNaiwaP3M2GenerationTuning
{
	float OuterSoftBrickRatio = 0.45f;
	float BeltSoftBrickRatio = 0.40f;
	float VaultSoftBrickRatio = 0.30f;
	float ItemBrickRatio = 0.20f;
	int32 MaxGenerationAttempts = 64;
};

class NAIWARUNTIME_API FNaiwaP3M2SeededMapGenerator final
{
public:
	static bool GenerateLevel31(FNaiwaP3M2SeededLevel& OutLevel, FString& OutError);
	// seed = levelId * 10000 + attemptCount。重开时传入已接受的 attemptCount，即可复现原图。
	static bool GenerateLevel32(int32 StartAttemptCount,
		FNaiwaP3M2SeededLevel& OutLevel, FString& OutError);
	static int32 MakeLevelSeed(int32 LevelId, int32 AttemptCount);

private:
	static bool GenerateCandidate(int32 AttemptCount, const FNaiwaP3M2GenerationTuning& Tuning,
		FNaiwaP3M2SeededLevel& OutLevel);
	static bool ValidateConnectivity(const FNaiwaP3M2SeededLevel& Level, FString& OutError);
	static bool CarveRoutesToObjectives(FNaiwaP3M2SeededLevel& Level);
	static bool FindPath(const FNaiwaP3M2SeededLevel& Level, const FIntPoint& Start,
		const FIntPoint& Goal, bool bGatesOpen, TArray<FIntPoint>& OutPath);
	static bool IsReachable(const FNaiwaP3M2SeededLevel& Level, const FIntPoint& Start,
		const FIntPoint& Goal, bool bGatesOpen);
	static uint32 CalculateLayoutHash(const FNaiwaP3M2SeededLevel& Level);
};
