#include "Data/NaiwaP7DataValidation.h"
#include "Core/NaiwaP7Simulation.h"

namespace NaiwaP7
{
    namespace
    {
        FValidationResult InvalidData(const FString& Error)
        {
            FValidationResult Result;
            Result.Error = Error;
            return Result;
        }

        bool HasExactLevelsOneThroughThree(const TSet<int32>& Levels)
        {
            return Levels.Num() == 3 && Levels.Contains(1) && Levels.Contains(2) && Levels.Contains(3);
        }

        template <typename TRow>
        bool CopyTypedTableRows(const UDataTable* Table, const TCHAR* TableLabel, TArray<TRow>& OutRows, FString& OutError)
        {
            if (!Table)
            {
                OutError = FString::Printf(TEXT("%s DataTable is missing."), TableLabel);
                return false;
            }
            if (Table->GetRowStruct() != TRow::StaticStruct())
            {
                OutError = FString::Printf(TEXT("%s DataTable has the wrong row struct."), TableLabel);
                return false;
            }

            TArray<TRow*> Rows;
            Table->GetAllRows<TRow>(FString::Printf(TEXT("P7 %s table assembly"), TableLabel), Rows);
            OutRows.Reset(Rows.Num());
            for (const TRow* Row : Rows)
            {
                if (!Row)
                {
                    OutError = FString::Printf(TEXT("%s DataTable contains an empty row."), TableLabel);
                    return false;
                }
                OutRows.Add(*Row);
            }
            return true;
        }
    }

    FValidationResult ValidateAlphaDataFixture(const FAlphaDataFixture& Fixture)
    {
        if (Fixture.StartingCoins < 0)
        {
            return InvalidData(TEXT("StartingCoins cannot be negative."));
        }
        return ValidateAlphaDataFixture(
            Fixture.Level, Fixture.Towers, Fixture.Enemies, Fixture.Obstacles, Fixture.Waves);
    }

    FValidationResult BuildAlphaDataFixtureFromTables(
        const UDataTable* LevelTable,
        const UDataTable* TowerTable,
        const UDataTable* EnemyTable,
        const UDataTable* ObstacleTable,
        const UDataTable* WaveTable,
        const FName LevelRowName,
        FAlphaDataFixture& OutFixture)
    {
        if (!LevelTable)
        {
            return InvalidData(TEXT("Level DataTable is missing."));
        }
        if (LevelTable->GetRowStruct() != FNaiwaP7LevelRow::StaticStruct())
        {
            return InvalidData(TEXT("Level DataTable has the wrong row struct."));
        }
        if (LevelRowName.IsNone())
        {
            return InvalidData(TEXT("Level row name must be set."));
        }

        const FNaiwaP7LevelRow* LevelRow = LevelTable->FindRow<FNaiwaP7LevelRow>(
            LevelRowName, TEXT("P7 alpha fixture assembly"), false);
        if (!LevelRow)
        {
            return InvalidData(FString::Printf(TEXT("Level DataTable has no row named %s."), *LevelRowName.ToString()));
        }
        if (LevelRow->LevelId != LevelRowName)
        {
            return InvalidData(TEXT("LevelId must match its Level DataTable row name."));
        }

        FAlphaDataFixture Candidate;
        Candidate.Level.LevelId = LevelRow->LevelId;
        Candidate.Level.RouteA.Cells = LevelRow->RouteACells;
        Candidate.Level.RouteB.Cells = LevelRow->RouteBCells;
        Candidate.Level.InterferenceTriggerCell = LevelRow->InterferenceTriggerCell;
        Candidate.Level.VisibleBuildCells = LevelRow->VisibleBuildCells;
        Candidate.Level.ObstacleIds = LevelRow->ObstacleIds;
        Candidate.StartingCoins = LevelRow->StartingCoins;
        Candidate.Level.HiddenBuildSites.Reserve(LevelRow->HiddenBuildSites.Num());
        for (const FNaiwaP7HiddenBuildSiteRow& Site : LevelRow->HiddenBuildSites)
        {
            FHiddenBuildSite& ConvertedSite = Candidate.Level.HiddenBuildSites.AddDefaulted_GetRef();
            ConvertedSite.Cell = Site.Cell;
            ConvertedSite.UnlockObstacleId = Site.UnlockObstacleId;
        }

        FString TableError;
        if (!CopyTypedTableRows(TowerTable, TEXT("Tower"), Candidate.Towers, TableError) ||
            !CopyTypedTableRows(EnemyTable, TEXT("Enemy"), Candidate.Enemies, TableError) ||
            !CopyTypedTableRows(ObstacleTable, TEXT("Obstacle"), Candidate.Obstacles, TableError) ||
            !CopyTypedTableRows(WaveTable, TEXT("Wave"), Candidate.Waves, TableError))
        {
            return InvalidData(TableError);
        }

        Candidate.Towers.Sort([](const FNaiwaP7TowerRow& A, const FNaiwaP7TowerRow& B)
        {
            const int32 IdOrder = A.TowerId.Compare(B.TowerId);
            return IdOrder == 0 ? A.Level < B.Level : IdOrder < 0;
        });
        Candidate.Enemies.Sort([](const FNaiwaP7EnemyRow& A, const FNaiwaP7EnemyRow& B)
        {
            return A.EnemyId.Compare(B.EnemyId) < 0;
        });
        Candidate.Obstacles.Sort([](const FNaiwaP7ObstacleRow& A, const FNaiwaP7ObstacleRow& B)
        {
            return A.ObstacleId.Compare(B.ObstacleId) < 0;
        });
        Candidate.Waves.Sort([](const FNaiwaP7WaveRow& A, const FNaiwaP7WaveRow& B)
        {
            return A.WaveIndex < B.WaveIndex;
        });

        const FValidationResult Validation = ValidateAlphaDataFixture(Candidate);
        if (!Validation.bIsValid)
        {
            return Validation;
        }
        OutFixture = MoveTemp(Candidate);
        FValidationResult Result;
        Result.bIsValid = true;
        return Result;
    }

