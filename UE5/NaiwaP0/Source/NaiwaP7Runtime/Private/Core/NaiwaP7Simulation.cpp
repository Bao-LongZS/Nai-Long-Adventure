#include "Core/NaiwaP7Simulation.h"

namespace NaiwaP7
{
    namespace
    {
        constexpr double FixedStepSeconds = 1.0 / static_cast<double>(FSimulation::FixedStepsPerSecond);
        constexpr double StepComparisonEpsilon = 1.0e-9;
        constexpr uint64 FnvOffsetBasis = 14695981039346656037ull;
        constexpr uint64 FnvPrime = 1099511628211ull;

        uint64 HashByte(uint64 Hash, const uint8 Value)
        {
            return (Hash ^ Value) * FnvPrime;
        }

        uint64 HashUInt32(uint64 Hash, const uint32 Value)
        {
            for (uint32 Shift = 0; Shift < 32; Shift += 8)
            {
                Hash = HashByte(Hash, static_cast<uint8>((Value >> Shift) & 0xffu));
            }
            return Hash;
        }

        uint64 HashUInt64(uint64 Hash, const uint64 Value)
        {
            for (uint32 Shift = 0; Shift < 64; Shift += 8)
            {
                Hash = HashByte(Hash, static_cast<uint8>((Value >> Shift) & 0xffull));
            }
            return Hash;
        }

        uint64 HashName(uint64 Hash, const FName Name)
        {
            const FTCHARToUTF8 NameUtf8(*Name.ToString());
            Hash = HashUInt32(Hash, static_cast<uint32>(NameUtf8.Length()));
            for (int32 Index = 0; Index < NameUtf8.Length(); ++Index)
            {
                Hash = HashByte(Hash, static_cast<uint8>(NameUtf8.Get()[Index]));
            }
            return Hash;
        }
    }

    bool FSimulation::Fail(FString* OutError, const FString& Message)
    {
        if (OutError)
        {
            *OutError = Message;
        }
        return false;
    }

    bool FSimulation::LoadLevel(const FLevelDefinition& InLevel, FString* OutError)
    {
        const FValidationResult Validation = ValidateLevelDefinition(InLevel);
        if (!Validation.bIsValid)
        {
            return Fail(OutError, Validation.Error);
        }

        Level = InLevel;
        TowerRows.Reset();
        EnemyRows.Reset();
        ObstacleRows.Reset();
        WaveRows.Reset();
        Towers.Reset();
        Obstacles.Reset();
        PendingSpawns.Reset();
        Phase = ESimulationPhase::BetweenWaves;
        ActiveRoute = ERoute::A;
        CurrentWave = 0;
        MilkJarHealth = StartingMilkJarHealth;
        Coins = 0;
        SimulationStep = 0;
        FixedAccumulatorSeconds = 0.0;
        Enemies.Reset();
        NextScheduledEntityId = MAX_int32;
        if (OutError)
        {
            OutError->Reset();
        }
        return true;
    }

    bool FSimulation::LoadFixture(const FAlphaDataFixture& Fixture, FString* OutError)
    {
        const FValidationResult Validation = ValidateAlphaDataFixture(Fixture);
        if (!Validation.bIsValid)
        {
            return Fail(OutError, Validation.Error);
        }
        if (!LoadLevel(Fixture.Level, OutError))
        {
            return false;
        }

        TowerRows = Fixture.Towers;
        EnemyRows = Fixture.Enemies;
        ObstacleRows = Fixture.Obstacles;
        WaveRows = Fixture.Waves;
        Coins = Fixture.StartingCoins;
        for (const FNaiwaP7ObstacleRow& Row : ObstacleRows)
        {
            FObstacleState& Obstacle = Obstacles.AddDefaulted_GetRef();
            Obstacle.ObstacleId = Row.ObstacleId;
            Obstacle.Cell = Row.Cell;
            Obstacle.Health = Row.MaxHealth;
            Obstacle.RewardCoins = Row.RewardCoins;
        }
        if (OutError)
        {
            OutError->Reset();
        }
        return true;
    }

    void FSimulation::Reset()
    {
        Level = FLevelDefinition{};
        TowerRows.Reset();
        EnemyRows.Reset();
        ObstacleRows.Reset();
        WaveRows.Reset();
        Towers.Reset();
        Obstacles.Reset();
        PendingSpawns.Reset();
        Phase = ESimulationPhase::NoLevel;
        ActiveRoute = ERoute::A;
        CurrentWave = 0;
        MilkJarHealth = StartingMilkJarHealth;
        Coins = 0;
        SimulationStep = 0;
        FixedAccumulatorSeconds = 0.0;
        Enemies.Reset();
        NextScheduledEntityId = MAX_int32;
    }

    bool FSimulation::TrySelectRoute(const ERoute Route, FString* OutError)
    {
        if (Phase != ESimulationPhase::BetweenWaves)
        {
            return Fail(OutError, TEXT("Route selection is only allowed between waves."));
        }
        if (Route != ERoute::A && Route != ERoute::B)
        {
            return Fail(OutError, TEXT("Unknown route ID."));
        }

        ActiveRoute = Route;
        if (OutError)
        {
            OutError->Reset();
        }
        return true;
    }

