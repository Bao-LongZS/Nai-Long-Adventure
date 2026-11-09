#pragma once

#include "Core/NaiwaP7Board.h"
#include "Data/NaiwaP7DataValidation.h"

namespace NaiwaP7
{
    enum class ESimulationPhase : uint8
    {
        NoLevel,
        BetweenWaves,
        WaveActive,
        Won,
        Lost
    };

    struct FEnemySnapshot
    {
        int32 EntityId = 0;
        FName EnemyId = NAME_None;
        int32 Health = 0;
        ERoute Route = ERoute::A;
        int64 ProgressMilliCells = 0;
        FVector2D GridPosition = FVector2D::ZeroVector;
        bool bInterferenceTriggered = false;
    };

    struct FTowerSnapshot
    {
        FIntPoint Cell = FIntPoint::ZeroValue;
        FName TowerId = NAME_None;
        int32 Level = 0;
        FName TargetObstacleId = NAME_None;
        int32 AttackCooldownSteps = 0;
        int32 InterferenceStepsRemaining = 0;
    };

    struct FObstacleSnapshot
    {
        FName ObstacleId = NAME_None;
        FIntPoint Cell = FIntPoint::ZeroValue;
        int32 Health = 0;
        bool bCleared = false;
        bool bHiddenBuildSiteUnlocked = false;
    };

    struct FSimulationSnapshot
    {
        ESimulationPhase Phase = ESimulationPhase::NoLevel;
        ERoute ActiveRoute = ERoute::A;
        int32 CurrentWave = 0;
        int32 TotalWaves = 8;
        int64 PendingEnemySpawns = 0;
        int32 MilkJarHealth = 10;
        int32 Coins = 0;
        uint64 SimulationStep = 0;
        uint64 StateHash = 0;
        bool bMilkJarFull = true;
        TArray<FEnemySnapshot> Enemies;
        TArray<FTowerSnapshot> Towers;
        TArray<FObstacleSnapshot> Obstacles;
    };

    /** Deterministic, fixed-step alpha rules core. No P4/P5 state or runtime types. */
    class NAIWAP7RUNTIME_API FSimulation final
    {
    public:
        static constexpr int32 AlphaWaveCount = 8;
        static constexpr int32 StartingMilkJarHealth = 10;
        static constexpr int32 FixedStepsPerSecond = 60;
        static constexpr int32 MaxCatchUpStepsPerFrame = 4;
        static constexpr int32 MilliCellsPerCell = 1000;

        bool LoadLevel(const FLevelDefinition& Level, FString* OutError = nullptr);
        bool LoadFixture(const FAlphaDataFixture& Fixture, FString* OutError = nullptr);
        void Reset();

        bool TrySelectRoute(ERoute Route, FString* OutError = nullptr);
        bool TryBeginNextWave(FString* OutError = nullptr);
        bool TrySpawnEnemy(int32 EntityId, int32 SpeedMilliCellsPerSecond, FString* OutError = nullptr);
        bool TrySpawnEnemy(int32 EntityId, FName EnemyId, FString* OutError = nullptr);
        bool TryCompleteCurrentWave(FString* OutError = nullptr);
        bool TryPlaceTower(const FIntPoint& Cell, FName TowerId, FString* OutError = nullptr);
        bool TryUpgradeTower(const FIntPoint& Cell, FString* OutError = nullptr);
        bool TrySetTowerObstacleTarget(const FIntPoint& TowerCell, FName ObstacleId, FString* OutError = nullptr);

        /** Adds elapsed frame time and advances at most four fixed steps; backlog is retained. */
        int32 AdvanceFrame(double DeltaSeconds);

        /** Advances an exact number of logical steps. Useful for replay and deterministic tests. */
        int32 AdvanceFixedSteps(int32 StepCount);

