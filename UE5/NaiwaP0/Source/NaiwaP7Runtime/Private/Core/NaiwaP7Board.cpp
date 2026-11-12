#include "Core/NaiwaP7Board.h"

namespace NaiwaP7
{
    namespace
    {
        FValidationResult Invalid(const FString& Message)
        {
            FValidationResult Result;
            Result.Error = Message;
            return Result;
        }

        bool ValidateRoute(const FRoutePath& Route, const TCHAR* RouteName, FString& OutError)
        {
            if (Route.Cells.Num() < 2)
            {
                OutError = FString::Printf(TEXT("%s route must contain at least two cells."), RouteName);
                return false;
            }

            TSet<FIntPoint> SeenCells;
            for (int32 Index = 0; Index < Route.Cells.Num(); ++Index)
            {
                const FIntPoint& Cell = Route.Cells[Index];
                if (!IsCellInBounds(Cell))
                {
                    OutError = FString::Printf(TEXT("%s route cell %d is outside the 20x11 board."), RouteName, Index);
                    return false;
                }

                if (SeenCells.Contains(Cell))
                {
                    OutError = FString::Printf(TEXT("%s route repeats cell (%d,%d)."), RouteName, Cell.X, Cell.Y);
                    return false;
                }
                SeenCells.Add(Cell);

                if (Index > 0)
                {
                    const FIntPoint& Previous = Route.Cells[Index - 1];
                    const int32 ManhattanDistance = FMath::Abs(Cell.X - Previous.X) + FMath::Abs(Cell.Y - Previous.Y);
                    if (ManhattanDistance != 1)
                    {
                        OutError = FString::Printf(TEXT("%s route is discontinuous between cells %d and %d."),
                            RouteName, Index - 1, Index);
                        return false;
                    }
                }
            }
            return true;
        }
    }

    bool IsCellInBounds(const FIntPoint& Cell)
    {
        return Cell.X >= 0 && Cell.X < BoardColumns && Cell.Y >= 0 && Cell.Y < BoardRows;
    }

    bool TryMapBoardPointToCell(const FVector2D& BoardLocalPoint, const FVector2D& BoardSize, FIntPoint& OutCell)
    {
        if (!FMath::IsFinite(BoardLocalPoint.X) || !FMath::IsFinite(BoardLocalPoint.Y) ||
            !FMath::IsFinite(BoardSize.X) || !FMath::IsFinite(BoardSize.Y) ||
            BoardSize.X <= 0.0 || BoardSize.Y <= 0.0 ||
            BoardLocalPoint.X < 0.0 || BoardLocalPoint.Y < 0.0 ||
            BoardLocalPoint.X >= BoardSize.X || BoardLocalPoint.Y >= BoardSize.Y)
        {
            return false;
        }

        const int32 Column = FMath::FloorToInt(BoardLocalPoint.X * BoardColumns / BoardSize.X);
        const int32 Row = FMath::FloorToInt(BoardLocalPoint.Y * BoardRows / BoardSize.Y);
        const FIntPoint MappedCell(Column, Row);
        if (!IsCellInBounds(MappedCell))
        {
            return false;
        }

        OutCell = MappedCell;
        return true;
    }

    bool TryMapBoardPointToCell(
        const FVector2D& BoardLocalPoint,
        const FVector2D& BoardSize,
        const int32 FirstVisibleRow,
        const int32 VisibleRows,
        FIntPoint& OutCell)
    {
        if (FirstVisibleRow < 0 || VisibleRows <= 0 ||
            FirstVisibleRow + VisibleRows > BoardRows ||
            !FMath::IsFinite(BoardLocalPoint.X) || !FMath::IsFinite(BoardLocalPoint.Y) ||
            !FMath::IsFinite(BoardSize.X) || !FMath::IsFinite(BoardSize.Y) ||
            BoardSize.X <= 0.0 || BoardSize.Y <= 0.0 ||
            BoardLocalPoint.X < 0.0 || BoardLocalPoint.Y < 0.0 ||
            BoardLocalPoint.X >= BoardSize.X || BoardLocalPoint.Y >= BoardSize.Y)
        {
            return false;
        }

        const int32 Column = FMath::FloorToInt(BoardLocalPoint.X * BoardColumns / BoardSize.X);
        const int32 Row = FirstVisibleRow +
            FMath::FloorToInt(BoardLocalPoint.Y * VisibleRows / BoardSize.Y);
        const FIntPoint MappedCell(Column, Row);
        if (!IsCellInBounds(MappedCell) || Row >= FirstVisibleRow + VisibleRows)
        {
            return false;
        }

        OutCell = MappedCell;
        return true;
    }

