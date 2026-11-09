#if WITH_DEV_AUTOMATION_TESTS

#include "Presentation/NaiwaP7GrayboxHUD.h"
#include "Core/NaiwaP7Board.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNaiwaP7BoardRectResolutionParityTest,
    "Naiwa.P7.Presentation.BoardRectResolutionParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP7BoardRectResolutionParityTest::RunTest(const FString& Parameters)
{
    struct FViewportCase
    {
        FVector2D Viewport;
        FVector2D ExpectedOrigin;
        FVector2D ExpectedBoardSize;
    };

    const FViewportCase Cases[] = {
        { FVector2D(1920.0f, 1080.0f), FVector2D(0.0f, 249.6f), FVector2D(1920.0f, 580.8f) },
        { FVector2D(1280.0f, 720.0f), FVector2D(0.0f, 166.4f), FVector2D(1280.0f, 387.2f) }
    };

    for (const FViewportCase& Case : Cases)
    {
        FVector2D BoardOrigin = FVector2D::ZeroVector;
        FVector2D BoardSize = FVector2D::ZeroVector;
        TestTrue(TEXT("Reference viewport produces a board rect"),
            ANaiwaP7GrayboxHUD::TryCalculateBoardRect(Case.Viewport, BoardOrigin, BoardSize));
        TestTrue(TEXT("Board origin matches the centered reference layout"),
            BoardOrigin.Equals(Case.ExpectedOrigin, 0.02f));
        TestTrue(TEXT("Board size matches the 96x52.8 cell contract"),
            BoardSize.Equals(Case.ExpectedBoardSize, 0.02f));

        const FVector2D CellSize(
            BoardSize.X / NaiwaP7::BoardColumns,
            BoardSize.Y / NaiwaP7::BoardRows);
        const FIntPoint ExpectedCells[] = {
            FIntPoint(0, 0), FIntPoint(10, 5), FIntPoint(19, 10)
        };
        for (const FIntPoint& ExpectedCell : ExpectedCells)
        {
            const FVector2D ScreenPoint = BoardOrigin + FVector2D(
                (ExpectedCell.X + 0.5f) * CellSize.X,
                (ExpectedCell.Y + 0.5f) * CellSize.Y);
            FIntPoint MappedCell = FIntPoint(-1, -1);
            TestTrue(TEXT("Visible board point maps to a logical cell"),
                NaiwaP7::TryMapBoardPointToCell(ScreenPoint - BoardOrigin, BoardSize, MappedCell));
            TestTrue(TEXT("Equivalent viewport points map to the same CellId"), MappedCell == ExpectedCell);
        }

        TestTrue(TEXT("Board rect remains inside the viewport"),
            BoardOrigin.X >= 0.0f && BoardOrigin.Y >= 0.0f &&
            BoardOrigin.X + BoardSize.X <= Case.Viewport.X + 0.02f &&
            BoardOrigin.Y + BoardSize.Y <= Case.Viewport.Y + 0.02f);
    }

    FVector2D PreservedOrigin(12.0f, 34.0f);
    FVector2D PreservedSize(56.0f, 78.0f);
    TestFalse(TEXT("Zero-width viewport is rejected"), ANaiwaP7GrayboxHUD::TryCalculateBoardRect(
        FVector2D(0.0f, 720.0f), PreservedOrigin, PreservedSize));
    TestTrue(TEXT("Invalid viewport leaves output origin intact"), PreservedOrigin == FVector2D(12.0f, 34.0f));
    TestTrue(TEXT("Invalid viewport leaves output size intact"), PreservedSize == FVector2D(56.0f, 78.0f));
    return true;
}

#endif