        FSimulationSnapshot MakeSnapshot() const;
        uint64 ComputeStateHash() const;
        int32 GetDeferredCatchUpSteps() const;
        bool HasLevel() const { return Phase != ESimulationPhase::NoLevel; }
        bool IsWaveActive() const { return Phase == ESimulationPhase::WaveActive; }
        ESimulationPhase GetPhase() const { return Phase; }
        ERoute GetActiveRoute() const { return ActiveRoute; }
        const FLevelDefinition& GetLevelDefinition() const { return Level; }
        const TArray<FNaiwaP7TowerRow>& GetTowerRows() const { return TowerRows; }
        int32 GetCurrentWave() const { return CurrentWave; }
        int32 GetMilkJarHealth() const { return MilkJarHealth; }
        int32 GetCoins() const { return Coins; }
        int32 GetEnemyCount() const { return Enemies.Num(); }
        int32 GetTowerCount() const { return Towers.Num(); }
        int64 GetPendingEnemySpawnCount() const;
        uint64 GetSimulationStep() const { return SimulationStep; }
        bool CanAdvance() const;

    private:
        struct FEnemyState
        {
            int32 EntityId = 0;
            FName EnemyId = NAME_None;
            int32 Health = 0;
            ERoute Route = ERoute::A;
            int32 SpeedMilliCellsPerSecond = 0;
            int32 StepRemainder = 0;
            int64 ProgressMilliCells = 0;
            int32 SlowPercent = 0;
            int32 SlowStepsRemaining = 0;
            bool bInterferenceTriggered = false;
        };

        struct FTowerState
        {
            FIntPoint Cell = FIntPoint::ZeroValue;
            FName TowerId = NAME_None;
            int32 Level = 1;
            int32 AttackCooldownSteps = 0;
            FName TargetObstacleId = NAME_None;
            int32 InterferenceStepsRemaining = 0;
        };

        struct FObstacleState
        {
            FName ObstacleId = NAME_None;
            FIntPoint Cell = FIntPoint::ZeroValue;
            int32 Health = 0;
            int32 RewardCoins = 0;
            bool bCleared = false;
        };

        struct FPendingSpawnGroup
        {
            FName EnemyId = NAME_None;
            int32 RemainingCount = 0;
            int32 StepsUntilNextSpawn = 0;
            int32 SpawnIntervalSteps = 0;
        };

        void AdvanceOneFixedStep();
        void AdvanceWaveSpawns();
        void AdvanceTowerAttacks();
        void TryTriggerInterference(FEnemyState& Enemy, int64 PreviousProgress, int64 CurrentProgress);
        const FRoutePath& GetRoutePath(ERoute Route) const;
        FVector2D GetEnemyGridPosition(const FEnemyState& Enemy) const;
        FIntPoint GetEnemyPositionMilliCells(const FEnemyState& Enemy) const;
        const FNaiwaP7TowerRow* FindTowerRow(FName TowerId, int32 Level) const;
        const FNaiwaP7EnemyRow* FindEnemyRow(FName EnemyId) const;
        FTowerState* FindTowerAtCell(const FIntPoint& Cell);
        FObstacleState* FindObstacle(FName ObstacleId);
        bool IsBuildSiteUnlocked(const FIntPoint& Cell) const;
        static int32 GetStableCellId(const FIntPoint& Cell);
        static bool Fail(FString* OutError, const FString& Message);

        FLevelDefinition Level;
        TArray<FNaiwaP7TowerRow> TowerRows;
        TArray<FNaiwaP7EnemyRow> EnemyRows;
        TArray<FNaiwaP7ObstacleRow> ObstacleRows;
        TArray<FNaiwaP7WaveRow> WaveRows;
        TArray<FTowerState> Towers;
        TArray<FObstacleState> Obstacles;
        TArray<FPendingSpawnGroup> PendingSpawns;
        ESimulationPhase Phase = ESimulationPhase::NoLevel;
        ERoute ActiveRoute = ERoute::A;
        int32 CurrentWave = 0;
        int32 MilkJarHealth = StartingMilkJarHealth;
        int32 Coins = 0;
        uint64 SimulationStep = 0;
        double FixedAccumulatorSeconds = 0.0;
        TArray<FEnemyState> Enemies;
        int32 NextScheduledEntityId = MAX_int32;
    };
}
