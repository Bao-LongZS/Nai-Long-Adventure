#if WITH_DEV_AUTOMATION_TESTS

#include "Core/NaiwaP7Simulation.h"
#include "Misc/AutomationTest.h"

namespace
{
    NaiwaP7::FAlphaDataFixture MakeCombatFixture()
    {
        using namespace NaiwaP7;

        FAlphaDataFixture Fixture;
        Fixture.Level.LevelId = TEXT("P7_CombatTest");
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
        Fixture.StartingCoins = 100;

        const ENaiwaP7TowerRole Roles[] = {
            ENaiwaP7TowerRole::DirectDamage,
            ENaiwaP7TowerRole::AreaDamage,
            ENaiwaP7TowerRole::SlowSupport
        };
        for (int32 RoleIndex = 0; RoleIndex < UE_ARRAY_COUNT(Roles); ++RoleIndex)
        {
            for (int32 Level = 1; Level <= 3; ++Level)
            {
                FNaiwaP7TowerRow& Tower = Fixture.Towers.AddDefaulted_GetRef();
                Tower.TowerId = RoleIndex == 0 ? FName(TEXT("Direct")) : (RoleIndex == 1 ? FName(TEXT("Area")) : FName(TEXT("Slow")));
                Tower.Role = Roles[RoleIndex];
                Tower.Level = Level;
                Tower.CostCoins = Level == 1 ? 10 : (Level == 2 ? 20 : 30);
                Tower.Damage = RoleIndex == 2 ? 0 : 10 * Level;
                Tower.AttackIntervalSteps = 1;
                Tower.RangeMilliCells = 10000;
                if (Tower.Role == ENaiwaP7TowerRole::AreaDamage)
                {
                    Tower.SplashRadiusMilliCells = 1000;
                }
                else if (Tower.Role == ENaiwaP7TowerRole::SlowSupport)
                {
                    Tower.SlowPercent = 50;
                    Tower.SlowDurationSteps = 3;
                }
            }
        }

        const ENaiwaP7EnemyKind Kinds[] = {
            ENaiwaP7EnemyKind::Normal,
            ENaiwaP7EnemyKind::Fast,
            ENaiwaP7EnemyKind::Interferer
        };
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Kinds); ++Index)
        {
            FNaiwaP7EnemyRow& Enemy = Fixture.Enemies.AddDefaulted_GetRef();
            Enemy.EnemyId = Index == 0 ? FName(TEXT("Normal")) : (Index == 1 ? FName(TEXT("Fast")) : FName(TEXT("Interferer")));
            Enemy.Kind = Kinds[Index];
            Enemy.MaxHealth = 40;
            Enemy.SpeedMilliCellsPerSecond = 60000;
            if (Enemy.Kind == ENaiwaP7EnemyKind::Interferer)
            {
                Enemy.InterferenceRadiusMilliCells = 2500;
                Enemy.InterferenceDurationSteps = 90;
            }
        }

        for (int32 Index = 0; Index < Fixture.Level.ObstacleIds.Num(); ++Index)
        {
            FNaiwaP7ObstacleRow& Obstacle = Fixture.Obstacles.AddDefaulted_GetRef();
            Obstacle.ObstacleId = Fixture.Level.ObstacleIds[Index];
            Obstacle.Cell = Index == 0 ? FIntPoint(3, 9) : FIntPoint(15, 9);
            Obstacle.MaxHealth = 25;
            Obstacle.RewardCoins = 25;
        }

        for (int32 WaveIndex = 1; WaveIndex <= FSimulation::AlphaWaveCount; ++WaveIndex)
        {
            FNaiwaP7WaveRow& Wave = Fixture.Waves.AddDefaulted_GetRef();
            Wave.WaveIndex = WaveIndex;
            FNaiwaP7WaveSpawnGroup& Spawn = Wave.Spawns.AddDefaulted_GetRef();
            Spawn.EnemyId = TEXT("Normal");
            Spawn.Count = 1;
            Spawn.FirstSpawnDelaySteps = 0;
            Spawn.SpawnIntervalSteps = 0;
        }
        return Fixture;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNaiwaP7BuildUpgradeAndObstacleTest,
    "Naiwa.P7.Combat.BuildUpgradeObstacleUnlock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP7BuildUpgradeAndObstacleTest::RunTest(const FString& Parameters)
{
    NaiwaP7::FAlphaDataFixture Fixture = MakeCombatFixture();
    Fixture.Towers[3].CostCoins = 1000;

    NaiwaP7::FSimulation Simulation;
    FString Error;
    TestTrue(*FString::Printf(TEXT("Fixture loads: %s"), *Error), Simulation.LoadFixture(Fixture, &Error));

    TestFalse(TEXT("Route cell is rejected as a tower site"), Simulation.TryPlaceTower(FIntPoint(2, 5), TEXT("Direct"), &Error));
    TestEqual(TEXT("Rejected placement leaves coins unchanged"), Simulation.GetCoins(), 100);
    TestFalse(TEXT("Unaffordable tower is rejected"), Simulation.TryPlaceTower(FIntPoint(4, 8), TEXT("Area"), &Error));
    TestTrue(TEXT("Visible build site accepts tower"), Simulation.TryPlaceTower(FIntPoint(2, 8), TEXT("Direct"), &Error));
    TestEqual(TEXT("Placement deducts level-one cost"), Simulation.GetCoins(), 90);
    TestFalse(TEXT("Occupied build site rejects a second tower"), Simulation.TryPlaceTower(FIntPoint(2, 8), TEXT("Direct"), &Error));
    TestTrue(TEXT("Tower upgrades to level two"), Simulation.TryUpgradeTower(FIntPoint(2, 8), &Error));
    TestEqual(TEXT("Upgrade deducts level-two cost"), Simulation.GetCoins(), 70);
    TestTrue(TEXT("Tower can be assigned to an in-range obstacle"), Simulation.TrySetTowerObstacleTarget(FIntPoint(2, 8), TEXT("Obstacle_A"), &Error));
    TestEqual(TEXT("Obstacle can be attacked during preparation"), Simulation.AdvanceFixedSteps(2), 2);

    NaiwaP7::FSimulationSnapshot Snapshot = Simulation.MakeSnapshot();
    TestEqual(TEXT("Obstacle clear grants its configured reward"), Snapshot.Coins, 95);
    TestTrue(TEXT("Obstacle state records that it was cleared"), Snapshot.Obstacles[0].bCleared);
    TestTrue(TEXT("Linked hidden build site unlocks"), Snapshot.Obstacles[0].bHiddenBuildSiteUnlocked);
    TestTrue(TEXT("Unlocked hidden build site accepts a tower"), Simulation.TryPlaceTower(FIntPoint(5, 9), TEXT("Direct"), &Error));
    TestEqual(TEXT("Hidden-site placement deducts its cost"), Simulation.GetCoins(), 85);

    TestTrue(TEXT("Wave starts after preparation"), Simulation.TryBeginNextWave(&Error));
    TestEqual(TEXT("Wave data exposes one pending enemy"), Simulation.GetPendingEnemySpawnCount(), static_cast<int64>(1));
    TestTrue(TEXT("Named enemy row spawns"), Simulation.TrySpawnEnemy(1, TEXT("Normal"), &Error));
    TestEqual(TEXT("Direct towers attack the selected enemy"), Simulation.AdvanceFixedSteps(1), 1);
    Snapshot = Simulation.MakeSnapshot();
    TestEqual(TEXT("Scheduled enemy spawns from the wave table"), Snapshot.Enemies.Num(), 2);
    TestEqual(TEXT("Wave table has no remaining scheduled enemy"), Snapshot.PendingEnemySpawns, static_cast<int64>(0));
    TestEqual(TEXT("Enemy health reflects both in-range direct towers"), Snapshot.Enemies[0].Health, 10);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNaiwaP7AreaAndSlowTowerTest,
    "Naiwa.P7.Combat.AreaDamageAndSlowSupport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP7AreaAndSlowTowerTest::RunTest(const FString& Parameters)
{
    NaiwaP7::FSimulation Simulation;
    FString Error;
    TestTrue(TEXT("Combat fixture loads"), Simulation.LoadFixture(MakeCombatFixture(), &Error));
    TestTrue(TEXT("Area tower is placed"), Simulation.TryPlaceTower(FIntPoint(2, 8), TEXT("Area"), &Error));
    TestTrue(TEXT("Slow tower is placed"), Simulation.TryPlaceTower(FIntPoint(4, 8), TEXT("Slow"), &Error));
    TestTrue(TEXT("Wave starts"), Simulation.TryBeginNextWave(&Error));
    TestTrue(TEXT("First named enemy spawns"), Simulation.TrySpawnEnemy(1, TEXT("Normal"), &Error));
    TestTrue(TEXT("Second named enemy spawns"), Simulation.TrySpawnEnemy(2, TEXT("Normal"), &Error));
    TestEqual(TEXT("First combat step advances"), Simulation.AdvanceFixedSteps(1), 1);
    TestEqual(TEXT("Second combat step advances"), Simulation.AdvanceFixedSteps(1), 1);

    const NaiwaP7::FSimulationSnapshot Snapshot = Simulation.MakeSnapshot();
    TestEqual(TEXT("Both manual enemies and the scheduled enemy remain alive"), Snapshot.Enemies.Num(), 3);
    TestEqual(TEXT("Area damage applies to the first enemy"), Snapshot.Enemies[0].Health, 20);
    TestEqual(TEXT("Area damage applies to the nearby second enemy"), Snapshot.Enemies[1].Health, 20);
    TestEqual(TEXT("Slow status reduces one enemy's second-step progress"), Snapshot.Enemies[0].ProgressMilliCells, static_cast<int64>(1500));
    TestEqual(TEXT("Other enemy keeps its normal speed"), Snapshot.Enemies[1].ProgressMilliCells, static_cast<int64>(2000));
    TestEqual(TEXT("Scheduled enemy also receives area damage"), Snapshot.Enemies[2].Health, 20);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNaiwaP7InterferenceTest,
    "Naiwa.P7.Combat.InterferenceStableTargetAndExpiry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP7InterferenceTest::RunTest(const FString& Parameters)
{
    NaiwaP7::FAlphaDataFixture Fixture = MakeCombatFixture();
    Fixture.Level.VisibleBuildCells[0] = FIntPoint(0, 6);
    Fixture.Level.VisibleBuildCells[1] = FIntPoint(2, 6);
    Fixture.Waves[0].Spawns[0].EnemyId = TEXT("Interferer");

    NaiwaP7::FSimulation Simulation;
    FString Error;
    TestTrue(TEXT("Interference fixture loads"), Simulation.LoadFixture(Fixture, &Error));
    TestTrue(TEXT("First tower is placed"), Simulation.TryPlaceTower(FIntPoint(0, 6), TEXT("Direct"), &Error));
    TestTrue(TEXT("Equidistant second tower is placed"), Simulation.TryPlaceTower(FIntPoint(2, 6), TEXT("Direct"), &Error));
    TestTrue(TEXT("Interference wave begins"), Simulation.TryBeginNextWave(&Error));
    TestTrue(TEXT("Second interferer is added for the non-stacking case"), Simulation.TrySpawnEnemy(3, TEXT("Interferer"), &Error));

    TestEqual(TEXT("Trigger step advances"), Simulation.AdvanceFixedSteps(1), 1);
    NaiwaP7::FSimulationSnapshot Snapshot = Simulation.MakeSnapshot();
    TestEqual(TEXT("Stable CellId breaks equal tower-distance ties"), Snapshot.Towers[0].Cell, FIntPoint(0, 6));
    TestEqual(TEXT("Chosen tower receives the full 90-step interference"), Snapshot.Towers[0].InterferenceStepsRemaining, 90);
    TestEqual(TEXT("Other equidistant tower remains available"), Snapshot.Towers[1].InterferenceStepsRemaining, 0);
    TestTrue(TEXT("Each interferer records its one allowed trigger"), Snapshot.Enemies[0].bInterferenceTriggered && Snapshot.Enemies[1].bInterferenceTriggered);
    TestEqual(TEXT("Equal-position target ties resolve to the smaller entity ID"), Snapshot.Enemies[0].Health, 30);
    TestEqual(TEXT("Higher entity ID is not hit on the tie"), Snapshot.Enemies[1].Health, 40);

    Simulation.AdvanceFixedSteps(1);
    Snapshot = Simulation.MakeSnapshot();
    TestEqual(TEXT("A second interferer does not stack or refresh the effect"), Snapshot.Towers[0].InterferenceStepsRemaining, 89);
    TestEqual(TEXT("The selected tower resumes exactly after 90 active steps"), Simulation.AdvanceFixedSteps(88), 88);
    TestEqual(TEXT("One active step remains on the timer"), Simulation.MakeSnapshot().Towers[0].InterferenceStepsRemaining, 1);
    TestEqual(TEXT("Final active step expires interference"), Simulation.AdvanceFixedSteps(1), 1);
    TestEqual(TEXT("Tower attack lock has expired"), Simulation.MakeSnapshot().Towers[0].InterferenceStepsRemaining, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNaiwaP7WaveScheduleTest,
    "Naiwa.P7.Waves.DelayIntervalAndPendingCompletionGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP7WaveScheduleTest::RunTest(const FString& Parameters)
{
    NaiwaP7::FAlphaDataFixture Fixture = MakeCombatFixture();
    Fixture.Waves[0].Spawns[0].Count = 3;
    Fixture.Waves[0].Spawns[0].FirstSpawnDelaySteps = 2;
    Fixture.Waves[0].Spawns[0].SpawnIntervalSteps = 2;

    NaiwaP7::FSimulation Simulation;
    FString Error;
    TestTrue(TEXT("Wave fixture loads"), Simulation.LoadFixture(Fixture, &Error));
    TestTrue(TEXT("Wave begins from preparation"), Simulation.TryBeginNextWave(&Error));
    TestEqual(TEXT("Three scheduled enemies are pending"), Simulation.GetPendingEnemySpawnCount(), static_cast<int64>(3));
    TestFalse(TEXT("Wave cannot complete while spawns are pending"), Simulation.TryCompleteCurrentWave(&Error));

    Simulation.AdvanceFixedSteps(1);
    TestEqual(TEXT("First delay step emits no enemy"), Simulation.GetEnemyCount(), 0);
    Simulation.AdvanceFixedSteps(1);
    TestEqual(TEXT("First enemy appears after its configured delay"), Simulation.GetEnemyCount(), 1);
    TestEqual(TEXT("Two enemies remain scheduled"), Simulation.GetPendingEnemySpawnCount(), static_cast<int64>(2));
    Simulation.AdvanceFixedSteps(1);
    TestEqual(TEXT("Spawn interval preserves a gap"), Simulation.GetEnemyCount(), 1);
    Simulation.AdvanceFixedSteps(1);
    TestEqual(TEXT("Second enemy appears after two interval steps"), Simulation.GetEnemyCount(), 2);
    Simulation.AdvanceFixedSteps(2);
    TestEqual(TEXT("Third enemy appears after another configured interval"), Simulation.GetEnemyCount(), 3);
    TestEqual(TEXT("All scheduled enemies have spawned"), Simulation.GetPendingEnemySpawnCount(), static_cast<int64>(0));
    TestFalse(TEXT("Spawn completion alone does not finish a wave with living enemies"), Simulation.TryCompleteCurrentWave(&Error));
    return true;
}

#endif
