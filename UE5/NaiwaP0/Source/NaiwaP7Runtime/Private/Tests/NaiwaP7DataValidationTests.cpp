#if WITH_DEV_AUTOMATION_TESTS

#include "Data/NaiwaP7DataValidation.h"
#include "Core/NaiwaP7Simulation.h"
#include "Misc/AutomationTest.h"

namespace
{
    struct FAlphaDataTestFixture
    {
        NaiwaP7::FLevelDefinition Level;
        TArray<FNaiwaP7TowerRow> Towers;
        TArray<FNaiwaP7EnemyRow> Enemies;
        TArray<FNaiwaP7ObstacleRow> Obstacles;
        TArray<FNaiwaP7WaveRow> Waves;
    };

    FAlphaDataTestFixture MakeValidAlphaDataFixture()
    {
        using namespace NaiwaP7;

        FAlphaDataTestFixture Fixture;
        Fixture.Level.LevelId = TEXT("P7_DataValidationTest");
        for (int32 Column = 0; Column < BoardColumns; ++Column)
        {
            Fixture.Level.RouteA.Cells.Add(FIntPoint(Column, 5));
        }
        Fixture.Level.RouteB.Cells.Add(FIntPoint(0, 5));
        Fixture.Level.RouteB.Cells.Add(FIntPoint(1, 5));
        Fixture.Level.RouteB.Cells.Add(FIntPoint(1, 4));
        for (int32 Column = 2; Column < BoardColumns; ++Column)
        {
            Fixture.Level.RouteB.Cells.Add(FIntPoint(Column, 4));
        }
        Fixture.Level.RouteB.Cells.Add(FIntPoint(BoardColumns - 1, 5));
        Fixture.Level.InterferenceTriggerCell = FIntPoint(1, 5);
        Fixture.Level.VisibleBuildCells = {
            FIntPoint(2, 8), FIntPoint(4, 8), FIntPoint(6, 8), FIntPoint(8, 8),
            FIntPoint(11, 8), FIntPoint(13, 8), FIntPoint(15, 8), FIntPoint(17, 8)
        };
        Fixture.Level.ObstacleIds = { TEXT("Obstacle_A"), TEXT("Obstacle_B") };
        Fixture.Level.HiddenBuildSites = {
            { FIntPoint(5, 9), TEXT("Obstacle_A") },
            { FIntPoint(14, 9), TEXT("Obstacle_B") }
        };

        const ENaiwaP7TowerRole TowerRoles[] = {
            ENaiwaP7TowerRole::DirectDamage,
            ENaiwaP7TowerRole::AreaDamage,
            ENaiwaP7TowerRole::SlowSupport
        };
        for (int32 RoleIndex = 0; RoleIndex < UE_ARRAY_COUNT(TowerRoles); ++RoleIndex)
        {
            for (int32 LevelIndex = 1; LevelIndex <= 3; ++LevelIndex)
            {
                FNaiwaP7TowerRow& Tower = Fixture.Towers.AddDefaulted_GetRef();
                Tower.TowerId = FName(*FString::Printf(TEXT("Tower_%d"), RoleIndex));
                Tower.Role = TowerRoles[RoleIndex];
                Tower.Level = LevelIndex;
                Tower.CostCoins = 100 * LevelIndex;
                Tower.Damage = 10 * LevelIndex;
                Tower.AttackIntervalSteps = 30;
                Tower.RangeMilliCells = 4000;
                if (Tower.Role == ENaiwaP7TowerRole::AreaDamage)
                {
                    Tower.SplashRadiusMilliCells = 1200;
                }
                else if (Tower.Role == ENaiwaP7TowerRole::SlowSupport)
                {
                    Tower.SlowPercent = 25;
                    Tower.SlowDurationSteps = 90;
                }
            }
        }

        const ENaiwaP7EnemyKind EnemyKinds[] = {
            ENaiwaP7EnemyKind::Normal,
            ENaiwaP7EnemyKind::Fast,
            ENaiwaP7EnemyKind::Interferer
        };
        for (int32 KindIndex = 0; KindIndex < UE_ARRAY_COUNT(EnemyKinds); ++KindIndex)
        {
            FNaiwaP7EnemyRow& Enemy = Fixture.Enemies.AddDefaulted_GetRef();
            Enemy.EnemyId = FName(*FString::Printf(TEXT("Enemy_%d"), KindIndex));
            Enemy.Kind = EnemyKinds[KindIndex];
            Enemy.MaxHealth = 10;
            Enemy.SpeedMilliCellsPerSecond = 60000;
            if (Enemy.Kind == ENaiwaP7EnemyKind::Interferer)
            {
                Enemy.InterferenceRadiusMilliCells = 2500;
                Enemy.InterferenceDurationSteps = 90;
            }
        }

        int32 ObstacleIndex = 0;
        for (const FName ObstacleId : Fixture.Level.ObstacleIds)
        {
            FNaiwaP7ObstacleRow& Obstacle = Fixture.Obstacles.AddDefaulted_GetRef();
            Obstacle.ObstacleId = ObstacleId;
            Obstacle.Cell = ObstacleIndex == 0 ? FIntPoint(3, 9) : FIntPoint(15, 9);
            Obstacle.MaxHealth = 100;
            Obstacle.RewardCoins = 25;
            ++ObstacleIndex;
        }

        for (int32 WaveIndex = 1; WaveIndex <= FSimulation::AlphaWaveCount; ++WaveIndex)
        {
            FNaiwaP7WaveRow& Wave = Fixture.Waves.AddDefaulted_GetRef();
            Wave.WaveIndex = WaveIndex;
            FNaiwaP7WaveSpawnGroup& Spawn = Wave.Spawns.AddDefaulted_GetRef();
            Spawn.EnemyId = TEXT("Enemy_0");
            Spawn.Count = 3;
            Spawn.FirstSpawnDelaySteps = 0;
            Spawn.SpawnIntervalSteps = 30;
        }
        return Fixture;
    }

