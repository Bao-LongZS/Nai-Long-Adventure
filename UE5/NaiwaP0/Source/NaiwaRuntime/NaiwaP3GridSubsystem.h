#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NaiwaP3M1Actors.h"
#include "NaiwaP3BombTypes.h"
#include "NaiwaP3GridSubsystem.generated.h"

class ANaiwaP3BombActor;
class ANaiwaP3M1EnemyActor;
class ANaiwaP3M1MapActor;
class ANaiwaP3PlayerPawn;

struct FNaiwaP3ActiveBomb
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	int32 FuseTick = 0;
	uint64 PlacementOrder = 0;
	ENaiwaP3BombType Type = ENaiwaP3BombType::Standard;
	int32 Range = 1;
	TWeakObjectPtr<ANaiwaP3BombActor> Visual;
};

UCLASS()
class NAIWARUNTIME_API UNaiwaP3GridSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bM1Initialized; }

	void InitializeM1();
	void ResetM1();
	void RegisterPlayerPawn(ANaiwaP3PlayerPawn* Pawn);
	void SetPlayerDirection(const FIntPoint& Direction);
	void SelectPlayerBombType(ENaiwaP3BombType Type);
	bool PlacePlayerBomb();
	bool StageChainDrill();

	FIntPoint GetPlayerCell() const { return PlayerCell; }
	FIntPoint GetEnemyCell() const { return EnemyCell; }
	int32 GetCurrentTick() const { return CurrentTick; }
	int32 GetActiveBombCount() const { return ActiveBombs.Num(); }
	float GetPlayerCellsPerSecond() const { return 1.0f / PlayerCellSeconds; }
	float GetPatrolCellsPerSecond() const { return 1.0f / EnemyCellSeconds; }
	int32 GetBombCapacity() const { return MaxActiveBombs; }
	int32 GetBombRange() const { return BombRange; }
	float GetBombPlacementIntervalSeconds() const { return static_cast<float>(BombPlacementIntervalTicks) * FixedStepSeconds; }
	ENaiwaP3BombType GetSelectedBombType() const { return SelectedBombType; }
	const TCHAR* GetSelectedBombDisplayName() const { return GetNaiwaP3BombDisplayName(SelectedBombType); }
	float GetBombFuseSeconds() const { return static_cast<float>(BombFuseTicks) * FixedStepSeconds; }
	float GetChainDelaySeconds() const { return static_cast<float>(ChainDelayTicks) * FixedStepSeconds; }
	float GetFlameDurationSeconds() const { return static_cast<float>(FlameLifeTicks) * FixedStepSeconds; }
	int32 GetPlayerHitCount() const { return PlayerHitCount; }
	int32 GetEnemyHitCount() const { return EnemyHitCount; }
	FString GetMapHash() const;

	bool RunM1SelfTest(FString& OutDetails);

private:
	static constexpr float FixedStepSeconds = 0.1f;
	static constexpr float CellSize = 100.0f;

	void BuildM1Rows();
	void RebuildWorldActors();
	void LoadM1Tuning();
	void AdvanceFixedStep();
	void AdvancePlayer(float StepSeconds);
	void AdvanceEnemy(float StepSeconds);
	void ResolveDueBombs();
	void ResolveBombAtIndex(int32 BombIndex);
	void ExpireFlames();
	bool SpawnBombAt(const FIntPoint& Cell, int32 FuseTick, bool bIgnorePlayerLimit = false,
		ENaiwaP3BombType Type = ENaiwaP3BombType::Standard, int32 RangeOverride = INDEX_NONE);
	bool CanEnterPlayerCell(const FIntPoint& Cell) const;
	bool IsInside(const FIntPoint& Cell) const;
	int32 CellIndex(const FIntPoint& Cell) const;
	ENaiwaP3Tile GetTile(const FIntPoint& Cell) const;
	void SetTile(const FIntPoint& Cell, ENaiwaP3Tile Tile);
	FVector GridToWorld(const FIntPoint& Cell, float Z = 0.0f) const;
	void BuildPatrolPath();
	void UpdatePresentation(float InterpolationAlpha);
	void ClearRuntimeState();
	void SetPlayerCellForTest(const FIntPoint& Cell);

	UPROPERTY()
	TObjectPtr<ANaiwaP3M1MapActor> MapActor;

	UPROPERTY()
	TObjectPtr<ANaiwaP3M1EnemyActor> EnemyActor;

	TWeakObjectPtr<ANaiwaP3PlayerPawn> PlayerPawn;

	TArray<FString> Rows;
	TArray<ENaiwaP3Tile> Tiles;
	TArray<FNaiwaP3ActiveBomb> ActiveBombs;
	TMap<int32, int32> FlameExpiryTicks;
	TArray<FIntPoint> PatrolPath;

	FIntPoint SpawnCell = FIntPoint(2, 9);
	FIntPoint PlayerCell = FIntPoint(2, 9);
	FIntPoint PlayerTargetCell = FIntPoint(2, 9);
	FIntPoint PlayerDirection = FIntPoint::ZeroValue;
	FIntPoint EnemyCell = FIntPoint(4, 1);
	FIntPoint EnemyTargetCell = FIntPoint(4, 1);

	float FixedAccumulator = 0.0f;
	float PlayerMoveProgress = 0.0f;
	float EnemyMoveProgress = 0.0f;
	float PlayerCellSeconds = 0.25f;
	float EnemyCellSeconds = 0.5f;
	int32 BombFuseTicks = 25;
	int32 BombPlacementIntervalTicks = 4;
	int32 ChainDelayTicks = 2;
	int32 FlameLifeTicks = 3;
	int32 BombRange = 1;
	int32 MaxActiveBombs = 1;
	int32 CurrentTick = 0;
	int32 NextPlayerBombPlacementTick = 0;
	ENaiwaP3BombType SelectedBombType = ENaiwaP3BombType::Standard;
	int32 PatrolIndex = 0;
	int32 PlayerHitCount = 0;
	int32 EnemyHitCount = 0;
	uint64 NextBombOrder = 1;
	bool bPlayerMoving = false;
	bool bEnemyMoving = false;
	bool bM1Initialized = false;
};