    bool FSimulation::TryBeginNextWave(FString* OutError)
    {
        if (Phase != ESimulationPhase::BetweenWaves)
        {
            return Fail(OutError, TEXT("A new wave can only start during preparation."));
        }
        if (CurrentWave >= AlphaWaveCount)
        {
            return Fail(OutError, TEXT("All alpha waves have already been completed."));
        }
        if (!Enemies.IsEmpty())
        {
            return Fail(OutError, TEXT("A new wave cannot start while enemies remain."));
        }

        ++CurrentWave;
        PendingSpawns.Reset();
        if (!WaveRows.IsEmpty())
        {
            const FNaiwaP7WaveRow* Wave = WaveRows.FindByPredicate([this](const FNaiwaP7WaveRow& Row)
            {
                return Row.WaveIndex == CurrentWave;
            });
            if (!Wave)
            {
                --CurrentWave;
                return Fail(OutError, TEXT("The current P7 wave has no loaded spawn row."));
            }
            for (const FNaiwaP7WaveSpawnGroup& Spawn : Wave->Spawns)
            {
                FPendingSpawnGroup& Pending = PendingSpawns.AddDefaulted_GetRef();
                Pending.EnemyId = Spawn.EnemyId;
                Pending.RemainingCount = Spawn.Count;
                Pending.StepsUntilNextSpawn = Spawn.FirstSpawnDelaySteps;
                Pending.SpawnIntervalSteps = Spawn.SpawnIntervalSteps;
            }
        }
        Phase = ESimulationPhase::WaveActive;
        FixedAccumulatorSeconds = 0.0;
        if (OutError)
        {
            OutError->Reset();
        }
        return true;
    }

    bool FSimulation::TrySpawnEnemy(const int32 EntityId, const int32 SpeedMilliCellsPerSecond, FString* OutError)
    {
        if (Phase != ESimulationPhase::WaveActive)
        {
            return Fail(OutError, TEXT("Enemies can only spawn during an active wave."));
        }
        if (EntityId <= 0)
        {
            return Fail(OutError, TEXT("Enemy EntityId must be positive."));
        }
        if (SpeedMilliCellsPerSecond <= 0)
        {
            return Fail(OutError, TEXT("Enemy speed must be a positive milli-cell-per-second value."));
        }
        if (Enemies.ContainsByPredicate([EntityId](const FEnemyState& Enemy) { return Enemy.EntityId == EntityId; }))
        {
            return Fail(OutError, TEXT("Enemy EntityId must be unique among active enemies."));
        }

        FEnemyState Enemy;
        Enemy.EntityId = EntityId;
        Enemy.Route = ActiveRoute;
        Enemy.SpeedMilliCellsPerSecond = SpeedMilliCellsPerSecond;
        Enemy.Health = MAX_int32;

        int32 InsertIndex = 0;
        while (InsertIndex < Enemies.Num() && Enemies[InsertIndex].EntityId < EntityId)
        {
            ++InsertIndex;
        }
        Enemies.Insert(Enemy, InsertIndex);
        if (OutError)
        {
            OutError->Reset();
        }
        return true;
    }

    bool FSimulation::TrySpawnEnemy(const int32 EntityId, const FName EnemyId, FString* OutError)
    {
        const FNaiwaP7EnemyRow* Row = FindEnemyRow(EnemyId);
        if (!Row)
        {
            return Fail(OutError, TEXT("EnemyId does not resolve to a loaded P7 enemy row."));
        }
        if (!TrySpawnEnemy(EntityId, Row->SpeedMilliCellsPerSecond, OutError))
        {
            return false;
        }
        FEnemyState* Spawned = Enemies.FindByPredicate([EntityId](const FEnemyState& Enemy) { return Enemy.EntityId == EntityId; });
        check(Spawned);
        Spawned->EnemyId = EnemyId;
        Spawned->Health = Row->MaxHealth;
        return true;
    }

    bool FSimulation::TryPlaceTower(const FIntPoint& Cell, const FName TowerId, FString* OutError)
    {
        if (!HasLevel() || Phase == ESimulationPhase::Won || Phase == ESimulationPhase::Lost)
        {
            return Fail(OutError, TEXT("Towers can only be placed in an active P7 level."));
        }
        if (!IsBuildSiteUnlocked(Cell))
        {
            return Fail(OutError, TEXT("Cell is not an unlocked P7 build site."));
        }
        if (Towers.ContainsByPredicate([&Cell](const FTowerState& Tower) { return Tower.Cell == Cell; }))
        {
            return Fail(OutError, TEXT("A tower already occupies this build site."));
        }

        const FNaiwaP7TowerRow* Row = FindTowerRow(TowerId, 1);
        if (!Row)
        {
            return Fail(OutError, TEXT("TowerId has no level 1 data row."));
        }
        if (Coins < Row->CostCoins)
        {
            return Fail(OutError, TEXT("Not enough coins to place this tower."));
        }

        FTowerState Tower;
        Tower.Cell = Cell;
        Tower.TowerId = TowerId;
        int32 InsertIndex = 0;
        while (InsertIndex < Towers.Num() && GetStableCellId(Towers[InsertIndex].Cell) < GetStableCellId(Cell))
        {
            ++InsertIndex;
        }
        Towers.Insert(Tower, InsertIndex);
        Coins -= Row->CostCoins;
        if (OutError)
        {
            OutError->Reset();
        }
        return true;
    }

    bool FSimulation::TryUpgradeTower(const FIntPoint& Cell, FString* OutError)
    {
        if (!HasLevel() || Phase == ESimulationPhase::Won || Phase == ESimulationPhase::Lost)
        {
            return Fail(OutError, TEXT("Towers can only be upgraded in an active P7 level."));
        }
        FTowerState* Tower = FindTowerAtCell(Cell);
        if (!Tower)
        {
            return Fail(OutError, TEXT("There is no tower at the selected build site."));
        }
        const FNaiwaP7TowerRow* NextLevel = FindTowerRow(Tower->TowerId, Tower->Level + 1);
        if (!NextLevel)
        {
            return Fail(OutError, TEXT("Tower is already at its maximum level."));
        }
        if (Coins < NextLevel->CostCoins)
        {
            return Fail(OutError, TEXT("Not enough coins to upgrade this tower."));
        }

        Coins -= NextLevel->CostCoins;
        Tower->Level = NextLevel->Level;
        Tower->AttackCooldownSteps = 0;
        if (OutError)
        {
            OutError->Reset();
        }
        return true;
    }

