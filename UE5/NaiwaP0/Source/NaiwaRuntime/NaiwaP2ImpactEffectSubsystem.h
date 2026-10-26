#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NaiwaP2ImpactEffectSubsystem.generated.h"

class UNiagaraSystem;

/** Small, throttled Niagara feedback for meaningful P2 physics impacts. */
UCLASS()
class NAIWARUNTIME_API UNaiwaP2ImpactEffectSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    void SpawnImpact(const AActor* ActorA, const AActor* ActorB,
        const FVector& ImpactPoint, const FVector& ImpactNormal,
        double ImpulseKgCmPerS, double RelativeSpeedCmPerS);

private:
    UPROPERTY(Transient)
    TObjectPtr<UNiagaraSystem> ImpactBurstSystem;

    TMap<uint64, double> LastSpawnTimeByActorPair;
    double LastWorldSpawnTimeSeconds = 0.0;
    bool bHasSpawnedWorldImpact = false;
};
