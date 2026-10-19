#include "NaiwaAnimInstance.h"
#include "NaiwaCharacter.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "AnimationRuntime.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
    /** Native evaluation is used even for an empty AnimBlueprint child. */
    struct FNaiwaAnimInstanceProxy final : FAnimInstanceProxy
    {
        explicit FNaiwaAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

        UAnimSequence* Sequences[4] = { nullptr, nullptr, nullptr, nullptr };
        float Times[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float Weights[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
        int32 State = 0;
        int32 PreviousState = 0;
        float BlendSeconds = 0.12f;
        float WalkRate = 1.0f;
        float JumpHoldFraction = 0.55f;
        uint32 ResetGeneration = 0;
        uint64 EvaluationCount = 0;
        bool bResetPosePending = false;

        virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
        {
            FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
            UNaiwaAnimInstance* Instance = CastChecked<UNaiwaAnimInstance>(InAnimInstance);
            Instance->CaptureCharacterState();
            if (ResetGeneration != Instance->GetResetGeneration())
            {
                ResetGeneration = Instance->GetResetGeneration();
                for (float& Time : Times) Time = 0.0f;
                Weights[0] = 1.0f;
                Weights[1] = Weights[2] = Weights[3] = 0.0f;
                PreviousState = 0;
                bResetPosePending = true;
            }
            Sequences[0] = Instance->IdleSequence;
            Sequences[1] = Instance->WalkSequence;
            Sequences[2] = Instance->JumpSequence;
            Sequences[3] = Instance->LaughSequence;
            State = static_cast<int32>(Instance->LocomotionState);
            BlendSeconds = FMath::Max(0.01f, Instance->TransitionSeconds);
            WalkRate = FMath::Clamp(Instance->Speed / FMath::Max(1.0f, Instance->WalkReferenceSpeed), 0.1f, 2.0f);
            JumpHoldFraction = FMath::Clamp(Instance->JumpAirHoldFraction, 0.05f, 0.95f);
        }

        virtual void Update(float DeltaSeconds) override
        {
            // Present the requested reset pose once before advancing the clock.
            // A slow first frame must not skip the start of the animation.
            const float AnimationDelta = bResetPosePending ? 0.0f : DeltaSeconds;
            bResetPosePending = false;
            if (State != PreviousState)
            {
                Times[State] = 0.0f;
                PreviousState = State;
            }
            float TotalWeight = 0.0f;
            for (int32 Index = 0; Index < 4; ++Index)
            {
                Weights[Index] = FMath::FInterpConstantTo(Weights[Index], Index == State ? 1.0f : 0.0f,
                    AnimationDelta, 1.0f / BlendSeconds);
                TotalWeight += Weights[Index];
                if (Sequences[Index])
                {
                    const float Length = Sequences[Index]->GetPlayLength();
                    const float Advance = AnimationDelta * (Index == 1 ? WalkRate : 1.0f);
                    if (Index == 2)
                    {
                        Times[Index] = FMath::Min(Times[Index] + Advance, Length * JumpHoldFraction);
                    }
                    else if (Index == 3)
                    {
                        // A single death event advances Laugh once, then holds its last pose.
                        Times[Index] = FMath::Min(Times[Index] + Advance, Length);
                    }
                    else if (Length > UE_SMALL_NUMBER)
                    {
                        Times[Index] = FMath::Fmod(Times[Index] + Advance, Length);
                    }
                }
            }
            if (TotalWeight > UE_SMALL_NUMBER)
            {
                for (float& Weight : Weights)
                {
                    Weight /= TotalWeight;
                }
            }
        }

        void Sample(int32 Index, FPoseContext& Context) const
        {
            Context.ResetToRefPose();
            if (Sequences[Index])
            {
                FAnimationPoseData PoseData(Context);
                const FAnimExtractContext ExtractContext(Times[Index], false, {}, Index < 2);
                Sequences[Index]->GetAnimationPose(PoseData, ExtractContext);
            }
        }

        virtual bool Evaluate(FPoseContext& Output) override
        {
            ++EvaluationCount;
            FPoseContext IdlePose(Output);
            FPoseContext WalkPose(Output);
            FPoseContext JumpPose(Output);
            FPoseContext GroundPose(Output);
            FPoseContext BasePose(Output);
            FPoseContext LaughPose(Output);
            Sample(0, IdlePose);
            Sample(1, WalkPose);
            Sample(2, JumpPose);
            Sample(3, LaughPose);
            const float GroundWeight = Weights[0] + Weights[1];
            FAnimationPoseData IdleData(IdlePose);
            FAnimationPoseData WalkData(WalkPose);
            FAnimationPoseData JumpData(JumpPose);
            FAnimationPoseData GroundData(GroundPose);
            FAnimationPoseData BaseData(BasePose);
            FAnimationPoseData LaughData(LaughPose);
            FAnimationPoseData OutputData(Output);
            FAnimationRuntime::BlendTwoPosesTogether(IdleData, WalkData,
                GroundWeight > UE_SMALL_NUMBER ? Weights[0] / GroundWeight : 1.0f, GroundData);
            const float BaseWeight = GroundWeight + Weights[2];
            FAnimationRuntime::BlendTwoPosesTogether(GroundData, JumpData,
                BaseWeight > UE_SMALL_NUMBER ? GroundWeight / BaseWeight : 1.0f, BaseData);
            FAnimationRuntime::BlendTwoPosesTogether(BaseData, LaughData, BaseWeight, OutputData);

            // CharacterMovement alone moves the capsule. Keep the skeleton Root at its reference transform.
            if (Output.Pose.GetNumBones() > 0)
            {
                const FCompactPoseBoneIndex RootIndex(0);
                Output.Pose[RootIndex] = Output.Pose.GetBoneContainer().GetRefPoseTransform(RootIndex);
            }
            return true;
        }
    };
}

UNaiwaAnimInstance::UNaiwaAnimInstance()
{
    RootMotionMode = ERootMotionMode::IgnoreRootMotion;
}

void UNaiwaAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    CaptureCharacterState();
}

void UNaiwaAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    CaptureCharacterState();
}

void UNaiwaAnimInstance::CaptureCharacterState()
{
    if (const ANaiwaCharacter* Character = Cast<ANaiwaCharacter>(TryGetPawnOwner()))
    {
        IdleSequence = Character->IdleSequence;
        WalkSequence = Character->IsRunningGait() && Character->RunSequence
            ? Character->RunSequence : Character->WalkSequence;
        WalkReferenceSpeed = Character->IsRunningGait() ? 300.0f : 150.0f;
        JumpSequence = Character->JumpSequence;
        LaughSequence = Character->LaughSequence;
        Speed = FMath::Abs(Character->GetVelocity().X);
        bIsFalling = Character->GetCharacterMovement()->IsFalling();
        LocomotionState = Character->IsP1DeathPoseActive() ? ENaiwaLocomotionState::Laugh
            : (bIsFalling ? ENaiwaLocomotionState::Jump
                : (Speed > 3.0f ? ENaiwaLocomotionState::Walk : ENaiwaLocomotionState::Idle));
    }
}

FAnimInstanceProxy* UNaiwaAnimInstance::CreateAnimInstanceProxy()
{
    return new FNaiwaAnimInstanceProxy(this);
}

void UNaiwaAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
    delete static_cast<FNaiwaAnimInstanceProxy*>(InProxy);
}

void UNaiwaAnimInstance::ResetLocomotionClock()
{
    ++ResetGeneration;
}

uint64 UNaiwaAnimInstance::GetPoseEvaluationCount() const
{
    return GetProxyOnGameThread<FNaiwaAnimInstanceProxy>().EvaluationCount;
}

float UNaiwaAnimInstance::GetAnimationSampleTime(ENaiwaLocomotionState State) const
{
    return GetProxyOnGameThread<FNaiwaAnimInstanceProxy>().Times[static_cast<int32>(State)];
}

uint32 UNaiwaAnimInstance::GetProxyResetGeneration() const
{
    return GetProxyOnGameThread<FNaiwaAnimInstanceProxy>().ResetGeneration;
}

const UAnimSequence* UNaiwaAnimInstance::GetProxyMovingSequence() const
{
    return GetProxyOnGameThread<FNaiwaAnimInstanceProxy>().Sequences[1];
}
