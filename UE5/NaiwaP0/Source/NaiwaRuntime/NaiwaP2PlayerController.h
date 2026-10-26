#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NaiwaP2PlayerController.generated.h"

/** Mouse drag slingshot controls for the isolated 2-1 prototype. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP2PlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ANaiwaP2PlayerController();
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaSeconds) override;
    virtual void SetupInputComponent() override;

    bool IsAimDragging() const { return bIsAimDragging; }
    bool HasValidAimDrag() const { return bHasValidAimDrag; }
    double GetDragPower() const { return CurrentPower; }
    double GetDragAngleDegrees() const { return CurrentAngleDegrees; }
    FVector GetDraggedPouchWorldLocation() const { return DraggedPouchWorldLocation; }

private:
    void StartAimDrag();
    void FinishAimDrag();
    void SelectEggSlot1();
    void SelectEggSlot2();
    void SelectEggSlot3();
    void SelectEggSlot4();
    void SelectEggSlot5();
    void UpdateAimFromCursor();
    void RetryLevel();
    void ReturnToLevelSelect();
    bool CursorOnLaunchPlane(FVector& OutPoint) const;

    FVector2D DragStartPoint = FVector2D::ZeroVector;
    FVector DraggedPouchWorldLocation = FVector(0.0, 0.0, 180.0);
    double CurrentAngleDegrees = 45.0;
    double CurrentPower = 0.0;
    bool bIsAimDragging = false;
    bool bHasValidAimDrag = false;
};
