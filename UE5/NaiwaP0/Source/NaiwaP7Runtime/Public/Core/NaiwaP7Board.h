#pragma once

#include "CoreMinimal.h"

namespace NaiwaP7
{
    inline constexpr int32 BoardColumns = 20;
    inline constexpr int32 BoardRows = 11;
    // The presentation trims unused rows while retaining the stable 20x11 gameplay coordinates.
    inline constexpr int32 PresentationFirstVisibleRow = 3;
    inline constexpr int32 PresentationVisibleRows = 7;
    inline constexpr int32 VisibleBuildSiteCount = 8;
    inline constexpr int32 HiddenBuildSiteCount = 2;
    inline constexpr double ReferenceCellWidth = 96.0;
    inline constexpr double ReferenceCellHeight = 52.8;

    enum class ERoute : uint8
    {
        A,
        B
    };

    struct FRoutePath
    {
        TArray<FIntPoint> Cells;
    };

    struct FHiddenBuildSite
    {
        FIntPoint Cell = FIntPoint::ZeroValue;
        FName UnlockObstacleId = NAME_None;
    };

    struct FLevelDefinition
    {
        FName LevelId = NAME_None;
        FRoutePath RouteA;
        FRoutePath RouteB;
        FIntPoint InterferenceTriggerCell = FIntPoint::ZeroValue;
        TArray<FIntPoint> VisibleBuildCells;
        TArray<FName> ObstacleIds;
        TArray<FHiddenBuildSite> HiddenBuildSites;
    };

    struct FValidationResult
    {
        bool bIsValid = false;
        FString Error;
    };

    NAIWAP7RUNTIME_API bool IsCellInBounds(const FIntPoint& Cell);

    /** Maps a point in the rendered board rectangle to the stable 20x11 logical grid. */
    NAIWAP7RUNTIME_API bool TryMapBoardPointToCell(
        const FVector2D& BoardLocalPoint,
        const FVector2D& BoardSize,
        FIntPoint& OutCell);

    /** Maps the cropped presentation board back to the stable logical row range. */
    NAIWAP7RUNTIME_API bool TryMapBoardPointToCell(
        const FVector2D& BoardLocalPoint,
        const FVector2D& BoardSize,
        int32 FirstVisibleRow,
        int32 VisibleRows,
        FIntPoint& OutCell);

    /** Validates the alpha fixture without depending on P4/P5 data or runtime types. */
    NAIWAP7RUNTIME_API FValidationResult ValidateLevelDefinition(const FLevelDefinition& Level);
}