    template <typename TRow>
    UDataTable* MakeDataTable(const TArray<TRow>& Rows)
    {
        UDataTable* Table = NewObject<UDataTable>();
        Table->RowStruct = TRow::StaticStruct();
        for (int32 Index = 0; Index < Rows.Num(); ++Index)
        {
            Table->AddRow(FName(*FString::Printf(TEXT("Row_%03d"), Index)), Rows[Index]);
        }
        return Table;
    }

    FString CsvCell(const FString& Value)
    {
        FString Escaped = Value;
        Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""));
        return FString::Printf(TEXT("\"%s\""), *Escaped);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNaiwaP7AlphaDataValidationTest,
    "Naiwa.P7.Data.AlphaSchemaValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP7AlphaDataValidationTest::RunTest(const FString& Parameters)
{
    FAlphaDataTestFixture Fixture = MakeValidAlphaDataFixture();
    const NaiwaP7::FValidationResult Valid = NaiwaP7::ValidateAlphaDataFixture(
        Fixture.Level, Fixture.Towers, Fixture.Enemies, Fixture.Obstacles, Fixture.Waves);
    TestTrue(*FString::Printf(TEXT("Valid alpha schema accepted: %s"), *Valid.Error), Valid.bIsValid);

    Fixture.Towers[1].Level = 1;
    const NaiwaP7::FValidationResult DuplicateTowerLevel = NaiwaP7::ValidateAlphaDataFixture(
        Fixture.Level, Fixture.Towers, Fixture.Enemies, Fixture.Obstacles, Fixture.Waves);
    TestFalse(TEXT("Duplicate tower tier is rejected"), DuplicateTowerLevel.bIsValid);

    Fixture = MakeValidAlphaDataFixture();
    Fixture.Enemies[2].InterferenceDurationSteps = 89;
    const NaiwaP7::FValidationResult WrongInterferenceDuration = NaiwaP7::ValidateAlphaDataFixture(
        Fixture.Level, Fixture.Towers, Fixture.Enemies, Fixture.Obstacles, Fixture.Waves);
    TestFalse(TEXT("Interference duration must be 90 fixed steps"), WrongInterferenceDuration.bIsValid);

    Fixture = MakeValidAlphaDataFixture();
    Fixture.Waves[0].Spawns[0].EnemyId = TEXT("MissingEnemy");
    const NaiwaP7::FValidationResult UnresolvedEnemy = NaiwaP7::ValidateAlphaDataFixture(
        Fixture.Level, Fixture.Towers, Fixture.Enemies, Fixture.Obstacles, Fixture.Waves);
    TestFalse(TEXT("Wave spawn enemy references must resolve"), UnresolvedEnemy.bIsValid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNaiwaP7DataTableAssemblyTest,
    "Naiwa.P7.Data.DataTableFixtureAssembly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP7DataTableAssemblyTest::RunTest(const FString& Parameters)
{
    const FAlphaDataTestFixture Source = MakeValidAlphaDataFixture();
    FNaiwaP7LevelRow LevelRow;
    LevelRow.LevelId = Source.Level.LevelId;
    LevelRow.RouteACells = Source.Level.RouteA.Cells;
    LevelRow.RouteBCells = Source.Level.RouteB.Cells;
    LevelRow.InterferenceTriggerCell = Source.Level.InterferenceTriggerCell;
    LevelRow.VisibleBuildCells = Source.Level.VisibleBuildCells;
    LevelRow.ObstacleIds = Source.Level.ObstacleIds;
    LevelRow.StartingCoins = 100;
    for (const NaiwaP7::FHiddenBuildSite& Site : Source.Level.HiddenBuildSites)
    {
        FNaiwaP7HiddenBuildSiteRow& TableSite = LevelRow.HiddenBuildSites.AddDefaulted_GetRef();
        TableSite.Cell = Site.Cell;
        TableSite.UnlockObstacleId = Site.UnlockObstacleId;
    }

    UDataTable* LevelTable = NewObject<UDataTable>();
    LevelTable->RowStruct = FNaiwaP7LevelRow::StaticStruct();
    LevelTable->AddRow(LevelRow.LevelId, LevelRow);
    UDataTable* TowerTable = MakeDataTable(Source.Towers);
    UDataTable* EnemyTable = MakeDataTable(Source.Enemies);
    UDataTable* ObstacleTable = MakeDataTable(Source.Obstacles);
    UDataTable* WaveTable = MakeDataTable(Source.Waves);

    NaiwaP7::FAlphaDataFixture LoadedFixture;
    const NaiwaP7::FValidationResult Loaded = NaiwaP7::BuildAlphaDataFixtureFromTables(
        LevelTable, TowerTable, EnemyTable, ObstacleTable, WaveTable, Source.Level.LevelId, LoadedFixture);
    TestTrue(*FString::Printf(TEXT("Typed tables assemble into a valid fixture: %s"), *Loaded.Error), Loaded.bIsValid);
    TestEqual(TEXT("Level ID is preserved"), LoadedFixture.Level.LevelId, Source.Level.LevelId);
    TestEqual(TEXT("Level starting coins are loaded"), LoadedFixture.StartingCoins, 100);
    TestEqual(TEXT("Tower rows are loaded"), LoadedFixture.Towers.Num(), 9);
    TestEqual(TEXT("Wave rows are loaded"), LoadedFixture.Waves.Num(), 8);

    NaiwaP7::FAlphaDataFixture Sentinel;
    Sentinel.Level.LevelId = TEXT("KeepOnFailure");
    const NaiwaP7::FValidationResult WrongRowType = NaiwaP7::BuildAlphaDataFixtureFromTables(
        LevelTable, LevelTable, EnemyTable, ObstacleTable, WaveTable, Source.Level.LevelId, Sentinel);
    TestFalse(TEXT("Table row-struct mismatch is rejected"), WrongRowType.bIsValid);
    TestEqual(TEXT("Failed assembly does not partially overwrite its output"), Sentinel.Level.LevelId, FName(TEXT("KeepOnFailure")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNaiwaP7AlphaFixtureAssetAssemblyTest,
    "Naiwa.P7.Data.AlphaFixtureAssetAssembly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP7AlphaFixtureAssetAssemblyTest::RunTest(const FString& Parameters)
{
    const FString ContentDirectory = FPaths::ProjectContentDir() / TEXT("Naiwa/P7/Data");
    const TCHAR* PackageNames[] = {
        TEXT("DT_P7_Alpha01_Level"),
        TEXT("DT_P7_Alpha01_Towers"),
        TEXT("DT_P7_Alpha01_Enemies"),
        TEXT("DT_P7_Alpha01_Obstacles"),
        TEXT("DT_P7_Alpha01_Waves")
    };
    bool bAnyPackageExists = false;
    for (const TCHAR* PackageName : PackageNames)
    {
        bAnyPackageExists |= FPaths::FileExists(ContentDirectory / FString(PackageName) + TEXT(".uasset"));
    }
    if (!bAnyPackageExists)
    {
        AddInfo(TEXT("Production Alpha DataTables are not present in this content-free P7 probe; asset assembly check was skipped."));
        return true;
    }

    TArray<UDataTable*> Tables;
    Tables.Reserve(UE_ARRAY_COUNT(PackageNames));
    bool bAllPackagesExist = true;
    for (const TCHAR* PackageName : PackageNames)
    {
        const FString ObjectPath = FString::Printf(TEXT("/Game/Naiwa/P7/Data/%s.%s"), PackageName, PackageName);
        UDataTable* Table = LoadObject<UDataTable>(nullptr, *ObjectPath);
        bAllPackagesExist &= TestNotNull(*FString::Printf(TEXT("Production table is loadable: %s"), PackageName), Table);
        Tables.Add(Table);
    }
    if (!bAllPackagesExist)
    {
        AddError(TEXT("The Alpha fixture is incomplete; all five DataTables must ship together."));
        return false;
    }

    const FName LevelRowName(TEXT("P7_Alpha01"));
    NaiwaP7::FAlphaDataFixture Fixture;
    const NaiwaP7::FValidationResult Result = NaiwaP7::BuildAlphaDataFixtureFromTables(
        Tables[0], Tables[1], Tables[2], Tables[3], Tables[4], LevelRowName, Fixture);
    if (!TestTrue(*FString::Printf(TEXT("Imported Alpha tables assemble and validate: %s"), *Result.Error), Result.bIsValid))
    {
        return false;
    }

    TestEqual(TEXT("Production fixture level identity"), Fixture.Level.LevelId, LevelRowName);
    TestEqual(TEXT("Production fixture starting coins"), Fixture.StartingCoins, 350);
    TestEqual(TEXT("Production fixture visible build sites"), Fixture.Level.VisibleBuildCells.Num(), 8);
    TestEqual(TEXT("Production fixture hidden build sites"), Fixture.Level.HiddenBuildSites.Num(), 2);
    TestEqual(TEXT("Production fixture Tower rows"), Fixture.Towers.Num(), 9);
    TestEqual(TEXT("Production fixture Enemy rows"), Fixture.Enemies.Num(), 3);
    TestEqual(TEXT("Production fixture Obstacle rows"), Fixture.Obstacles.Num(), 2);
    TestEqual(TEXT("Production fixture Wave rows"), Fixture.Waves.Num(), 8);
    TestEqual(TEXT("Route A contains 20 cells"), Fixture.Level.RouteA.Cells.Num(), 20);
    TestEqual(TEXT("Route B contains 22 cells"), Fixture.Level.RouteB.Cells.Num(), 22);

    NaiwaP7::FSimulation Simulation;
    TestTrue(TEXT("Production fixture loads into the authoritative simulator"), Simulation.LoadFixture(Fixture));
    TestTrue(TEXT("A tower can be purchased from the production tower rows"),
        Simulation.TryPlaceTower(FIntPoint(2, 8), TEXT("P7_Tower_MilkChicken")));
    TestEqual(TEXT("Tower purchase uses the configured level-one cost"), Simulation.GetCoins(), 250);
    TestTrue(TEXT("Production wave one starts"), Simulation.TryBeginNextWave());
    TestEqual(TEXT("Wave one queues four enemies"), Simulation.GetPendingEnemySpawnCount(), static_cast<int64>(4));
    TestEqual(TEXT("One fixed step spawns from production Wave row"), Simulation.AdvanceFixedSteps(1), 1);
    const NaiwaP7::FSimulationSnapshot LiveSnapshot = Simulation.MakeSnapshot();
    TestEqual(TEXT("Spawned enemy resolves through the production Enemy table"), LiveSnapshot.Enemies.Num(), 1);
    if (LiveSnapshot.Enemies.Num() == 1)
    {
        TestEqual(TEXT("Production Wave references the expected normal enemy"),
            LiveSnapshot.Enemies[0].EnemyId, FName(TEXT("P7_Enemy_Normal")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNaiwaP7DataTableCsvArrayImportTest,
    "Naiwa.P7.Data.CsvArrayImport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP7DataTableCsvArrayImportTest::RunTest(const FString& Parameters)
{
    const FString Csv = FString::Printf(
        TEXT("---,LevelId,RouteACells,RouteBCells,InterferenceTriggerCell,VisibleBuildCells,ObstacleIds,HiddenBuildSites,StartingCoins\n%s\n"),
        *FString::Join(TArray<FString>{
            TEXT("P7CsvArrayProbe"),
            CsvCell(TEXT("P7CsvArrayProbe")),
            CsvCell(TEXT("((X=0,Y=5),(X=1,Y=5),(X=2,Y=5))")),
            CsvCell(TEXT("((X=0,Y=6),(X=1,Y=6),(X=2,Y=6))")),
            CsvCell(TEXT("(X=1,Y=5)")),
            CsvCell(TEXT("((X=3,Y=8),(X=4,Y=8))")),
            CsvCell(TEXT("(Obstacle_A,Obstacle_B)")),
            CsvCell(TEXT("((Cell=(X=5,Y=9),UnlockObstacleId=Obstacle_A),(Cell=(X=14,Y=9),UnlockObstacleId=Obstacle_B))")),
            CsvCell(TEXT("100"))
        }, TEXT(",")));

    UDataTable* LevelTable = NewObject<UDataTable>();
    LevelTable->RowStruct = FNaiwaP7LevelRow::StaticStruct();
    const TArray<FString> ImportProblems = LevelTable->CreateTableFromCSVString(Csv);
    TestEqual(TEXT("Level array CSV imports without parser errors"), ImportProblems.Num(), 0);

    const FNaiwaP7LevelRow* Level = LevelTable->FindRow<FNaiwaP7LevelRow>(
        TEXT("P7CsvArrayProbe"), TEXT("P7 CSV array automation test"), false);
    if (!TestNotNull(TEXT("Level row is created"), Level))
    {
        return false;
    }

    TestEqual(TEXT("FIntPoint array cell count"), Level->RouteACells.Num(), 3);
    if (Level->RouteACells.Num() == 3)
    {
        TestEqual(TEXT("FIntPoint array element round-trips"), Level->RouteACells[1], FIntPoint(1, 5));
    }
    TestEqual(TEXT("Visible build cell array count"), Level->VisibleBuildCells.Num(), 2);
    TestEqual(TEXT("FName array element round-trips"), Level->ObstacleIds.Num(), 2);
    if (Level->ObstacleIds.Num() == 2)
    {
        TestEqual(TEXT("FName array value round-trips"), Level->ObstacleIds[1], FName(TEXT("Obstacle_B")));
    }
    TestEqual(TEXT("Nested struct array count"), Level->HiddenBuildSites.Num(), 2);
    if (Level->HiddenBuildSites.Num() == 2)
    {
        TestEqual(TEXT("Nested FIntPoint value round-trips"), Level->HiddenBuildSites[1].Cell, FIntPoint(14, 9));
        TestEqual(TEXT("Nested FName value round-trips"), Level->HiddenBuildSites[1].UnlockObstacleId, FName(TEXT("Obstacle_B")));
    }

    const FString WaveCsv = FString::Printf(
        TEXT("---,WaveIndex,Spawns\n%s\n"),
        *FString::Join(TArray<FString>{
            TEXT("WaveCsvProbe"),
            CsvCell(TEXT("1")),
            CsvCell(TEXT("((EnemyId=Enemy_Normal,Count=2,FirstSpawnDelaySteps=0,SpawnIntervalSteps=30),(EnemyId=Enemy_Fast,Count=1,FirstSpawnDelaySteps=60,SpawnIntervalSteps=0))"))
        }, TEXT(",")));

    UDataTable* WaveTable = NewObject<UDataTable>();
    WaveTable->RowStruct = FNaiwaP7WaveRow::StaticStruct();
    const TArray<FString> WaveImportProblems = WaveTable->CreateTableFromCSVString(WaveCsv);
    TestEqual(TEXT("Nested wave spawn CSV imports without parser errors"), WaveImportProblems.Num(), 0);
    const FNaiwaP7WaveRow* Wave = WaveTable->FindRow<FNaiwaP7WaveRow>(
        TEXT("WaveCsvProbe"), TEXT("P7 CSV wave array automation test"), false);
    if (!TestNotNull(TEXT("Wave row is created"), Wave))
    {
        return false;
    }
    TestEqual(TEXT("Wave spawn group count round-trips"), Wave->Spawns.Num(), 2);
    if (Wave->Spawns.Num() == 2)
    {
        TestEqual(TEXT("Wave spawn enemy ID round-trips"), Wave->Spawns[1].EnemyId, FName(TEXT("Enemy_Fast")));
        TestEqual(TEXT("Wave spawn interval round-trips"), Wave->Spawns[1].SpawnIntervalSteps, 0);
    }
    return true;
}

#endif
