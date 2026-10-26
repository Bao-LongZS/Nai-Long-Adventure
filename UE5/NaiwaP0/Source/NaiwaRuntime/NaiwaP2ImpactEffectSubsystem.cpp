#include "NaiwaP2ImpactEffectSubsystem.h"

#include "Engine/World.h"
#include "NaiwaP2PhysicsProfile.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

void UNaiwaP2ImpactEffectSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    ImpactBurstSystem = LoadObject<UNiagaraSystem>(nullptr,
        TEXT("/Niagara/DefaultAssets/Templates/Systems/RadialBurst.RadialBurst"));
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P2_IMPACT_VFX_INIT loaded=%d"),
        ImpactBurstSystem != nullptr ? 1 : 0);
}

void UNaiwaP2ImpactEffectSubsystem::SpawnImpact(const AActor* ActorA, const AActor* ActorB,
    const FVector& ImpactPoint, const FVector& ImpactNormal,
    const double ImpulseKgCmPerS, const double RelativeSpeedCmPerS)
{
    using namespace NaiwaP2::Physics;
    UWorld* World = GetWorld();
    if (!World || !ImpactBurstSystem || !ActorA || !ActorB ||
        ImpulseKgCmPerS < ImpactEffectMinImpulseKgCmPerS ||
        RelativeSpeedCmPerS < ImpactEffectMinRelativeSpeedCmPerS)
    {
        return;
    }

    const uint32 IdA = static_cast<uint32>(ActorA->GetUniqueID());
    const uint32 IdB = static_cast<uint32>(ActorB->GetUniqueID());
    const uint32 LowId = FMath::Min(IdA, IdB);
    const uint32 HighId = FMath::Max(IdA, IdB);
    const uint64 PairKey = (static_cast<uint64>(LowId) << 32) | HighId;
    const double Now = World->GetTimeSeconds();

    if (bHasSpawnedWorldImpact &&
        Now - LastWorldSpawnTimeSeconds < ImpactEffectGlobalCooldownSeconds)
    {
        return;
    }

    if (const double* LastSpawnTime = LastSpawnTimeByActorPair.Find(PairKey))
    {
        if (Now - *LastSpawnTime < ImpactEffectPairCooldownSeconds)
        {
            return;
        }
    }

    for (auto It = LastSpawnTimeByActorPair.CreateIterator(); It; ++It)
    {
        if (Now - It.Value() > 2.0)
        {
            It.RemoveCurrent();
        }
    }

    const double ImpactStrength = FMath::Clamp(
        (ImpulseKgCmPerS - ImpactEffectMinImpulseKgCmPerS) /
            ImpactEffectScaleImpulseRangeKgCmPerS,
        0.0, 1.0);
    const float Scale = static_cast<float>(FMath::Lerp(
        ImpactEffectMinScale, ImpactEffectMaxScale, ImpactStrength));
    const FVector Normal = ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
    UNiagaraComponent* Burst = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        World, ImpactBurstSystem, ImpactPoint + Normal * ImpactEffectNormalOffsetCm,
        Normal.Rotation(), FVector(Scale), true, true, ENCPoolMethod::AutoRelease, true);
    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P2_IMPACT_VFX spawned=%d point_cm=(%.1f,%.1f,%.1f) impulse=%.1f relative_speed=%.1f scale=%.2f"),
        Burst != nullptr ? 1 : 0, ImpactPoint.X, ImpactPoint.Y, ImpactPoint.Z,
        ImpulseKgCmPerS, RelativeSpeedCmPerS, Scale);
    if (!Burst)
    {
        return;
    }

    LastSpawnTimeByActorPair.Add(PairKey, Now);
    LastWorldSpawnTimeSeconds = Now;
    bHasSpawnedWorldImpact = true;
}