    bool FSimulation::TrySetTowerObstacleTarget(
        const FIntPoint& TowerCell,
        const FName ObstacleId,
        FString* OutError)
    {
        FTowerState* Tower = FindTowerAtCell(TowerCell);
        if (!Tower)
        {
            return Fail(OutError, TEXT("There is no tower at the selected build site."));
        }
        if (ObstacleId.IsNone())
        {
            Tower->TargetObstacleId = NAME_None;
            if (OutError)
            {
                OutError->Reset();
            }
            return true;
        }
        if (Phase == ESimulationPhase::Won || Phase == ESimulationPhase::Lost)
        {
            return Fail(OutError, TEXT("Obstacle targets cannot change after the level ends."));
        }

        FObstacleState* Obstacle = FindObstacle(ObstacleId);
        if (!Obstacle || Obstacle->bCleared)
        {
            return Fail(OutError, TEXT("Obstacle target is missing or already cleared."));
        }
        const FNaiwaP7TowerRow* Row = FindTowerRow(Tower->TowerId, Tower->Level);
        if (!Row)
        {
            return Fail(OutError, TEXT("Tower data row is missing."));
        }
        const int64 Dx = static_cast<int64>(TowerCell.X - Obstacle->Cell.X) * MilliCellsPerCell;
        const int64 Dy = static_cast<int64>(TowerCell.Y - Obstacle->Cell.Y) * MilliCellsPerCell;
        const int64 Range = Row->RangeMilliCells;
        if (Dx * Dx + Dy * Dy > Range * Range)
        {
            return Fail(OutError, TEXT("Obstacle is outside this tower's range."));
        }

        Tower->TargetObstacleId = ObstacleId;
        if (OutError)
        {
            OutError->Reset();
        }
        return true;
    }

    const FNaiwaP7TowerRow* FSimulation::FindTowerRow(const FName TowerId, const int32 TowerLevel) const
    {
        return TowerRows.FindByPredicate([TowerId, TowerLevel](const FNaiwaP7TowerRow& Row)
        {
            return Row.TowerId == TowerId && Row.Level == TowerLevel;
        });
    }

    const FNaiwaP7EnemyRow* FSimulation::FindEnemyRow(const FName EnemyId) const
    {
        return EnemyRows.FindByPredicate([EnemyId](const FNaiwaP7EnemyRow& Row)
        {
            return Row.EnemyId == EnemyId;
        });
    }

    FSimulation::FTowerState* FSimulation::FindTowerAtCell(const FIntPoint& Cell)
    {
        return Towers.FindByPredicate([&Cell](const FTowerState& Tower)
        {
            return Tower.Cell == Cell;
        });
    }

    FSimulation::FObstacleState* FSimulation::FindObstacle(const FName ObstacleId)
    {
        return Obstacles.FindByPredicate([ObstacleId](const FObstacleState& Obstacle)
        {
            return Obstacle.ObstacleId == ObstacleId;
        });
    }

    bool FSimulation::IsBuildSiteUnlocked(const FIntPoint& Cell) const
    {
        if (Level.VisibleBuildCells.Contains(Cell))
        {
            return true;
        }
        const FHiddenBuildSite* Site = Level.HiddenBuildSites.FindByPredicate([&Cell](const FHiddenBuildSite& Candidate)
        {
            return Candidate.Cell == Cell;
        });
        if (!Site)
        {
            return false;
        }
        const FObstacleState* Obstacle = Obstacles.FindByPredicate([Site](const FObstacleState& Candidate)
        {
            return Candidate.ObstacleId == Site->UnlockObstacleId;
        });
        return Obstacle && Obstacle->bCleared;
    }

    int32 FSimulation::GetStableCellId(const FIntPoint& Cell)
    {
        return Cell.Y * BoardColumns + Cell.X;
    }

    bool FSimulation::CanAdvance() const
    {
        if (Phase == ESimulationPhase::WaveActive)
        {
            return true;
        }
        return Phase == ESimulationPhase::BetweenWaves && Towers.ContainsByPredicate([](const FTowerState& Tower)
        {
            return !Tower.TargetObstacleId.IsNone();
        });
    }

    bool FSimulation::TryCompleteCurrentWave(FString* OutError)
    {
        if (Phase != ESimulationPhase::WaveActive)
        {
            return Fail(OutError, TEXT("There is no active wave to complete."));
        }
        if (!Enemies.IsEmpty())
        {
            return Fail(OutError, TEXT("The wave cannot complete while enemies remain."));
        }
        if (!PendingSpawns.IsEmpty())
        {
            return Fail(OutError, TEXT("The wave cannot complete while scheduled enemies remain."));
        }

        FixedAccumulatorSeconds = 0.0;
        Phase = CurrentWave >= AlphaWaveCount ? ESimulationPhase::Won : ESimulationPhase::BetweenWaves;
        if (OutError)
        {
            OutError->Reset();
        }
        return true;
    }

    int32 FSimulation::AdvanceFrame(const double DeltaSeconds)
    {
        if (!CanAdvance() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0)
        {
            return 0;
        }

        FixedAccumulatorSeconds += DeltaSeconds;
        int32 AdvancedSteps = 0;
        while (AdvancedSteps < MaxCatchUpStepsPerFrame &&
            FixedAccumulatorSeconds + StepComparisonEpsilon >= FixedStepSeconds &&
            CanAdvance())
        {
            FixedAccumulatorSeconds = FMath::Max(0.0, FixedAccumulatorSeconds - FixedStepSeconds);
            AdvanceOneFixedStep();
            ++AdvancedSteps;
        }
        return AdvancedSteps;
    }

