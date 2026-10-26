#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "NaiwaP2ArcherAnimInstance.generated.h"

class UAnimSequence;

/** Blends Naiwa's idle with an interaction-sampled slingshot draw pose. */
UCLASS(Blueprintable, BlueprintType, Transient)
class NAIWARUNTIME_API UNaiwaP2ArcherAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UNaiwaP2ArcherAnimInstance();

    void SetArcherSequences(UAnimSequence* Idle, UAnimSequence* Draw);
    void SetDrawPose(float Progress, float Weight);
    UAnimSequence* GetIdleSequenceForProxy() const { return IdleSequence.Get(); }
    UAnimSequence* GetDrawSequenceForProxy() const { return DrawSequence.Get(); }
    float GetDrawProgressForProxy() const { return DrawProgress; }
    float GetDrawWeightForProxy() const { return DrawWeight; }

protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> IdleSequence;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> DrawSequence;

    float DrawProgress = 0.0f;
    float DrawWeight = 0.0f;
};
