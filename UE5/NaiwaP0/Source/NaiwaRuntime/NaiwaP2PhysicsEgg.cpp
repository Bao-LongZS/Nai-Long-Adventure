#include "NaiwaP2PhysicsEgg.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"
#include "NaiwaP2PhysicsProfile.h"
#include "NaiwaP2ImpactEffectSubsystem.h"
#include "NaiwaP2PhysicsGoal.h"
#include "NaiwaP2StructureBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ANaiwaP2PhysicsEgg::ANaiwaP2PhysicsEgg()
{
    PrimaryActorTick.bCanEverTick = false;

    static ConstructorHelpers::FObjectFinder<USoundBase> CollisionCue(
        TEXT("/Game/Naiwa/P2/Audio/SFX_P2_EggCollision.SFX_P2_EggCollision"));
    if (CollisionCue.Succeeded()) EggCollisionSound = CollisionCue.Object;

    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    SetRootComponent(CollisionSphere);
    CollisionSphere->SetMobility(EComponentMobility::Movable);
    CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CollisionSphere->SetGenerateOverlapEvents(false);
    CollisionSphere->SetNotifyRigidBodyCollision(true);
    CollisionSphere->SetConstraintMode(EDOFMode::XZPlane);

    VisualMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EggVisual"));
    VisualMesh->SetupAttachment(CollisionSphere);
    VisualMesh->SetMobility(EComponentMobility::Movable);
    VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VisualMesh->SetGenerateOverlapEvents(false);
    VisualMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void ANaiwaP2PhysicsEgg::InitializePhysics(const double RadiusCm, const double MassKg,
    const FVector& InitialVelocityCmPerS, const float Restitution, const float Friction,
    const bool bUseCCD)
{
    CollisionSphere->SetSphereRadius(static_cast<float>(RadiusCm), true);
    CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
    CollisionSphere->SetCollisionResponseToAllChannels(ECR_Block);
    CollisionSphere->SetUseCCD(bUseCCD);

    PhysicsMaterial = NewObject<UPhysicalMaterial>(this, TEXT("P2EggPhysMat"));
    PhysicsMaterial->Friction = Friction;
    PhysicsMaterial->StaticFriction = FMath::Max(Friction, Friction + 0.10f);
    PhysicsMaterial->Restitution = Restitution;
    PhysicsMaterial->FrictionCombineMode = EFrictionCombineMode::Max;
    PhysicsMaterial->bOverrideFrictionCombineMode = true;
    PhysicsMaterial->RestitutionCombineMode = EFrictionCombineMode::Max;
    PhysicsMaterial->bOverrideRestitutionCombineMode = true;
    CollisionSphere->SetPhysMaterialOverride(PhysicsMaterial);

    CollisionSphere->SetLinearDamping(0.01f);
    CollisionSphere->SetAngularDamping(0.04f);
    CollisionSphere->SetSimulatePhysics(true);
    CollisionSphere->SetMassOverrideInKg(NAME_None, static_cast<float>(MassKg), true);
    if (FBodyInstance* Body = CollisionSphere->GetBodyInstance())
    {
        Body->SleepFamily = NaiwaP2::Physics::BlockSleepFamily;
        Body->SetMaxDepenetrationVelocity(NaiwaP2::Physics::MaxDepenetrationVelocityCmPerS);
    }
    CollisionSphere->SetPhysicsLinearVelocity(InitialVelocityCmPerS, false);
    CollisionSphere->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector, false);
    CollisionSphere->OnComponentHit.AddDynamic(this, &ANaiwaP2PhysicsEgg::HandlePhysicsHit);

    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P2_PHYS_EGG_INIT radius_cm=%.1f mass_kg=%.2f velocity_cm_s=(%.1f,%.1f,%.1f) ccd=%d"),
        RadiusCm, MassKg, InitialVelocityCmPerS.X, InitialVelocityCmPerS.Y,
        InitialVelocityCmPerS.Z, bUseCCD ? 1 : 0);
}

void ANaiwaP2PhysicsEgg::SetVisualScale(const double Scale)
{
    VisualMesh->SetRelativeScale3D(FVector(Scale));
}