    FValidationResult ValidateLevelDefinition(const FLevelDefinition& Level)
    {
        if (Level.LevelId.IsNone())
        {
            return Invalid(TEXT("LevelId must be set."));
        }

        FString RouteError;
        if (!ValidateRoute(Level.RouteA, TEXT("A"), RouteError))
        {
            return Invalid(RouteError);
        }
        if (!ValidateRoute(Level.RouteB, TEXT("B"), RouteError))
        {
            return Invalid(RouteError);
        }
        if (Level.RouteA.Cells[0] != Level.RouteB.Cells[0] ||
            Level.RouteA.Cells.Last() != Level.RouteB.Cells.Last())
        {
            return Invalid(TEXT("Route A and B must share the same start and end cells."));
        }
        if (Level.RouteA.Cells == Level.RouteB.Cells)
        {
            return Invalid(TEXT("Route A and B must provide distinct preset choices."));
        }
        const int32 TriggerIndexA = Level.RouteA.Cells.Find(Level.InterferenceTriggerCell);
        const int32 TriggerIndexB = Level.RouteB.Cells.Find(Level.InterferenceTriggerCell);
        if (TriggerIndexA <= 0 || TriggerIndexA >= Level.RouteA.Cells.Num() - 1 ||
            TriggerIndexB <= 0 || TriggerIndexB >= Level.RouteB.Cells.Num() - 1)
        {
            return Invalid(TEXT("The interference trigger must be a shared interior cell on routes A and B."));
        }

        if (Level.VisibleBuildCells.Num() != VisibleBuildSiteCount)
        {
            return Invalid(FString::Printf(TEXT("Alpha requires exactly %d visible build sites."), VisibleBuildSiteCount));
        }
        if (Level.ObstacleIds.Num() != HiddenBuildSiteCount ||
            Level.HiddenBuildSites.Num() != HiddenBuildSiteCount)
        {
            return Invalid(FString::Printf(TEXT("Alpha requires exactly %d obstacles and hidden build sites."), HiddenBuildSiteCount));
        }

        TSet<FName> UniqueObstacleIds;
        for (const FName ObstacleId : Level.ObstacleIds)
        {
            if (ObstacleId.IsNone() || UniqueObstacleIds.Contains(ObstacleId))
            {
                return Invalid(TEXT("Obstacle IDs must be non-empty and unique."));
            }
            UniqueObstacleIds.Add(ObstacleId);
        }

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
        const auto AddBuildCell = [&RouteCells, &BuildCells](const FIntPoint& Cell, const TCHAR* SiteKind, FString& OutError)
        {
            if (!IsCellInBounds(Cell))
            {
                OutError = FString::Printf(TEXT("%s build site (%d,%d) is outside the 20x11 board."), SiteKind, Cell.X, Cell.Y);
                return false;
            }
            if (RouteCells.Contains(Cell))
            {
                OutError = FString::Printf(TEXT("%s build site (%d,%d) overlaps a route."), SiteKind, Cell.X, Cell.Y);
                return false;
            }
            if (BuildCells.Contains(Cell))
            {
                OutError = FString::Printf(TEXT("Build site (%d,%d) is duplicated."), Cell.X, Cell.Y);
                return false;
            }
            BuildCells.Add(Cell);
            return true;
        };

        for (const FIntPoint& Cell : Level.VisibleBuildCells)
        {
            FString Error;
            if (!AddBuildCell(Cell, TEXT("Visible"), Error))
            {
                return Invalid(Error);
            }
        }

        TSet<FName> UnlockingObstacleIds;
        for (const FHiddenBuildSite& Site : Level.HiddenBuildSites)
        {
            FString Error;
            if (!AddBuildCell(Site.Cell, TEXT("Hidden"), Error))
            {
                return Invalid(Error);
            }
            if (!UniqueObstacleIds.Contains(Site.UnlockObstacleId) || UnlockingObstacleIds.Contains(Site.UnlockObstacleId))
            {
                return Invalid(TEXT("Each hidden build site must reference a different obstacle in this level."));
            }
            UnlockingObstacleIds.Add(Site.UnlockObstacleId);
        }

        FValidationResult Result;
        Result.bIsValid = true;
        return Result;
    }
}
