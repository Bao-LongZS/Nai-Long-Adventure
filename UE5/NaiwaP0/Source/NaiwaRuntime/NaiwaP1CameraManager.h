#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "NaiwaP1CameraManager.generated.h"

/** Camera behavior for P1 maps only. The P0 pawn camera remains untouched. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP1CameraManager : public APlayerCameraManager
{
    GENERATED_BODY()

public:
    virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;

    UPROPERTY(EditDefaultsOnly, Category = "Naiwa|P1 Camera", meta = (ClampMin = "100.0", Units = "cm"))
    float CameraDistanceCm = 1000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Naiwa|P1 Camera", meta = (Units = "cm"))
    float HeightOffsetCm = 140.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Naiwa|P1 Camera", meta = (ClampMin = "20.0", ClampMax = "120.0", Units = "deg"))
    float HorizontalFovDegrees = 75.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Naiwa|P1 Camera", meta = (ClampMin = "0.1"))
    float HorizontalFollowSpeed = 8.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Naiwa|P1 Camera", meta = (ClampMin = "0.1"))
    float VerticalFollowSpeed = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Naiwa|P1 Camera", meta = (ClampMin = "0.0", Units = "cm"))
    float VerticalDeadZoneCm = 25.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Naiwa|P1 Camera", meta = (ClampMin = "0.0", Units = "cm"))
    float EdgePaddingCm = 80.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Naiwa|P1 Camera", meta = (ClampMin = "0.0", Units = "cm"))
    float TeleportSnapDistanceCm = 450.0f;

private:
    void DiscoverTerrainBounds();
    FVector ConstrainTarget(const FVector& PawnLocation) const;

    bool bHasTerrainBounds = false;
    bool bCameraInitialized = false;
    FVector PreviousPawnLocation = FVector::ZeroVector;
    FVector SmoothedCameraLocation = FVector::ZeroVector;
    float TerrainMinX = 0.0f;
    float TerrainMaxX = 0.0f;
    float TerrainMinTopZ = 0.0f;
    float TerrainMaxTopZ = 0.0f;
};
