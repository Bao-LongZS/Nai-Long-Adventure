#include "Core/NaiwaP7TowerDefenseSubsystem.h"

void UNaiwaP7TowerDefenseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Simulation.Reset();
    bPaused = false;
}

void UNaiwaP7TowerDefenseSubsystem::Deinitialize()
{
    Simulation.Reset();
    bPaused = false;
    Super::Deinitialize();
}

void UNaiwaP7TowerDefenseSubsystem::Tick(const float DeltaTime)
{
    if (!bPaused)
    {
        Simulation.AdvanceFrame(static_cast<double>(DeltaTime));
    }
}

TStatId UNaiwaP7TowerDefenseSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UNaiwaP7TowerDefenseSubsystem, STATGROUP_Tickables);
}

bool UNaiwaP7TowerDefenseSubsystem::IsTickable() const
{
    return !bPaused && Simulation.CanAdvance();
}

bool UNaiwaP7TowerDefenseSubsystem::LoadLevel(const NaiwaP7::FLevelDefinition& Level, FString* OutError)
{
    const bool bLoaded = Simulation.LoadLevel(Level, OutError);
    if (bLoaded)
    {
        bPaused = false;
    }
    return bLoaded;
}

bool UNaiwaP7TowerDefenseSubsystem::LoadFixture(const NaiwaP7::FAlphaDataFixture& Fixture, FString* OutError)
{
    const bool bLoaded = Simulation.LoadFixture(Fixture, OutError);
    if (bLoaded)
    {
        bPaused = false;
    }
    return bLoaded;
}

bool UNaiwaP7TowerDefenseSubsystem::LoadFromDataTables(
    const UDataTable* LevelTable,
    const UDataTable* TowerTable,
    const UDataTable* EnemyTable,
    const UDataTable* ObstacleTable,
    const UDataTable* WaveTable,
    const FName LevelRowName,
    FString* OutError)
{
    NaiwaP7::FAlphaDataFixture Fixture;
    const NaiwaP7::FValidationResult FixtureResult = NaiwaP7::BuildAlphaDataFixtureFromTables(
        LevelTable, TowerTable, EnemyTable, ObstacleTable, WaveTable, LevelRowName, Fixture);
    if (!FixtureResult.bIsValid)
    {
        if (OutError)
        {
            *OutError = FixtureResult.Error;
        }
        return false;
    }
    return LoadFixture(Fixture, OutError);
}

bool UNaiwaP7TowerDefenseSubsystem::TrySelectRoute(const NaiwaP7::ERoute Route, FString* OutError)
{
    return Simulation.TrySelectRoute(Route, OutError);
}

bool UNaiwaP7TowerDefenseSubsystem::TryBeginNextWave(FString* OutError)
{
    return Simulation.TryBeginNextWave(OutError);
}

bool UNaiwaP7TowerDefenseSubsystem::TrySpawnEnemy(
    const int32 EntityId,
    const int32 SpeedMilliCellsPerSecond,
    FString* OutError)
{
    return Simulation.TrySpawnEnemy(EntityId, SpeedMilliCellsPerSecond, OutError);
}

bool UNaiwaP7TowerDefenseSubsystem::TrySpawnEnemy(const int32 EntityId, const FName EnemyId, FString* OutError)
{
    return Simulation.TrySpawnEnemy(EntityId, EnemyId, OutError);
}

bool UNaiwaP7TowerDefenseSubsystem::TryPlaceTower(const FIntPoint& Cell, const FName TowerId, FString* OutError)
{
    return Simulation.TryPlaceTower(Cell, TowerId, OutError);
}

bool UNaiwaP7TowerDefenseSubsystem::TryUpgradeTower(const FIntPoint& Cell, FString* OutError)
{
    return Simulation.TryUpgradeTower(Cell, OutError);
}

bool UNaiwaP7TowerDefenseSubsystem::TrySetTowerObstacleTarget(
    const FIntPoint& TowerCell,
    const FName ObstacleId,
    FString* OutError)
{
    return Simulation.TrySetTowerObstacleTarget(TowerCell, ObstacleId, OutError);
}

bool UNaiwaP7TowerDefenseSubsystem::TryCompleteCurrentWave(FString* OutError)
{
    return Simulation.TryCompleteCurrentWave(OutError);
}

void UNaiwaP7TowerDefenseSubsystem::SetPaused(const bool bInPaused)
{
    bPaused = bInPaused;
}