void ANaiwaP2PhysicsEgg::HandlePhysicsHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, const FVector NormalImpulse, const FHitResult& Hit)
{
    using namespace NaiwaP2::Physics;
    UWorld* World = GetWorld();
    if (!World || !OtherActor)
    {
        return;
    }

    const ANaiwaP2StructureBlock* StructureBlock = Cast<ANaiwaP2StructureBlock>(OtherActor);
    const double CollisionSoundNow = World->GetTimeSeconds();
    if (EggCollisionSound && StructureBlock && CollisionSphere)
    {
        const double EggMassKg = CollisionSphere->GetMass();
        const double BuildingMassKg = StructureBlock->GetMassKg();
        const double TotalMassKg = EggMassKg + BuildingMassKg;
        if (EggMassKg > UE_SMALL_NUMBER && BuildingMassKg > UE_SMALL_NUMBER &&
            TotalMassKg > UE_SMALL_NUMBER)
        {
            // NormalImpulse is the momentum exchanged in the collision. Its
            // squared magnitude over twice the reduced mass estimates the
            // collision's kinetic energy (kg*cm^2/s^2 -> joules).
            const double ReducedMassKg = EggMassKg * BuildingMassKg / TotalMassKg;
            const double ImpactKineticEnergyJoules =
                0.5 * FMath::Square(static_cast<double>(NormalImpulse.Size())) /
                (ReducedMassKg * 10000.0);
            const double SoundCooldownSeconds = FMath::Max(
                CollisionSoundMinIntervalSeconds,
                static_cast<double>(EggCollisionSound->GetDuration()));

            if (ImpactKineticEnergyJoules >= CollisionSoundMinKineticEnergyJoules &&
                CollisionSoundNow - LastCollisionSoundTimeSeconds >= SoundCooldownSeconds)
            {
                LastCollisionSoundTimeSeconds = CollisionSoundNow;
                UGameplayStatics::PlaySoundAtLocation(this, EggCollisionSound,
                    Hit.ImpactPoint, 0.75f);
            }
        }
    }

    if (Hit.bStartPenetrating)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("NAIWA_P2_PHYS_PENETRATION source=egg other=%s depth_cm=%.3f point_cm=(%.1f,%.1f,%.1f)"),
            *GetNameSafe(OtherActor), Hit.PenetrationDepth,
            Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
    }

    const bool bSandboxGoal = OtherActor->ActorHasTag(FName(TEXT("NaiwaP2PhysicsSandboxGoal")));
    if (bSandboxGoal)
    {
        // A goal should be reported once even if Chaos keeps dispatching while the egg rests against it.
        if (LoggedSandboxGoals.Contains(OtherActor))
        {
            return;
        }
        LoggedSandboxGoals.Add(OtherActor);
    }
    else
    {
        const double Impulse = NormalImpulse.Size();
        if (Impulse < CollisionLogMinImpulseKgCmPerS)
        {
            return;
        }

        const FVector SelfVelocity = CollisionSphere->GetPhysicsLinearVelocity();
        const FVector OtherVelocity = OtherComp
            ? OtherComp->GetPhysicsLinearVelocity()
            : FVector::ZeroVector;
        const FVector RelativeVelocity = SelfVelocity - OtherVelocity;
        if (RelativeVelocity.Size() < CollisionLogMinRelativeSpeedCmPerS)
        {
            return;
        }

        const TWeakObjectPtr<AActor> OtherKey(OtherActor);
        const double Now = World->GetTimeSeconds();
        if (const double* LastLogTime = LastImpactLogTimeByActor.Find(OtherKey))
        {
            if (Now - *LastLogTime < CollisionLogPairCooldownSeconds)
            {
                return;
            }
        }
        LastImpactLogTimeByActor.Add(OtherKey, Now);
    }

    ++CollisionCount;
    const FVector Velocity = CollisionSphere->GetPhysicsLinearVelocity();
    const double OutwardReboundSpeed = FVector::DotProduct(Velocity, Hit.ImpactNormal);
    const FString StructureBlockId = StructureBlock ? StructureBlock->GetBlockId() : FString();
    if (StructureBlock)
    {
        ++StructureImpactCountById.FindOrAdd(StructureBlockId);
        double& MaxOutwardSpeed = MaxOutwardReboundSpeedById.FindOrAdd(StructureBlockId);
        MaxOutwardSpeed = FMath::Max(MaxOutwardSpeed, FMath::Max(0.0, OutwardReboundSpeed));
    }
    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P2_PHYS_EGG_HIT count=%d other=%s other_id=%s point_cm=(%.1f,%.1f,%.1f) normal=(%.3f,%.3f,%.3f) "
             "speed_cm_s=%.1f velocity_cm_s=(%.1f,%.1f,%.1f) outward_rebound_cm_s=%.1f impulse_kg_cm_s=%.1f "
             "start_penetrating=%d penetration_depth_cm=%.3f"),
        CollisionCount, *GetNameSafe(OtherActor), StructureBlock ? *StructureBlockId : TEXT("-"),
        Hit.ImpactPoint.X, Hit.ImpactPoint.Y,
        Hit.ImpactPoint.Z, Hit.ImpactNormal.X, Hit.ImpactNormal.Y, Hit.ImpactNormal.Z,
        Velocity.Size(), Velocity.X, Velocity.Y, Velocity.Z, OutwardReboundSpeed, NormalImpulse.Size(),
        Hit.bStartPenetrating ? 1 : 0,
        Hit.bStartPenetrating ? Hit.PenetrationDepth : 0.0f);

    // The full-level goal callback owns goal-hit feedback; all other meaningful
    // projectile contacts are presented here.
    if (!Cast<ANaiwaP2PhysicsGoal>(OtherActor))
    {
        const FVector OtherVelocity = OtherComp
            ? OtherComp->GetPhysicsLinearVelocity()
            : FVector::ZeroVector;
        if (UNaiwaP2ImpactEffectSubsystem* ImpactEffects =
            World->GetSubsystem<UNaiwaP2ImpactEffectSubsystem>())
        {
            ImpactEffects->SpawnImpact(this, OtherActor, Hit.ImpactPoint, Hit.ImpactNormal,
                NormalImpulse.Size(), FVector::Distance(Velocity, OtherVelocity));
        }
    }

    if (bSandboxGoal)
    {
        const FVector GoalVelocity = OtherComp
            ? OtherComp->GetPhysicsLinearVelocity()
            : FVector::ZeroVector;
        UE_LOG(LogTemp, Display,
            TEXT("NAIWA_P2_PHYS_EGG_GOAL_HIT actor=%s goal_velocity_cm_s=(%.1f,%.1f,%.1f) "
                 "impulse_kg_cm_s=%.1f"),
            *GetNameSafe(OtherActor), GoalVelocity.X, GoalVelocity.Y, GoalVelocity.Z,
            NormalImpulse.Size());
    }
}