    int32 FSimulation::AdvanceFixedSteps(const int32 StepCount)
    {
        if (!CanAdvance() || StepCount <= 0)
        {
            return 0;
        }

        int32 AdvancedSteps = 0;
        while (AdvancedSteps < StepCount && CanAdvance())
        {
            AdvanceOneFixedStep();
            ++AdvancedSteps;
        }
        return AdvancedSteps;
    }

    void FSimulation::AdvanceOneFixedStep()
    {
        ++SimulationStep;
        if (Phase == ESimulationPhase::WaveActive)
        {
            AdvanceWaveSpawns();
            for (FTowerState& Tower : Towers)
            {
                if (Tower.InterferenceStepsRemaining > 0)
                {
                    --Tower.InterferenceStepsRemaining;
                }
            }

            for (int32 EnemyIndex = 0; EnemyIndex < Enemies.Num(); ++EnemyIndex)
            {
                FEnemyState& Enemy = Enemies[EnemyIndex];
                const int64 PreviousProgress = Enemy.ProgressMilliCells;
                const int64 EnemyMaxProgress = static_cast<int64>(GetRoutePath(Enemy.Route).Cells.Num() - 1) * MilliCellsPerCell;
                const int32 EffectiveSlowPercent = Enemy.SlowStepsRemaining > 0 ? Enemy.SlowPercent : 0;
                const int64 Numerator = static_cast<int64>(Enemy.SpeedMilliCellsPerSecond) * (100 - EffectiveSlowPercent) + Enemy.StepRemainder;
                Enemy.ProgressMilliCells = FMath::Min(
                    EnemyMaxProgress, Enemy.ProgressMilliCells + Numerator / (FixedStepsPerSecond * 100));
                Enemy.StepRemainder = static_cast<int32>(Numerator % (FixedStepsPerSecond * 100));
                if (Enemy.SlowStepsRemaining > 0)
                {
                    --Enemy.SlowStepsRemaining;
                    if (Enemy.SlowStepsRemaining == 0)
                    {
                        Enemy.SlowPercent = 0;
                    }
                }
                TryTriggerInterference(Enemy, PreviousProgress, Enemy.ProgressMilliCells);
            }

        }

        if (Phase != ESimulationPhase::Lost)
        {
            AdvanceTowerAttacks();
        }

        if (Phase == ESimulationPhase::WaveActive)
        {
            int32 LeakedEnemyCount = 0;
            for (int32 EnemyIndex = Enemies.Num() - 1; EnemyIndex >= 0; --EnemyIndex)
            {
                const int64 EnemyMaxProgress = static_cast<int64>(
                    GetRoutePath(Enemies[EnemyIndex].Route).Cells.Num() - 1) * MilliCellsPerCell;
                if (Enemies[EnemyIndex].ProgressMilliCells >= EnemyMaxProgress)
                {
                    Enemies.RemoveAt(EnemyIndex, 1, EAllowShrinking::No);
                    ++LeakedEnemyCount;
                }
            }
            if (LeakedEnemyCount > 0)
            {
                MilkJarHealth = FMath::Max(0, MilkJarHealth - LeakedEnemyCount);
                if (MilkJarHealth == 0)
                {
                    Phase = ESimulationPhase::Lost;
                    FixedAccumulatorSeconds = 0.0;
                }
            }
        }
    }

    void FSimulation::AdvanceWaveSpawns()
    {
        for (int32 GroupIndex = 0; GroupIndex < PendingSpawns.Num(); ++GroupIndex)
        {
            FPendingSpawnGroup& Group = PendingSpawns[GroupIndex];
            if (Group.StepsUntilNextSpawn > 0)
            {
                --Group.StepsUntilNextSpawn;
                if (Group.StepsUntilNextSpawn > 0)
                {
                    continue;
                }
            }

            while (NextScheduledEntityId > 0 && Enemies.ContainsByPredicate([this](const FEnemyState& Enemy)
            {
                return Enemy.EntityId == NextScheduledEntityId;
            }))
            {
                --NextScheduledEntityId;
            }
            if (NextScheduledEntityId <= 0)
            {
                continue;
            }

            const int32 EntityId = NextScheduledEntityId--;
            const bool bSpawned = TrySpawnEnemy(EntityId, Group.EnemyId, nullptr);
            if (!bSpawned)
            {
                continue;
            }
            --Group.RemainingCount;
            if (Group.RemainingCount <= 0)
            {
                PendingSpawns.RemoveAt(GroupIndex, 1, EAllowShrinking::No);
                --GroupIndex;
            }
            else
            {
                Group.StepsUntilNextSpawn = Group.SpawnIntervalSteps;
            }
        }
    }

