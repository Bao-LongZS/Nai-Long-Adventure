#include "NaiwaP2ArcherAnimInstance.h"

#include "Animation/AnimationPoseData.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"

namespace
{
    struct FNaiwaP2ArcherAnimInstanceProxy final : FAnimInstanceProxy
    {
        explicit FNaiwaP2ArcherAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

        UAnimSequence* Idle = nullptr;
        UAnimSequence* Draw = nullptr;
        float Progress = 0.0f;
        float Weight = 0.0f;
        float IdleTime = 0.0f;

        virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
        {
            FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
            const UNaiwaP2ArcherAnimInstance* Instance = CastChecked<UNaiwaP2ArcherAnimInstance>(InAnimInstance);
            // Copy UObject references and control values on the game thread; worker evaluation
            // only consumes this snapshot and never follows the actor/component pointer.
            Idle = Instance->GetIdleSequenceForProxy();
            Draw = Instance->GetDrawSequenceForProxy();
            Progress = Instance->GetDrawProgressForProxy();
            Weight = Instance->GetDrawWeightForProxy();
        }

        virtual void Update(float DeltaSeconds) override
        {
            IdleTime += FMath::Max(0.0f, DeltaSeconds);
            if (Idle && Idle->GetPlayLength() > UE_SMALL_NUMBER)
            {
                IdleTime = FMath::Fmod(IdleTime, Idle->GetPlayLength());
            }
            Weight = FMath::Clamp(Weight, 0.0f, 1.0f);
            Progress = FMath::Clamp(Progress, 0.0f, 1.0f);
        }

        static void Sample(UAnimSequence* Sequence, const float Time, const bool bLooping, FPoseContext& Context)
        {
            Context.ResetToRefPose();
            if (Sequence)
            {
                FAnimationPoseData PoseData(Context);
                const FAnimExtractContext ExtractContext(Time, false, {}, bLooping);
                Sequence->GetAnimationPose(PoseData, ExtractContext);
            }
        }

        virtual bool Evaluate(FPoseContext& Output) override
        {
            FPoseContext IdlePose(Output);
            FPoseContext DrawPose(Output);
            FAnimationPoseData IdleData(IdlePose);
            FAnimationPoseData DrawData(DrawPose);
            FAnimationPoseData OutputData(Output);
            Sample(Idle, IdleTime, true, IdlePose);
            const float DrawTime = Draw ? Draw->GetPlayLength() * Progress : 0.0f;
            Sample(Draw, DrawTime, false, DrawPose);
            FAnimationRuntime::BlendTwoPosesTogether(IdleData, DrawData, 1.0f - Weight, OutputData);

            // Character placement is authored at floor level; prevent any FBX root drift.
            if (Output.Pose.GetNumBones() > 0)
            {
                const FCompactPoseBoneIndex RootIndex(0);
                Output.Pose[RootIndex] = Output.Pose.GetBoneContainer().GetRefPoseTransform(RootIndex);
            }
            return true;
        }
    };
}

UNaiwaP2ArcherAnimInstance::UNaiwaP2ArcherAnimInstance()
{
    RootMotionMode = ERootMotionMode::IgnoreRootMotion;
}

void UNaiwaP2ArcherAnimInstance::SetArcherSequences(UAnimSequence* Idle, UAnimSequence* Draw)
{
    IdleSequence = Idle;
    DrawSequence = Draw;
}

void UNaiwaP2ArcherAnimInstance::SetDrawPose(const float Progress, const float Weight)
{
    DrawProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
    DrawWeight = FMath::Clamp(Weight, 0.0f, 1.0f);
}

FAnimInstanceProxy* UNaiwaP2ArcherAnimInstance::CreateAnimInstanceProxy()
{
    return new FNaiwaP2ArcherAnimInstanceProxy(this);
}

void UNaiwaP2ArcherAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
    delete static_cast<FNaiwaP2ArcherAnimInstanceProxy*>(InProxy);
}
