#if WITH_DEV_AUTOMATION_TESTS

#include "Core/NaiwaP7Board.h"
#include "Misc/AutomationTest.h"

namespace
{
    NaiwaP7::FLevelDefinition MakeValidLevel()
    {
        using namespace NaiwaP7;

        FLevelDefinition Level;
        Level.LevelId = TEXT("P7_AlphaFixture");
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
    FNaiwaP7BoardMappingTest,
    "Naiwa.P7.Board.MappingAcrossResolutions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP7BoardMappingTest::RunTest(const FString& Parameters)
{
    FIntPoint Cell = FIntPoint::ZeroValue;
    TestTrue(TEXT("1920x580.8 board center maps"),
        NaiwaP7::TryMapBoardPointToCell(FVector2D(960.0, 290.4), FVector2D(1920.0, 580.8), Cell));
    TestEqual(TEXT("1920 board center column"), Cell.X, 10);
    TestEqual(TEXT("1920 board center row"), Cell.Y, 5);

    TestTrue(TEXT("1280x387.2 board center maps"),
        NaiwaP7::TryMapBoardPointToCell(FVector2D(640.0, 193.6), FVector2D(1280.0, 387.2), Cell));
    TestEqual(TEXT("720p board center column"), Cell.X, 10);
    TestEqual(TEXT("720p board center row"), Cell.Y, 5);

    TestFalse(TEXT("Right edge is outside the board"),
        NaiwaP7::TryMapBoardPointToCell(FVector2D(1280.0, 10.0), FVector2D(1280.0, 387.2), Cell));
    TestFalse(TEXT("Invalid board size is rejected"),
        NaiwaP7::TryMapBoardPointToCell(FVector2D(0.0, 0.0), FVector2D(0.0, 387.2), Cell));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNaiwaP7LevelDefinitionValidationTest,
    "Naiwa.P7.Board.LevelDefinitionValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP7LevelDefinitionValidationTest::RunTest(const FString& Parameters)
{
    NaiwaP7::FLevelDefinition Level = MakeValidLevel();
    const NaiwaP7::FValidationResult ValidResult = NaiwaP7::ValidateLevelDefinition(Level);
    TestTrue(*FString::Printf(TEXT("Valid level fixture accepted: %s"), *ValidResult.Error), ValidResult.bIsValid);

    Level.RouteB.Cells[1] = FIntPoint(1, 3);
    const NaiwaP7::FValidationResult DiscontinuousRoute = NaiwaP7::ValidateLevelDefinition(Level);
    TestFalse(TEXT("Discontinuous route rejected"), DiscontinuousRoute.bIsValid);

    Level = MakeValidLevel();
    Level.VisibleBuildCells[0] = Level.RouteA.Cells[3];
    const NaiwaP7::FValidationResult BuildOnRoute = NaiwaP7::ValidateLevelDefinition(Level);
    TestFalse(TEXT("Build site on route rejected"), BuildOnRoute.bIsValid);

    Level = MakeValidLevel();
    Level.HiddenBuildSites[0].UnlockObstacleId = TEXT("MissingObstacle");
    const NaiwaP7::FValidationResult MissingObstacle = NaiwaP7::ValidateLevelDefinition(Level);
    TestFalse(TEXT("Unreferenced unlock obstacle rejected"), MissingObstacle.bIsValid);

    Level = MakeValidLevel();
    Level.InterferenceTriggerCell = FIntPoint(0, 4);
    const NaiwaP7::FValidationResult NonSharedTrigger = NaiwaP7::ValidateLevelDefinition(Level);
    TestFalse(TEXT("Interference trigger must be an interior cell shared by both routes"), NonSharedTrigger.bIsValid);
    return true;
}

#endif