    FValidationResult ValidateAlphaDataFixture(
        const FLevelDefinition& Level,
        const TArray<FNaiwaP7TowerRow>& Towers,
        const TArray<FNaiwaP7EnemyRow>& Enemies,
        const TArray<FNaiwaP7ObstacleRow>& Obstacles,
        const TArray<FNaiwaP7WaveRow>& Waves)
    {
        const FValidationResult LevelValidation = ValidateLevelDefinition(Level);
        if (!LevelValidation.bIsValid)
        {
            return LevelValidation;
        }

        if (Towers.Num() != 9)
        {
            return InvalidData(TEXT("Alpha requires three tower IDs with three levels each."));
        }

        TMap<FName, TSet<int32>> TowerLevels;
        TMap<FName, ENaiwaP7TowerRole> TowerRoles;
        TSet<uint8> Roles;
        for (const FNaiwaP7TowerRow& Tower : Towers)
        {
            if (Tower.TowerId.IsNone() || Tower.Level < 1 || Tower.Level > 3 || Tower.CostCoins <= 0 ||
                Tower.Damage < 0 || Tower.AttackIntervalSteps <= 0 || Tower.RangeMilliCells <= 0 ||
                Tower.RangeMilliCells > 30000 || Tower.SplashRadiusMilliCells > 30000)
            {
                return InvalidData(TEXT("Tower rows require an ID, level 1-3, positive cost/interval/range, and non-negative damage."));
            }

            TSet<int32>& Levels = TowerLevels.FindOrAdd(Tower.TowerId);
            if (Levels.Contains(Tower.Level))
            {
                return InvalidData(FString::Printf(TEXT("Tower %s has a duplicate level %d."), *Tower.TowerId.ToString(), Tower.Level));
            }
            Levels.Add(Tower.Level);

            if (const ENaiwaP7TowerRole* ExistingRole = TowerRoles.Find(Tower.TowerId))
            {
                if (*ExistingRole != Tower.Role)
                {
                    return InvalidData(FString::Printf(TEXT("Tower %s changes role between levels."), *Tower.TowerId.ToString()));
                }
            }
            else
            {
                TowerRoles.Add(Tower.TowerId, Tower.Role);
                Roles.Add(static_cast<uint8>(Tower.Role));
            }

            if (Tower.Role == ENaiwaP7TowerRole::AreaDamage)
            {
                if (Tower.SplashRadiusMilliCells <= 0 || Tower.SlowPercent != 0 || Tower.SlowDurationSteps != 0)
                {
                    return InvalidData(TEXT("Area towers require a positive splash radius and no slow fields."));
                }
            }
            else if (Tower.Role == ENaiwaP7TowerRole::SlowSupport)
            {
                if (Tower.SplashRadiusMilliCells != 0 || Tower.SlowPercent <= 0 || Tower.SlowPercent >= 100 || Tower.SlowDurationSteps <= 0)
                {
                    return InvalidData(TEXT("Slow towers require valid slow fields and no splash radius."));
                }
            }
            else if (Tower.Role == ENaiwaP7TowerRole::DirectDamage)
            {
                if (Tower.SplashRadiusMilliCells != 0 || Tower.SlowPercent != 0 || Tower.SlowDurationSteps != 0)
                {
                    return InvalidData(TEXT("Direct towers cannot define area or slow fields."));
                }
            }
            else
            {
                return InvalidData(TEXT("Tower row contains an unknown role."));
            }
        }

        if (TowerLevels.Num() != 3 || Roles.Num() != 3)
        {
            return InvalidData(TEXT("Alpha towers must provide three distinct tactical roles."));
        }
        for (const TPair<FName, TSet<int32>>& Pair : TowerLevels)
        {
            if (!HasExactLevelsOneThroughThree(Pair.Value))
            {
                return InvalidData(FString::Printf(TEXT("Tower %s must provide levels 1, 2, and 3."), *Pair.Key.ToString()));
            }
        }

        if (Enemies.Num() != 3)
        {
            return InvalidData(TEXT("Alpha requires one normal, one fast, and one interferer enemy row."));
        }
        TSet<FName> EnemyIds;
        TSet<uint8> EnemyKinds;
        for (const FNaiwaP7EnemyRow& Enemy : Enemies)
        {
            if (Enemy.EnemyId.IsNone() || EnemyIds.Contains(Enemy.EnemyId) || Enemy.MaxHealth <= 0 || Enemy.SpeedMilliCellsPerSecond <= 0)
            {
                return InvalidData(TEXT("Enemy rows require unique IDs, positive health, and positive speed."));
            }
            EnemyIds.Add(Enemy.EnemyId);
            EnemyKinds.Add(static_cast<uint8>(Enemy.Kind));

            if (Enemy.Kind == ENaiwaP7EnemyKind::Interferer)
            {
                if (Enemy.InterferenceRadiusMilliCells <= 0 || Enemy.InterferenceRadiusMilliCells > 30000 ||
                    Enemy.InterferenceDurationSteps != 90)
                {
                    return InvalidData(TEXT("The alpha interferer requires a radius in 1-30000 milli-cells and a fixed 90-step duration."));
                }
            }
            else if (Enemy.Kind == ENaiwaP7EnemyKind::Normal || Enemy.Kind == ENaiwaP7EnemyKind::Fast)
            {
                if (Enemy.InterferenceRadiusMilliCells != 0 || Enemy.InterferenceDurationSteps != 0)
                {
                    return InvalidData(TEXT("Non-interferer enemies cannot define tower interference fields."));
                }
            }
            else
            {
                return InvalidData(TEXT("Enemy row contains an unknown kind."));
            }
        }
        if (EnemyKinds.Num() != 3)
        {
            return InvalidData(TEXT("Alpha enemy rows must cover all three enemy kinds."));
        }

        if (Obstacles.Num() != HiddenBuildSiteCount)
        {
            return InvalidData(FString::Printf(TEXT("Alpha requires exactly %d obstacle rows."), HiddenBuildSiteCount));
        }
        TSet<FName> ObstacleIds;
        TSet<FIntPoint> ObstacleCells;
        TSet<FIntPoint> RouteCells;
        for (const FIntPoint& Cell : Level.RouteA.Cells)
        {
            RouteCells.Add(Cell);
        }
        for (const FIntPoint& Cell : Level.RouteB.Cells)
        {
            RouteCells.Add(Cell);
        }
        TSet<FIntPoint> BuildCells;
        for (const FIntPoint& Cell : Level.VisibleBuildCells)
        {
            BuildCells.Add(Cell);
        }
        for (const FHiddenBuildSite& Site : Level.HiddenBuildSites)
        {
            BuildCells.Add(Site.Cell);
        }
        for (const FNaiwaP7ObstacleRow& Obstacle : Obstacles)
        {
            if (Obstacle.ObstacleId.IsNone() || ObstacleIds.Contains(Obstacle.ObstacleId) ||
                Obstacle.MaxHealth <= 0 || Obstacle.RewardCoins < 0)
            {
                return InvalidData(TEXT("Obstacle rows require unique IDs, positive health, and non-negative rewards."));
            }
            if (!IsCellInBounds(Obstacle.Cell) || RouteCells.Contains(Obstacle.Cell) ||
                BuildCells.Contains(Obstacle.Cell) || ObstacleCells.Contains(Obstacle.Cell))
            {
                return InvalidData(FString::Printf(TEXT("Obstacle %s has an invalid, occupied, or duplicated cell."), *Obstacle.ObstacleId.ToString()));
            }
            ObstacleIds.Add(Obstacle.ObstacleId);
            ObstacleCells.Add(Obstacle.Cell);
        }
        TSet<FName> LevelObstacleIds;
        for (const FName LevelObstacleId : Level.ObstacleIds)
        {
            if (LevelObstacleIds.Contains(LevelObstacleId))
            {
                return InvalidData(FString::Printf(TEXT("Level contains duplicate obstacle ID %s."), *LevelObstacleId.ToString()));
            }
            LevelObstacleIds.Add(LevelObstacleId);
            if (!ObstacleIds.Contains(LevelObstacleId))
            {
                return InvalidData(FString::Printf(TEXT("Level obstacle %s has no obstacle data row."), *LevelObstacleId.ToString()));
            }
        }
        if (LevelObstacleIds.Num() != ObstacleIds.Num())
        {
            return InvalidData(TEXT("Every alpha obstacle data row must be referenced by the level."));
        }

        if (Waves.Num() != 8)
        {
            return InvalidData(TEXT("Alpha requires exactly eight wave rows."));
        }
        TSet<int32> WaveIndices;
        for (const FNaiwaP7WaveRow& Wave : Waves)
        {
            if (Wave.WaveIndex < 1 || Wave.WaveIndex > FSimulation::AlphaWaveCount || WaveIndices.Contains(Wave.WaveIndex) || Wave.Spawns.IsEmpty())
            {
                return InvalidData(TEXT("Wave rows require unique indices 1-8 and at least one spawn group."));
            }
            WaveIndices.Add(Wave.WaveIndex);

            for (const FNaiwaP7WaveSpawnGroup& Spawn : Wave.Spawns)
            {
                if (!EnemyIds.Contains(Spawn.EnemyId) || Spawn.Count <= 0 || Spawn.FirstSpawnDelaySteps < 0 ||
                    Spawn.SpawnIntervalSteps < 0 || (Spawn.Count > 1 && Spawn.SpawnIntervalSteps <= 0))
                {
                    return InvalidData(FString::Printf(TEXT("Wave %d contains an invalid or unresolved spawn group."), Wave.WaveIndex));
                }
            }
        }
        if (WaveIndices.Num() != FSimulation::AlphaWaveCount)
        {
            return InvalidData(TEXT("Wave table must contain every index from 1 through 8."));
        }

        FValidationResult Result;
        Result.bIsValid = true;
        return Result;
    }
}
