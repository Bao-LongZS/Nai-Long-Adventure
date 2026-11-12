#if WITH_DEV_AUTOMATION_TESTS

#include "Presentation/NaiwaP7GrayboxHUD.h"
#include "Presentation/NaiwaP7GrayboxPlayerController.h"
#include "Core/NaiwaP7Board.h"
#include "InputCoreTypes.h"
#include "InputAction.h"
#include "InputMappingContext.h"
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
        { FVector2D(1920.0f, 1080.0f), FVector2D(76.8f, 249.6f), FVector2D(1766.4f, 580.8f) },
        { FVector2D(1280.0f, 720.0f), FVector2D(51.2f, 166.4f), FVector2D(1177.6f, 387.2f) }
    };

    for (const FViewportCase& Case : Cases)
    {
        FVector2D BoardOrigin = FVector2D::ZeroVector;
        FVector2D BoardSize = FVector2D::ZeroVector;
        TestTrue(TEXT("Reference viewport produces a board rect"),
            ANaiwaP7GrayboxHUD::TryCalculateBoardRect(Case.Viewport, BoardOrigin, BoardSize));
        TestTrue(TEXT("Board origin matches the centered reference layout"),
            BoardOrigin.Equals(Case.ExpectedOrigin, 0.02f));
        TestTrue(TEXT("Board size uses the 92-percent width-safe area"),
            BoardSize.Equals(Case.ExpectedBoardSize, 0.02f));

        const FVector2D CellSize(
            BoardSize.X / NaiwaP7::BoardColumns,
            BoardSize.Y / NaiwaP7::PresentationVisibleRows);
        const FIntPoint ExpectedCells[] = {
            FIntPoint(0, NaiwaP7::PresentationFirstVisibleRow),
            FIntPoint(10, NaiwaP7::PresentationFirstVisibleRow + 3),
            FIntPoint(19, NaiwaP7::PresentationFirstVisibleRow + NaiwaP7::PresentationVisibleRows - 1)
        };
        for (const FIntPoint& ExpectedCell : ExpectedCells)
        {
            const FVector2D ScreenPoint = BoardOrigin + FVector2D(
                (ExpectedCell.X + 0.5f) * CellSize.X,
                (ExpectedCell.Y - NaiwaP7::PresentationFirstVisibleRow + 0.5f) * CellSize.Y);
            FIntPoint MappedCell = FIntPoint(-1, -1);
            TestTrue(TEXT("Visible board point maps to a logical cell"),
                NaiwaP7::TryMapBoardPointToCell(ScreenPoint - BoardOrigin, BoardSize,
                    NaiwaP7::PresentationFirstVisibleRow, NaiwaP7::PresentationVisibleRows, MappedCell));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNaiwaP7RuntimeInputBindingsTest,
    "Naiwa.P7.Presentation.RuntimeInputBindings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaiwaP7RuntimeInputBindingsTest::RunTest(const FString& Parameters)
{
    ANaiwaP7GrayboxPlayerController* Controller = NewObject<ANaiwaP7GrayboxPlayerController>(GetTransientPackage());
    TestNotNull(TEXT("Graybox controller can be created for input mapping inspection"), Controller);
    if (!Controller)
    {
        return false;
    }

    Controller->CreateRuntimeInputContext();
    TestNotNull(TEXT("Runtime mapping context is created"), Controller->RuntimeInputContext.Get());
    if (!Controller->RuntimeInputContext)
    {
        return false;
    }

    struct FExpectedBinding
    {
        UInputAction* Action;
        FKey Key;
        const TCHAR* Label;
    };
    const FExpectedBinding Expected[] = {
        { Controller->PrimaryClickAction, EKeys::LeftMouseButton, TEXT("primary click / left mouse") },
        { Controller->SecondaryClickAction, EKeys::RightMouseButton, TEXT("clear selection / right mouse") },
        { Controller->SelectTowerOneAction, EKeys::One, TEXT("tower slot 1") },
        { Controller->SelectTowerTwoAction, EKeys::Two, TEXT("tower slot 2") },
        { Controller->SelectTowerThreeAction, EKeys::Three, TEXT("tower slot 3") },
        { Controller->UpgradeAction, EKeys::U, TEXT("upgrade") },
        { Controller->ClearTowerTargetAction, EKeys::C, TEXT("clear tower target") },
        { Controller->BeginWaveAction, EKeys::N, TEXT("begin wave") },
        { Controller->TogglePauseAction, EKeys::SpaceBar, TEXT("pause / resume") },
        { Controller->SelectRouteAAction, EKeys::A, TEXT("route A") },
        { Controller->SelectRouteBAction, EKeys::B, TEXT("route B") }
    };

    const TArray<FEnhancedActionKeyMapping>& Mappings = Controller->RuntimeInputContext->GetMappings();
    TestEqual(TEXT("Mapping context contains exactly the supported P7 controls"),
        Mappings.Num(), static_cast<int32>(UE_ARRAY_COUNT(Expected)));
    for (const FExpectedBinding& Binding : Expected)
    {
        TestNotNull(FString::Printf(TEXT("Action exists for %s"), Binding.Label), Binding.Action);
        const bool bHasExpectedMapping = Mappings.ContainsByPredicate(
            [&Binding](const FEnhancedActionKeyMapping& Mapping)
            {
                return Mapping.Action == Binding.Action && Mapping.Key == Binding.Key;
            });
        TestTrue(FString::Printf(TEXT("Expected key mapping exists for %s"), Binding.Label), bHasExpectedMapping);
    }
    return true;
}

#endif