    void FSimulation::TryTriggerInterference(
        FEnemyState& Enemy,
        const int64 PreviousProgress,
        const int64 CurrentProgress)
    {
        if (Enemy.bInterferenceTriggered)
        {
            return;
        }
        const FNaiwaP7EnemyRow* EnemyRow = FindEnemyRow(Enemy.EnemyId);
        if (!EnemyRow || EnemyRow->Kind != ENaiwaP7EnemyKind::Interferer)
        {
            return;
        }

        const TArray<FIntPoint>& RouteCells = GetRoutePath(Enemy.Route).Cells;
        const int32 TriggerIndex = RouteCells.Find(Level.InterferenceTriggerCell);
        if (TriggerIndex == INDEX_NONE)
        {
            return;
        }
        const int64 TriggerProgress = static_cast<int64>(TriggerIndex) * MilliCellsPerCell;
        if (PreviousProgress >= TriggerProgress || CurrentProgress < TriggerProgress)
        {
            return;
        }

        Enemy.bInterferenceTriggered = true;
        const int64 RadiusSquared = static_cast<int64>(EnemyRow->InterferenceRadiusMilliCells) * EnemyRow->InterferenceRadiusMilliCells;
        FTowerState* TargetTower = nullptr;
        int64 BestDistanceSquared = MAX_int64;
        for (FTowerState& Tower : Towers)
        {
            const int64 Dx = static_cast<int64>(Tower.Cell.X - Level.InterferenceTriggerCell.X) * MilliCellsPerCell;
            const int64 Dy = static_cast<int64>(Tower.Cell.Y - Level.InterferenceTriggerCell.Y) * MilliCellsPerCell;
            const int64 DistanceSquared = Dx * Dx + Dy * Dy;
            if (DistanceSquared > RadiusSquared)
            {
                continue;
            }
            if (DistanceSquared < BestDistanceSquared ||
                (DistanceSquared == BestDistanceSquared && TargetTower &&
                    GetStableCellId(Tower.Cell) < GetStableCellId(TargetTower->Cell)))
            {
                TargetTower = &Tower;
                BestDistanceSquared = DistanceSquared;
            }
        }
        if (TargetTower && TargetTower->InterferenceStepsRemaining == 0)
        {
            TargetTower->InterferenceStepsRemaining = EnemyRow->InterferenceDurationSteps;
        }
    }

