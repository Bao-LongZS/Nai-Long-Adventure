#pragma once

#include "Core/NaiwaP7Board.h"
#include "Data/NaiwaP7DataRows.h"
#include "Engine/DataTable.h"

namespace NaiwaP7
{
    struct NAIWAP7RUNTIME_API FAlphaDataFixture
    {
        FLevelDefinition Level;
        TArray<FNaiwaP7TowerRow> Towers;
        TArray<FNaiwaP7EnemyRow> Enemies;
        TArray<FNaiwaP7ObstacleRow> Obstacles;
        TArray<FNaiwaP7WaveRow> Waves;
        int32 StartingCoins = 0;
    };

    /** Validates the alpha table contract without supplying production balance values. */
    NAIWAP7RUNTIME_API FValidationResult ValidateAlphaDataFixture(const FAlphaDataFixture& Fixture);

    /** Copies typed Unreal DataTables into a validated immutable-style alpha fixture. */
    NAIWAP7RUNTIME_API FValidationResult BuildAlphaDataFixtureFromTables(
        const UDataTable* LevelTable,
        const UDataTable* TowerTable,
        const UDataTable* EnemyTable,
        const UDataTable* ObstacleTable,
        const UDataTable* WaveTable,
        FName LevelRowName,
        FAlphaDataFixture& OutFixture);

    /** Compatibility overload for validating table arrays and the level in isolation. */
    NAIWAP7RUNTIME_API FValidationResult ValidateAlphaDataFixture(
        const FLevelDefinition& Level,
        const TArray<FNaiwaP7TowerRow>& Towers,
        const TArray<FNaiwaP7EnemyRow>& Enemies,
        const TArray<FNaiwaP7ObstacleRow>& Obstacles,
        const TArray<FNaiwaP7WaveRow>& Waves);
}
