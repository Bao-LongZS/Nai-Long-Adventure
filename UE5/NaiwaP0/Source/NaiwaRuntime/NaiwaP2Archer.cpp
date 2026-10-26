#include "NaiwaP2Archer.h"

#include "NaiwaP2ArcherAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"

ANaiwaP2Archer::ANaiwaP2Archer()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    SetActorEnableCollision(false);
    SetCanBeDamaged(false);

    USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);
    CharacterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("NaiwaMesh"));
    CharacterMesh->SetupAttachment(SceneRoot);
    CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CharacterMesh->SetGenerateOverlapEvents(false);
    CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    CharacterMesh->bEnableUpdateRateOptimizations = false;

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshFinder(
        TEXT("/Game/Naiwa/SK_Naiwa.SK_Naiwa"));
    if (MeshFinder.Succeeded()) CharacterAsset = MeshFinder.Object;

    static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleFinder(
        TEXT("/Game/Naiwa/AS_Naiwa_Idle.AS_Naiwa_Idle"));
    if (IdleFinder.Succeeded()) IdleAsset = IdleFinder.Object;

    static ConstructorHelpers::FObjectFinder<UAnimSequence> DrawFinder(
        TEXT("/Game/Naiwa/P2/Art/Character/AS_Naiwa_P2_SlingshotDraw.AS_Naiwa_P2_SlingshotDraw"));
    if (DrawFinder.Succeeded()) DrawAsset = DrawFinder.Object;
}

void ANaiwaP2Archer::BeginPlay()
{
    Super::BeginPlay();
    if (CharacterAsset)
    {
        CharacterMesh->SetSkeletalMesh(CharacterAsset);
    }
    CharacterMesh->SetRelativeScale3D(FVector(0.682638f));
    CharacterMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    CharacterMesh->SetAnimInstanceClass(UNaiwaP2ArcherAnimInstance::StaticClass());
    if (UNaiwaP2ArcherAnimInstance* Anim = Cast<UNaiwaP2ArcherAnimInstance>(CharacterMesh->GetAnimInstance()))
    {
        Anim->SetArcherSequences(IdleAsset, DrawAsset);
        Anim->SetDrawPose(DrawProgress, DrawWeight);
        bAnimSequencesAssigned = true;
    }
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_ARCHER_ASSETS mesh=%s idle=%s draw=%s"),
        *GetNameSafe(CharacterAsset), *GetNameSafe(IdleAsset), *GetNameSafe(DrawAsset));
}

void ANaiwaP2Archer::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float SafeDelta = FMath::Max(0.0f, DeltaSeconds);
    DrawHoldClock += SafeDelta;

    float ProgressTarget = bDrawActive ? TargetDrawProgress : 0.0f;
    if (bDrawActive && bFullTension)
    {
        // Tiny breathing variation keeps the held pose alive at capped power.
        ProgressTarget = 0.975f + 0.012f * FMath::Sin(DrawHoldClock * 2.0f);
    }
    DrawProgress = FMath::FInterpTo(DrawProgress, ProgressTarget, SafeDelta,
        bDrawActive ? 7.5f : 13.0f);
    DrawWeight = FMath::FInterpTo(DrawWeight, bDrawActive ? 1.0f : 0.0f, SafeDelta,
        bDrawActive ? 11.0f : 12.0f);

    float Recoil = 0.0f;
    if (RecoilRemaining > 0.0f)
    {
        RecoilRemaining = FMath::Max(0.0f, RecoilRemaining - SafeDelta);
        const float ReleaseProgress = 1.0f - RecoilRemaining / RecoilDuration;
        Recoil = FMath::Sin(PI * FMath::Clamp(ReleaseProgress, 0.0f, 1.0f));
    }
    CharacterMesh->SetRelativeRotation(FRotator(1.25f * Recoil, -90.0f, 0.0f));

    if (UNaiwaP2ArcherAnimInstance* Anim = Cast<UNaiwaP2ArcherAnimInstance>(CharacterMesh->GetAnimInstance()))
    {
        if (!bAnimSequencesAssigned)
        {
            Anim->SetArcherSequences(IdleAsset, DrawAsset);
            bAnimSequencesAssigned = true;
        }
        Anim->SetDrawPose(DrawProgress, DrawWeight);
    }
}

void ANaiwaP2Archer::BeginDraw()
{
    bDrawActive = true;
    bFullTension = false;
    TargetDrawProgress = 0.0f;
    DrawHoldClock = 0.0f;
    RecoilRemaining = 0.0f;
}

void ANaiwaP2Archer::SetDrawProgress(const float Progress)
{
    TargetDrawProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
    bFullTension = TargetDrawProgress >= 0.995f;
}

void ANaiwaP2Archer::ReleaseDraw()
{
    if (!bDrawActive) return;
    bDrawActive = false;
    bFullTension = false;
    TargetDrawProgress = 0.0f;
    RecoilRemaining = RecoilDuration;
}

void ANaiwaP2Archer::CancelDraw()
{
    bDrawActive = false;
    bFullTension = false;
    TargetDrawProgress = 0.0f;
    RecoilRemaining = 0.0f;
}

void ANaiwaP2Archer::ResetDrawPose()
{
    bDrawActive = false;
    bFullTension = false;
    TargetDrawProgress = 0.0f;
    DrawProgress = 0.0f;
    DrawWeight = 0.0f;
    RecoilRemaining = 0.0f;
    DrawHoldClock = 0.0f;
}