    void FSimulation::AdvanceTowerAttacks()
    {
        TMap<int32, int64> QueuedEnemyDamage;
        struct FSlowApplication
        {
            int32 EntityId = 0;
            int32 SlowPercent = 0;
            int32 DurationSteps = 0;
        };
        TArray<FSlowApplication> QueuedSlows;
        TMap<FName, int64> QueuedObstacleDamage;
        const bool bCanAttackEnemies = Phase == ESimulationPhase::WaveActive;

        const auto QueueDamage = [](int64& Existing, const int32 Damage)
        {
            Existing = FMath::Min<int64>(MAX_int32, Existing + static_cast<int64>(Damage));
        };

        for (FTowerState& Tower : Towers)
        {
            const FNaiwaP7TowerRow* Row = FindTowerRow(Tower.TowerId, Tower.Level);
            if (!Row)
            {
                continue;
            }

            FObstacleState* TargetObstacle = Tower.TargetObstacleId.IsNone() ? nullptr : FindObstacle(Tower.TargetObstacleId);
            if (TargetObstacle && TargetObstacle->bCleared)
            {
                Tower.TargetObstacleId = NAME_None;
                TargetObstacle = nullptr;
            }
            const bool bHasObstacleTarget = TargetObstacle != nullptr;
            if (!bCanAttackEnemies && !bHasObstacleTarget)
            {
                continue;
            }

            if (Tower.AttackCooldownSteps > 0)
            {
                --Tower.AttackCooldownSteps;
            }
            if (Tower.InterferenceStepsRemaining > 0)
            {
                continue;
            }
            if (Tower.AttackCooldownSteps > 0)
            {
                continue;
            }

            if (bHasObstacleTarget)
            {
                QueueDamage(QueuedObstacleDamage.FindOrAdd(TargetObstacle->ObstacleId), Row->Damage);
                Tower.AttackCooldownSteps = Row->AttackIntervalSteps;
                continue;
            }

            if (!bCanAttackEnemies)
            {
                continue;
            }

            const int64 RangeSquared = static_cast<int64>(Row->RangeMilliCells) * Row->RangeMilliCells;
            int32 TargetIndex = INDEX_NONE;
            int64 BestDistanceToEndpoint = MAX_int64;
            int64 BestDistanceSquared = MAX_int64;
            for (int32 EnemyIndex = 0; EnemyIndex < Enemies.Num(); ++EnemyIndex)
            {
                const int64 EnemyMaxProgress = static_cast<int64>(
                    GetRoutePath(Enemies[EnemyIndex].Route).Cells.Num() - 1) * MilliCellsPerCell;
                const int64 DistanceToEndpoint = EnemyMaxProgress - Enemies[EnemyIndex].ProgressMilliCells;
                const FIntPoint Position = GetEnemyPositionMilliCells(Enemies[EnemyIndex]);
                const int64 Dx = static_cast<int64>(Position.X) - static_cast<int64>(Tower.Cell.X) * MilliCellsPerCell;
                const int64 Dy = static_cast<int64>(Position.Y) - static_cast<int64>(Tower.Cell.Y) * MilliCellsPerCell;
                if (Dx * Dx + Dy * Dy > RangeSquared)
                {
                    continue;
                }
                const int64 DistanceSquared = Dx * Dx + Dy * Dy;
                if (DistanceToEndpoint < BestDistanceToEndpoint ||
                    (DistanceToEndpoint == BestDistanceToEndpoint &&
                        (DistanceSquared < BestDistanceSquared ||
                            (DistanceSquared == BestDistanceSquared &&
                                (TargetIndex == INDEX_NONE || Enemies[EnemyIndex].EntityId < Enemies[TargetIndex].EntityId)))))
                {
                    TargetIndex = EnemyIndex;
                    BestDistanceToEndpoint = DistanceToEndpoint;
                    BestDistanceSquared = DistanceSquared;
                }
            }
            if (TargetIndex == INDEX_NONE)
            {
                continue;
            }

            const FEnemyState& Target = Enemies[TargetIndex];
            if (Row->Role == ENaiwaP7TowerRole::AreaDamage)
            {
                const int64 SplashSquared = static_cast<int64>(Row->SplashRadiusMilliCells) * Row->SplashRadiusMilliCells;
                for (const FEnemyState& Enemy : Enemies)
                {
                    const FIntPoint Position = GetEnemyPositionMilliCells(Enemy);
                    const FIntPoint TargetPosition = GetEnemyPositionMilliCells(Target);
                    const int64 Dx = static_cast<int64>(Position.X) - TargetPosition.X;
                    const int64 Dy = static_cast<int64>(Position.Y) - TargetPosition.Y;
                    if (Dx * Dx + Dy * Dy <= SplashSquared)
                    {
                        QueueDamage(QueuedEnemyDamage.FindOrAdd(Enemy.EntityId), Row->Damage);
                    }
                }
            }
            else
            {
                QueueDamage(QueuedEnemyDamage.FindOrAdd(Target.EntityId), Row->Damage);
            }

            if (Row->Role == ENaiwaP7TowerRole::SlowSupport)
            {
                FSlowApplication& Slow = QueuedSlows.AddDefaulted_GetRef();
                Slow.EntityId = Target.EntityId;
                Slow.SlowPercent = Row->SlowPercent;
                Slow.DurationSteps = Row->SlowDurationSteps;
            }
            Tower.AttackCooldownSteps = Row->AttackIntervalSteps;
        }

        for (const TPair<int32, int64>& DamagePair : QueuedEnemyDamage)
        {
            FEnemyState* Enemy = Enemies.FindByPredicate([&DamagePair](const FEnemyState& Candidate)
            {
                return Candidate.EntityId == DamagePair.Key;
            });
            if (Enemy)
            {
                Enemy->Health = static_cast<int32>(FMath::Max<int64>(0, static_cast<int64>(Enemy->Health) - DamagePair.Value));
            }
        }
        Enemies.RemoveAll([](const FEnemyState& Enemy) { return Enemy.Health <= 0; });

        for (const FSlowApplication& Slow : QueuedSlows)
        {
            FEnemyState* Enemy = Enemies.FindByPredicate([&Slow](const FEnemyState& Candidate)
            {
                return Candidate.EntityId == Slow.EntityId;
            });
            if (Enemy)
            {
                Enemy->SlowPercent = FMath::Max(Enemy->SlowPercent, Slow.SlowPercent);
                Enemy->SlowStepsRemaining = FMath::Max(Enemy->SlowStepsRemaining, Slow.DurationSteps);
            }
        }

        for (const TPair<FName, int64>& DamagePair : QueuedObstacleDamage)
        {
            FObstacleState* Obstacle = FindObstacle(DamagePair.Key);
            if (!Obstacle || Obstacle->bCleared)
            {
                continue;
            }
            Obstacle->Health = static_cast<int32>(FMath::Max<int64>(0, static_cast<int64>(Obstacle->Health) - DamagePair.Value));
            if (Obstacle->Health == 0)
            {
                Obstacle->bCleared = true;
                Coins = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Coins) + Obstacle->RewardCoins));
                for (FTowerState& Tower : Towers)
                {
                    if (Tower.TargetObstacleId == Obstacle->ObstacleId)
                    {
                        Tower.TargetObstacleId = NAME_None;
                    }
                }
            }
        }
    }

    const FRoutePath& FSimulation::GetRoutePath(const ERoute Route) const
    {
        return Route == ERoute::B ? Level.RouteB : Level.RouteA;
    }

    FVector2D FSimulation::GetEnemyGridPosition(const FEnemyState& Enemy) const
    {
        const TArray<FIntPoint>& RouteCells = GetRoutePath(Enemy.Route).Cells;
        if (RouteCells.IsEmpty())
        {
            return FVector2D::ZeroVector;
        }

        const int64 MaxProgress = static_cast<int64>(RouteCells.Num() - 1) * MilliCellsPerCell;
        const int64 ClampedProgress = FMath::Clamp<int64>(Enemy.ProgressMilliCells, 0, MaxProgress);
        const int32 SegmentIndex = FMath::Min(
            static_cast<int32>(ClampedProgress / MilliCellsPerCell), RouteCells.Num() - 1);
        if (SegmentIndex >= RouteCells.Num() - 1)
        {
            return FVector2D(RouteCells.Last().X, RouteCells.Last().Y);
        }

        const double SegmentAlpha = static_cast<double>(ClampedProgress % MilliCellsPerCell) / MilliCellsPerCell;
        const FIntPoint& Start = RouteCells[SegmentIndex];
        const FIntPoint& End = RouteCells[SegmentIndex + 1];
        return FVector2D(
            FMath::Lerp(static_cast<double>(Start.X), static_cast<double>(End.X), SegmentAlpha),
            FMath::Lerp(static_cast<double>(Start.Y), static_cast<double>(End.Y), SegmentAlpha));
    }

    FIntPoint FSimulation::GetEnemyPositionMilliCells(const FEnemyState& Enemy) const
    {
        const FVector2D Position = GetEnemyGridPosition(Enemy);
        return FIntPoint(
            FMath::RoundToInt(Position.X * MilliCellsPerCell),
            FMath::RoundToInt(Position.Y * MilliCellsPerCell));
    }

    FSimulationSnapshot FSimulation::MakeSnapshot() const
    {
        FSimulationSnapshot Snapshot;
        Snapshot.Phase = Phase;
        Snapshot.ActiveRoute = ActiveRoute;
        Snapshot.CurrentWave = CurrentWave;
        Snapshot.PendingEnemySpawns = GetPendingEnemySpawnCount();
        Snapshot.MilkJarHealth = MilkJarHealth;
        Snapshot.Coins = Coins;
        Snapshot.SimulationStep = SimulationStep;
        Snapshot.StateHash = ComputeStateHash();
        Snapshot.bMilkJarFull = MilkJarHealth == StartingMilkJarHealth;
        Snapshot.Enemies.Reserve(Enemies.Num());

        for (const FEnemyState& Enemy : Enemies)
        {
            FEnemySnapshot& EnemyView = Snapshot.Enemies.AddDefaulted_GetRef();
            EnemyView.EntityId = Enemy.EntityId;
            EnemyView.EnemyId = Enemy.EnemyId;
            EnemyView.Health = Enemy.Health;
            EnemyView.Route = Enemy.Route;
            EnemyView.ProgressMilliCells = Enemy.ProgressMilliCells;
            EnemyView.GridPosition = GetEnemyGridPosition(Enemy);
            EnemyView.bInterferenceTriggered = Enemy.bInterferenceTriggered;
        }
        Snapshot.Towers.Reserve(Towers.Num());
        for (const FTowerState& Tower : Towers)
        {
            FTowerSnapshot& TowerView = Snapshot.Towers.AddDefaulted_GetRef();
            TowerView.Cell = Tower.Cell;
            TowerView.TowerId = Tower.TowerId;
            TowerView.Level = Tower.Level;
            TowerView.TargetObstacleId = Tower.TargetObstacleId;
            TowerView.AttackCooldownSteps = Tower.AttackCooldownSteps;
            TowerView.InterferenceStepsRemaining = Tower.InterferenceStepsRemaining;
        }
        Snapshot.Obstacles.Reserve(Obstacles.Num());
        for (const FObstacleState& Obstacle : Obstacles)
        {
            FObstacleSnapshot& ObstacleView = Snapshot.Obstacles.AddDefaulted_GetRef();
            ObstacleView.ObstacleId = Obstacle.ObstacleId;
            ObstacleView.Cell = Obstacle.Cell;
            ObstacleView.Health = Obstacle.Health;
            ObstacleView.bCleared = Obstacle.bCleared;
        }
        for (int32 ObstacleIndex = 0; ObstacleIndex < Snapshot.Obstacles.Num(); ++ObstacleIndex)
        {
            FObstacleSnapshot& ObstacleView = Snapshot.Obstacles[ObstacleIndex];
            const FHiddenBuildSite* Site = Level.HiddenBuildSites.FindByPredicate([&ObstacleView](const FHiddenBuildSite& Candidate)
            {
                return Candidate.UnlockObstacleId == ObstacleView.ObstacleId;
            });
            if (Site)
            {
                ObstacleView.bHiddenBuildSiteUnlocked = IsBuildSiteUnlocked(Site->Cell);
            }
        }
        return Snapshot;
    }

    uint64 FSimulation::ComputeStateHash() const
    {
        uint64 Hash = FnvOffsetBasis;
        Hash = HashName(Hash, HasLevel() ? Level.LevelId : NAME_None);
        Hash = HashUInt32(Hash, static_cast<uint32>(Level.InterferenceTriggerCell.X));
        Hash = HashUInt32(Hash, static_cast<uint32>(Level.InterferenceTriggerCell.Y));
        Hash = HashUInt32(Hash, static_cast<uint32>(Level.RouteA.Cells.Num()));
        for (const FIntPoint& Cell : Level.RouteA.Cells)
        {
            Hash = HashUInt32(Hash, static_cast<uint32>(Cell.X));
            Hash = HashUInt32(Hash, static_cast<uint32>(Cell.Y));
        }
        Hash = HashUInt32(Hash, static_cast<uint32>(Level.RouteB.Cells.Num()));
        for (const FIntPoint& Cell : Level.RouteB.Cells)
        {
            Hash = HashUInt32(Hash, static_cast<uint32>(Cell.X));
            Hash = HashUInt32(Hash, static_cast<uint32>(Cell.Y));
        }
        Hash = HashUInt32(Hash, static_cast<uint32>(Level.VisibleBuildCells.Num()));
        for (const FIntPoint& Cell : Level.VisibleBuildCells)
        {
            Hash = HashUInt32(Hash, static_cast<uint32>(Cell.X));
            Hash = HashUInt32(Hash, static_cast<uint32>(Cell.Y));
        }
        Hash = HashUInt32(Hash, static_cast<uint32>(Level.ObstacleIds.Num()));
        for (const FName ObstacleId : Level.ObstacleIds)
        {
            Hash = HashName(Hash, ObstacleId);
        }
        Hash = HashUInt32(Hash, static_cast<uint32>(Level.HiddenBuildSites.Num()));
        for (const FHiddenBuildSite& Site : Level.HiddenBuildSites)
        {
            Hash = HashUInt32(Hash, static_cast<uint32>(Site.Cell.X));
            Hash = HashUInt32(Hash, static_cast<uint32>(Site.Cell.Y));
            Hash = HashName(Hash, Site.UnlockObstacleId);
        }
        Hash = HashUInt32(Hash, static_cast<uint32>(TowerRows.Num()));
        for (const FNaiwaP7TowerRow& Row : TowerRows)
        {
            Hash = HashName(Hash, Row.TowerId);
            Hash = HashByte(Hash, static_cast<uint8>(Row.Role));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.Level));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.CostCoins));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.Damage));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.AttackIntervalSteps));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.RangeMilliCells));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.SplashRadiusMilliCells));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.SlowPercent));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.SlowDurationSteps));
        }
        Hash = HashUInt32(Hash, static_cast<uint32>(EnemyRows.Num()));
        for (const FNaiwaP7EnemyRow& Row : EnemyRows)
        {
            Hash = HashName(Hash, Row.EnemyId);
            Hash = HashByte(Hash, static_cast<uint8>(Row.Kind));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.MaxHealth));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.SpeedMilliCellsPerSecond));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.InterferenceRadiusMilliCells));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.InterferenceDurationSteps));
        }
        Hash = HashUInt32(Hash, static_cast<uint32>(ObstacleRows.Num()));
        for (const FNaiwaP7ObstacleRow& Row : ObstacleRows)
        {
            Hash = HashName(Hash, Row.ObstacleId);
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.Cell.X));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.Cell.Y));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.MaxHealth));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.RewardCoins));
        }
        Hash = HashUInt32(Hash, static_cast<uint32>(WaveRows.Num()));
        for (const FNaiwaP7WaveRow& Row : WaveRows)
        {
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.WaveIndex));
            Hash = HashUInt32(Hash, static_cast<uint32>(Row.Spawns.Num()));
            for (const FNaiwaP7WaveSpawnGroup& Spawn : Row.Spawns)
            {
                Hash = HashName(Hash, Spawn.EnemyId);
                Hash = HashUInt32(Hash, static_cast<uint32>(Spawn.Count));
                Hash = HashUInt32(Hash, static_cast<uint32>(Spawn.FirstSpawnDelaySteps));
                Hash = HashUInt32(Hash, static_cast<uint32>(Spawn.SpawnIntervalSteps));
            }
        }
        Hash = HashByte(Hash, static_cast<uint8>(Phase));
        Hash = HashByte(Hash, static_cast<uint8>(ActiveRoute));
        Hash = HashUInt32(Hash, static_cast<uint32>(CurrentWave));
        Hash = HashUInt32(Hash, static_cast<uint32>(MilkJarHealth));
        Hash = HashUInt32(Hash, static_cast<uint32>(Coins));
        Hash = HashUInt64(Hash, SimulationStep);
        Hash = HashUInt32(Hash, static_cast<uint32>(Enemies.Num()));

        for (const FEnemyState& Enemy : Enemies)
        {
            Hash = HashUInt32(Hash, static_cast<uint32>(Enemy.EntityId));
            Hash = HashName(Hash, Enemy.EnemyId);
            Hash = HashUInt32(Hash, static_cast<uint32>(Enemy.Health));
            Hash = HashByte(Hash, static_cast<uint8>(Enemy.Route));
            Hash = HashUInt32(Hash, static_cast<uint32>(Enemy.SpeedMilliCellsPerSecond));
            Hash = HashUInt32(Hash, static_cast<uint32>(Enemy.StepRemainder));
            Hash = HashUInt64(Hash, static_cast<uint64>(Enemy.ProgressMilliCells));
            Hash = HashUInt32(Hash, static_cast<uint32>(Enemy.SlowPercent));
            Hash = HashUInt32(Hash, static_cast<uint32>(Enemy.SlowStepsRemaining));
            Hash = HashByte(Hash, Enemy.bInterferenceTriggered ? 1 : 0);
        }
        Hash = HashUInt32(Hash, static_cast<uint32>(Towers.Num()));
        for (const FTowerState& Tower : Towers)
        {
            Hash = HashUInt32(Hash, static_cast<uint32>(Tower.Cell.X));
            Hash = HashUInt32(Hash, static_cast<uint32>(Tower.Cell.Y));
            Hash = HashName(Hash, Tower.TowerId);
            Hash = HashUInt32(Hash, static_cast<uint32>(Tower.Level));
            Hash = HashUInt32(Hash, static_cast<uint32>(Tower.AttackCooldownSteps));
            Hash = HashName(Hash, Tower.TargetObstacleId);
            Hash = HashUInt32(Hash, static_cast<uint32>(Tower.InterferenceStepsRemaining));
        }
        Hash = HashUInt32(Hash, static_cast<uint32>(Obstacles.Num()));
        for (const FObstacleState& Obstacle : Obstacles)
        {
            Hash = HashName(Hash, Obstacle.ObstacleId);
            Hash = HashUInt32(Hash, static_cast<uint32>(Obstacle.Cell.X));
            Hash = HashUInt32(Hash, static_cast<uint32>(Obstacle.Cell.Y));
            Hash = HashUInt32(Hash, static_cast<uint32>(Obstacle.Health));
            Hash = HashUInt32(Hash, static_cast<uint32>(Obstacle.RewardCoins));
            Hash = HashByte(Hash, Obstacle.bCleared ? 1 : 0);
        }
        Hash = HashUInt32(Hash, static_cast<uint32>(PendingSpawns.Num()));
        for (const FPendingSpawnGroup& Spawn : PendingSpawns)
        {
            Hash = HashName(Hash, Spawn.EnemyId);
            Hash = HashUInt32(Hash, static_cast<uint32>(Spawn.RemainingCount));
            Hash = HashUInt32(Hash, static_cast<uint32>(Spawn.StepsUntilNextSpawn));
            Hash = HashUInt32(Hash, static_cast<uint32>(Spawn.SpawnIntervalSteps));
        }
        Hash = HashUInt32(Hash, static_cast<uint32>(NextScheduledEntityId));
        return Hash;
    }

    int32 FSimulation::GetDeferredCatchUpSteps() const
    {
        if (FixedAccumulatorSeconds <= 0.0)
        {
            return 0;
        }
        return FMath::Max(0, FMath::FloorToInt((FixedAccumulatorSeconds + StepComparisonEpsilon) / FixedStepSeconds));
    }

    int64 FSimulation::GetPendingEnemySpawnCount() const
    {
        int64 Count = 0;
        for (const FPendingSpawnGroup& Group : PendingSpawns)
        {
            Count += static_cast<int64>(Group.RemainingCount);
        }
        return Count;
    }
}
