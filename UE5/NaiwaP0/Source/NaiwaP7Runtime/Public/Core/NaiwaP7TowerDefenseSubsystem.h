#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/NaiwaP7Simulation.h"
#include "NaiwaP7TowerDefenseSubsystem.generated.h"

/** P7-only authority boundary. UI and actors read its immutable simulation snapshot. */
UCLASS()
class NAIWAP7RUNTIME_API UNaiwaP7TowerDefenseSubsystem final : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;

    bool LoadLevel(const NaiwaP7::FLevelDefinition& Level, FString* OutError = nullptr);
    bool LoadFixture(const NaiwaP7::FAlphaDataFixture& Fixture, FString* OutError = nullptr);
    bool LoadFromDataTables(
        const UDataTable* LevelTable,
        const UDataTable* TowerTable,
        const UDataTable* EnemyTable,
        const UDataTable* ObstacleTable,
        const UDataTable* WaveTable,
        FName LevelRowName,
        FString* OutError = nullptr);
    bool TrySelectRoute(NaiwaP7::ERoute Route, FString* OutError = nullptr);
    bool TryBeginNextWave(FString* OutError = nullptr);
    bool TrySpawnEnemy(int32 EntityId, int32 SpeedMilliCellsPerSecond, FString* OutError = nullptr);
    bool TrySpawnEnemy(int32 EntityId, FName EnemyId, FString* OutError = nullptr);
    bool TryCompleteCurrentWave(FString* OutError = nullptr);
    bool TryPlaceTower(const FIntPoint& Cell, FName TowerId, FString* OutError = nullptr);
    bool TryUpgradeTower(const FIntPoint& Cell, FString* OutError = nullptr);
    bool TrySetTowerObstacleTarget(const FIntPoint& TowerCell, FName ObstacleId, FString* OutError = nullptr);
    void SetPaused(bool bPaused);

    bool IsPaused() const { return bPaused; }
    NaiwaP7::FSimulationSnapshot GetSnapshot() const { return Simulation.MakeSnapshot(); }
    const NaiwaP7::FSimulation& GetSimulation() const { return Simulation; }

private:
    NaiwaP7::FSimulation Simulation;
    bool bPaused = false;
};
