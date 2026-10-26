#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NaiwaP2Archer.generated.h"

class UAnimSequence;
class USkeletalMesh;
class USkeletalMeshComponent;

/** Non-colliding level-side Naiwa whose draw pose follows slingshot pull power. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP2Archer : public AActor
{
    GENERATED_BODY()

public:
    ANaiwaP2Archer();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    void BeginDraw();
    void SetDrawProgress(float Progress);
    void ReleaseDraw();
    void CancelDraw();
    void ResetDrawPose();

    float GetDrawProgress() const { return DrawProgress; }
    float GetDrawWeight() const { return DrawWeight; }

private:
    UPROPERTY(VisibleAnywhere, Category = "Naiwa|P2 Archer")
    TObjectPtr<USkeletalMeshComponent> CharacterMesh;

    UPROPERTY()
    TObjectPtr<USkeletalMesh> CharacterAsset;

    UPROPERTY()
    TObjectPtr<UAnimSequence> IdleAsset;

    UPROPERTY()
    TObjectPtr<UAnimSequence> DrawAsset;

    float TargetDrawProgress = 0.0f;
    float DrawProgress = 0.0f;
    float DrawWeight = 0.0f;
    float RecoilDuration = 0.22f;
    float RecoilRemaining = 0.0f;
    float DrawHoldClock = 0.0f;
    bool bDrawActive = false;
    bool bFullTension = false;
    bool bAnimSequencesAssigned = false;
};
