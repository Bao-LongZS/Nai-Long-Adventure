#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "NaiwaAnimInstance.generated.h"

class UAnimSequence;

UENUM(BlueprintType)
enum class ENaiwaLocomotionState : uint8
{
    Idle,
    Walk,
    Jump
};

/** The native pose graph remains active when this class is the parent of ABP_Naiwa. */
UCLASS(Blueprintable, BlueprintType, Transient)
class NAIWARUNTIME_API UNaiwaAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UNaiwaAnimInstance();
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Animation")
    TObjectPtr<UAnimSequence> IdleSequence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Animation")
    TObjectPtr<UAnimSequence> WalkSequence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Animation")
    TObjectPtr<UAnimSequence> JumpSequence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Animation")
    TObjectPtr<UAnimSequence> LaughSequence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Animation", meta = (ClampMin = "0.01"))
    float TransitionSeconds = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Animation", meta = (ClampMin = "1.0"))
    float WalkReferenceSpeed = 300.0f;

    /** Holds a pose while airborne; landing immediately selects a grounded state. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Animation", meta = (ClampMin = "0.05", ClampMax = "0.95"))
    float JumpAirHoldFraction = 0.55f;

    UPROPERTY(BlueprintReadOnly, Category = "Naiwa|State")
    float Speed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Naiwa|State")
    bool bIsFalling = false;

    UPROPERTY(BlueprintReadOnly, Category = "Naiwa|State")
    ENaiwaLocomotionState LocomotionState = ENaiwaLocomotionState::Idle;

    /** Game-thread snapshot; the proxy never reads an Actor on an animation worker thread. */
    void CaptureCharacterState();

    /** Reset is copied to the worker proxy in PreUpdate, before the next pose evaluation. */
    void ResetLocomotionClock();
    uint64 GetPoseEvaluationCount() const;
    float GetAnimationSampleTime(ENaiwaLocomotionState State) const;
    uint32 GetResetGeneration() const { return ResetGeneration; }
    uint32 GetProxyResetGeneration() const;
    const UAnimSequence* GetProxyMovingSequence() const;

protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
    uint32 ResetGeneration = 0;
};
