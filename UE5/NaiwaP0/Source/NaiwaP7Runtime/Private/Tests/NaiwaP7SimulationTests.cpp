#if WITH_DEV_AUTOMATION_TESTS

#include "Core/NaiwaP7Simulation.h"
#include "Misc/AutomationTest.h"

namespace
{
    NaiwaP7::FLevelDefinition MakeSimulationTestLevel()
    {
        using namespace NaiwaP7;

        FLevelDefinition Level;
        Level.LevelId = TEXT("P7_SimulationTest");
        for (int32 Column = 0; Column < BoardColumns; ++Column)
        {
            Level.RouteA.Cells.Add(FIntPoint(Column, 5));
        }
        Level.RouteB.Cells.Add(FIntPoint(0, 5));
        Level.RouteB.Cells.Add(FIntPoint(1, 5));
        Level.RouteB.Cells.Add(FIntPoint(1, 4));
        for (int32 Column = 2; Column < BoardColumns; ++Column)
        {
            Level.RouteB.Cells.Add(FIntPoint(Column, 4));
        }
        Level.RouteB.Cells.Add(FIntPoint(BoardColumns - 1, 5));
        Level.InterferenceTriggerCell = FIntPoint(1, 5);
        Level.VisibleBuildCells = {
            FIntPoint(2, 8), FIntPoint(4, 8), FIntPoint(6, 8), FIntPoint(8, 8),
            FIntPoint(11, 8), FIntPoint(13, 8), FIntPoint(15, 8), FIntPoint(17, 8)
        };
        Level.ObstacleIds = { TEXT("Obstacle_A"), TEXT("Obstacle_B") };
        Level.HiddenBuildSites = {
            { FIntPoint(5, 9), TEXT("Obstacle_A") },
            { FIntPoint(14, 9), TEXT("Obstacle_B") }
        };
        return Level;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNaiwaP7SimulationRouteAndReplayTest,
    "Naiwa.P7.Simulation.RouteMovementAndReplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP7SimulationRouteAndReplayTest::RunTest(const FString& Parameters)
{
    const NaiwaP7::FLevelDefinition Level = MakeSimulationTestLevel();
    NaiwaP7::FSimulation First;
    NaiwaP7::FSimulation Replay;
    TestTrue(TEXT("First simulation loads level"), First.LoadLevel(Level));
    TestTrue(TEXT("Replay loads level"), Replay.LoadLevel(Level));

    NaiwaP7::FLevelDefinition AlternateLevel = Level;
    AlternateLevel.RouteA.Cells.Reset();
    AlternateLevel.RouteA.Cells.Add(FIntPoint(0, 5));
    AlternateLevel.RouteA.Cells.Add(FIntPoint(1, 5));
    AlternateLevel.RouteA.Cells.Add(FIntPoint(1, 6));
    for (int32 Column = 2; Column < NaiwaP7::BoardColumns; ++Column)
    {
        AlternateLevel.RouteA.Cells.Add(FIntPoint(Column, 6));
    }
    AlternateLevel.RouteA.Cells.Add(FIntPoint(NaiwaP7::BoardColumns - 1, 5));
    NaiwaP7::FSimulation DifferentFixture;
    TestTrue(TEXT("Alternate route fixture is still valid"), DifferentFixture.LoadLevel(AlternateLevel));
    TestNotEqual(TEXT("State hash identifies different route geometry"), First.ComputeStateHash(), DifferentFixture.ComputeStateHash());

    TestTrue(TEXT("Route B can be selected before a wave"), First.TrySelectRoute(NaiwaP7::ERoute::B));
    TestTrue(TEXT("Replay selects same route"), Replay.TrySelectRoute(NaiwaP7::ERoute::B));
    TestTrue(TEXT("First wave starts"), First.TryBeginNextWave());
    TestTrue(TEXT("Replay starts same wave"), Replay.TryBeginNextWave());
    TestTrue(TEXT("Enemy spawns"), First.TrySpawnEnemy(17, 60000));
    TestTrue(TEXT("Replay enemy spawns"), Replay.TrySpawnEnemy(17, 60000));
    TestFalse(TEXT("Route cannot change during a wave"), First.TrySelectRoute(NaiwaP7::ERoute::A));
    TestFalse(TEXT("Duplicate active EntityId is rejected"), First.TrySpawnEnemy(17, 60000));
    TestFalse(TEXT("Wave cannot complete while an enemy remains"), First.TryCompleteCurrentWave());

    for (int32 Step = 0; Step < 20; ++Step)
    {
        TestEqual(TEXT("First simulation advances one step"), First.AdvanceFixedSteps(1), 1);
        TestEqual(TEXT("Replay advances one step"), Replay.AdvanceFixedSteps(1), 1);
        TestEqual(TEXT("Replay state hash matches after every step"), First.ComputeStateHash(), Replay.ComputeStateHash());
    }

    NaiwaP7::FSimulationSnapshot Snapshot = First.MakeSnapshot();
    TestEqual(TEXT("Longer route has not leaked at step 20"), Snapshot.MilkJarHealth, 10);
    TestEqual(TEXT("Enemy remains on the longer route"), Snapshot.Enemies.Num(), 1);
    TestEqual(TEXT("Captured route is B"), static_cast<uint8>(Snapshot.Enemies[0].Route), static_cast<uint8>(NaiwaP7::ERoute::B));
    TestEqual(TEXT("Enemy has reached the last bend before the endpoint"), Snapshot.Enemies[0].GridPosition, FVector2D(19.0, 4.0));

    TestEqual(TEXT("Final route step advances"), First.AdvanceFixedSteps(1), 1);
    TestEqual(TEXT("Replay final route step advances"), Replay.AdvanceFixedSteps(1), 1);
    Snapshot = First.MakeSnapshot();
    TestEqual(TEXT("Leaked enemy removes one jar health"), Snapshot.MilkJarHealth, 9);
    TestEqual(TEXT("Leaked enemy leaves the active set"), Snapshot.Enemies.Num(), 0);
    TestEqual(TEXT("Replay hash remains identical after leak"), First.ComputeStateHash(), Replay.ComputeStateHash());
    TestEqual(TEXT("An empty schedule auto-completes after its last enemy leaks"),
        static_cast<uint8>(First.GetPhase()), static_cast<uint8>(NaiwaP7::ESimulationPhase::BetweenWaves));
    TestEqual(TEXT("Replay also auto-completes the same wave"), First.ComputeStateHash(), Replay.ComputeStateHash());

    for (int32 Wave = 2; Wave <= NaiwaP7::FSimulation::AlphaWaveCount; ++Wave)
    {
        TestTrue(TEXT("Next wave starts from preparation"), First.TryBeginNextWave());
        TestTrue(TEXT("Replay starts same next wave"), Replay.TryBeginNextWave());
        TestEqual(TEXT("Empty wave advances one simulation step"), First.AdvanceFixedSteps(1), 1);
        TestEqual(TEXT("Replay advances the same empty wave"), Replay.AdvanceFixedSteps(1), 1);
        const NaiwaP7::ESimulationPhase ExpectedPhase = Wave == NaiwaP7::FSimulation::AlphaWaveCount
            ? NaiwaP7::ESimulationPhase::Won
            : NaiwaP7::ESimulationPhase::BetweenWaves;
        TestEqual(TEXT("Empty wave automatically advances or wins"),
            static_cast<uint8>(First.GetPhase()), static_cast<uint8>(ExpectedPhase));
        TestEqual(TEXT("Replay phase remains deterministic"), First.ComputeStateHash(), Replay.ComputeStateHash());
    }
    TestEqual(TEXT("Eight completed waves win the level"), static_cast<uint8>(First.GetPhase()), static_cast<uint8>(NaiwaP7::ESimulationPhase::Won));
    TestFalse(TEXT("Leaked enemy means the jar is not full"), First.MakeSnapshot().bMilkJarFull);
    TestEqual(TEXT("Final replay hash matches"), First.ComputeStateHash(), Replay.ComputeStateHash());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNaiwaP7SimulationCatchUpTest,
    "Naiwa.P7.Simulation.CatchUpBacklogIsRetained",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP7SimulationCatchUpTest::RunTest(const FString& Parameters)
{
    NaiwaP7::FSimulation Simulation;
    TestTrue(TEXT("Level loads"), Simulation.LoadLevel(MakeSimulationTestLevel()));
    TestTrue(TEXT("Wave starts"), Simulation.TryBeginNextWave());

    TestEqual(TEXT("Only four catch-up steps run in one frame"), Simulation.AdvanceFrame(10.0 / 60.0), 4);
    TestEqual(TEXT("Six deferred steps are retained"), Simulation.GetDeferredCatchUpSteps(), 6);
    TestEqual(TEXT("Four retained steps run on a zero-delta frame"), Simulation.AdvanceFrame(0.0), 4);
    TestEqual(TEXT("Two deferred steps remain"), Simulation.GetDeferredCatchUpSteps(), 2);
    TestEqual(TEXT("Final deferred steps run without new frame time"), Simulation.AdvanceFrame(0.0), 2);
    TestEqual(TEXT("No simulation steps were silently discarded"), Simulation.GetDeferredCatchUpSteps(), 0);
    TestEqual(TEXT("All ten expected logical steps ran"), Simulation.GetSimulationStep(), static_cast<uint64>(10));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNaiwaP7SimulationJarLossTest,
    "Naiwa.P7.Simulation.JarReachesZeroOnLeak",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP7SimulationJarLossTest::RunTest(const FString& Parameters)
{
    NaiwaP7::FSimulation Simulation;
    TestTrue(TEXT("Level loads"), Simulation.LoadLevel(MakeSimulationTestLevel()));
    TestTrue(TEXT("Wave starts"), Simulation.TryBeginNextWave());
    for (int32 EntityId = 10; EntityId >= 1; --EntityId)
    {
        TestTrue(TEXT("Enemy spawn accepted"), Simulation.TrySpawnEnemy(EntityId, 60000));
    }

    TestEqual(TEXT("All ten enemies reach the endpoint on the final route step"), Simulation.AdvanceFixedSteps(19), 19);
    const NaiwaP7::FSimulationSnapshot Snapshot = Simulation.MakeSnapshot();
    TestEqual(TEXT("Milk jar health clamps at zero"), Snapshot.MilkJarHealth, 0);
    TestEqual(TEXT("Zero health ends the run as a loss"), static_cast<uint8>(Snapshot.Phase), static_cast<uint8>(NaiwaP7::ESimulationPhase::Lost));
    TestEqual(TEXT("Leaked enemies are removed"), Snapshot.Enemies.Num(), 0);
    TestFalse(TEXT("Lost run cannot complete its wave"), Simulation.TryCompleteCurrentWave());
    return true;
}

#endif
