#include "NaiwaP1CameraManager.h"

#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

namespace
{
    const FName P1GeneratedTag(TEXT("NaiwaP1GeneratedV1"));
}

void ANaiwaP1CameraManager::DiscoverTerrainBounds()
{
    UWorld* World = GetWorld();
    if (!World) return;

    bool bFound = false;
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        // The P1 builder owns only its terrain StaticMeshActors with this tag;
        // triggers, lights, and the P0 map never contribute to camera bounds.
        if (!It->ActorHasTag(P1GeneratedTag)) continue;
        FVector Origin;
        FVector Extent;
        It->GetActorBounds(false, Origin, Extent);
        if (Extent.X <= 1.0f || Extent.Z <= 1.0f) continue;
        const float Left = Origin.X - Extent.X;
        const float Right = Origin.X + Extent.X;
        const float Top = Origin.Z + Extent.Z;
        if (!bFound)
        {
            TerrainMinX = Left;
            TerrainMaxX = Right;
            TerrainMinTopZ = Top;
            TerrainMaxTopZ = Top;
            bFound = true;
        }
        else
        {
            TerrainMinX = FMath::Min(TerrainMinX, Left);
            TerrainMaxX = FMath::Max(TerrainMaxX, Right);
            TerrainMinTopZ = FMath::Min(TerrainMinTopZ, Top);
            TerrainMaxTopZ = FMath::Max(TerrainMaxTopZ, Top);
        }
    }

    bHasTerrainBounds = bFound;
    if (bFound)
    {
        UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_CAMERA terrain x=[%.1f,%.1f] top_z=[%.1f,%.1f]"),
            TerrainMinX, TerrainMaxX, TerrainMinTopZ, TerrainMaxTopZ);
    }
}

FVector ANaiwaP1CameraManager::ConstrainTarget(const FVector& PawnLocation) const
{
    FVector Result(PawnLocation.X, PawnLocation.Y + CameraDistanceCm, PawnLocation.Z + HeightOffsetCm);
    if (!bHasTerrainBounds) return Result;

    // Horizontal FOV defines the visible half-width at the fixed camera distance.
    const float HalfViewWidth = CameraDistanceCm * FMath::Tan(FMath::DegreesToRadians(HorizontalFovDegrees * 0.5f));
    const float MinCameraX = TerrainMinX + HalfViewWidth - EdgePaddingCm;
    const float MaxCameraX = TerrainMaxX - HalfViewWidth + EdgePaddingCm;
    Result.X = MinCameraX <= MaxCameraX
        ? FMath::Clamp(Result.X, MinCameraX, MaxCameraX)
        : (TerrainMinX + TerrainMaxX) * 0.5f;

    // Keep the floor and highest landing area in view while a pawn jumps or falls.
    const float MinCameraZ = TerrainMinTopZ + HeightOffsetCm + 90.0f;
    const float MaxCameraZ = TerrainMaxTopZ + HeightOffsetCm + 90.0f;
    Result.Z = FMath::Clamp(Result.Z, MinCameraZ, MaxCameraZ);
    return Result;
}

void ANaiwaP1CameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
    // Preserve the pawn camera's post-process/aspect settings, then position the
    // P1 view independently of the rigid camera component attached to the pawn.
    Super::UpdateViewTarget(OutVT, DeltaTime);
    const APawn* Pawn = Cast<APawn>(OutVT.Target);
    if (!IsValid(Pawn))
    {
        bCameraInitialized = false;
        return;
    }

    if (!bHasTerrainBounds) DiscoverTerrainBounds();
    const FVector PawnLocation = Pawn->GetActorLocation();
    const FVector Desired = ConstrainTarget(PawnLocation);
    const bool bTeleport = bCameraInitialized
        && FVector::DistSquared(PawnLocation, PreviousPawnLocation) > FMath::Square(TeleportSnapDistanceCm);
    if (!bCameraInitialized || bTeleport)
    {
        SmoothedCameraLocation = Desired;
        bCameraInitialized = true;
        if (bTeleport) UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_CAMERA snap_after_teleport"));
    }
    else
    {
        SmoothedCameraLocation.X = FMath::FInterpTo(SmoothedCameraLocation.X, Desired.X, DeltaTime, HorizontalFollowSpeed);
        SmoothedCameraLocation.Y = Desired.Y;
        if (FMath::Abs(Desired.Z - SmoothedCameraLocation.Z) > VerticalDeadZoneCm)
        {
            SmoothedCameraLocation.Z = FMath::FInterpTo(SmoothedCameraLocation.Z, Desired.Z, DeltaTime, VerticalFollowSpeed);
        }
    }
    PreviousPawnLocation = PawnLocation;
    OutVT.POV.Location = SmoothedCameraLocation;
    OutVT.POV.Rotation = FRotator(0.0f, -90.0f, 0.0f);
    OutVT.POV.FOV = HorizontalFovDegrees;
}
