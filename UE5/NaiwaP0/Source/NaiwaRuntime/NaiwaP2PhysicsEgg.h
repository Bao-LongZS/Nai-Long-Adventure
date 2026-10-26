#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NaiwaP2PhysicsEgg.generated.h"

class USphereComponent;
class USkeletalMeshComponent;
class UPhysicalMaterial;
class USoundBase;

/** 真正由 Chaos 驱动的奶蛋弹丸。球形碰撞代理与视觉骨骼网格分离。 */
UCLASS()
class NAIWARUNTIME_API ANaiwaP2PhysicsEgg : public AActor
{
    GENERATED_BODY()

public:
    ANaiwaP2PhysicsEgg();

    void InitializePhysics(double RadiusCm, double MassKg, const FVector& InitialVelocityCmPerS,
                          float Restitution, float Friction, bool bUseCCD);
    void SetVisualScale(double Scale);

    USphereComponent* GetCollisionSphere() const { return CollisionSphere; }
    USkeletalMeshComponent* GetVisualMesh() const { return VisualMesh; }
    int32 GetCollisionCount() const { return CollisionCount; }
    int32 GetStructureImpactCount(const FString& BlockId) const
    {
        const int32* Count = StructureImpactCountById.Find(BlockId);
        return Count ? *Count : 0;
    }
    double GetMaxOutwardReboundSpeedCmPerS(const FString& BlockId) const
    {
        const double* Speed = MaxOutwardReboundSpeedById.Find(BlockId);
        return Speed ? *Speed : 0.0;
    }

private:
    UFUNCTION()
    void HandlePhysicsHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                          UPrimitiveComponent* OtherComp, FVector NormalImpulse,
                          const FHitResult& Hit);

    UPROPERTY(VisibleAnywhere, Category = "Naiwa|P2|Physics")
    TObjectPtr<USphereComponent> CollisionSphere;

    UPROPERTY(VisibleAnywhere, Category = "Naiwa|P2|Physics")
    TObjectPtr<USkeletalMeshComponent> VisualMesh;

    UPROPERTY(Transient)
    TObjectPtr<UPhysicalMaterial> PhysicsMaterial;

    UPROPERTY(Transient)
    TObjectPtr<USoundBase> EggCollisionSound;

    TMap<TWeakObjectPtr<AActor>, double> LastImpactLogTimeByActor;
    double LastCollisionSoundTimeSeconds = -1.0e9;
    TSet<TWeakObjectPtr<AActor>> LoggedSandboxGoals;
    int32 CollisionCount = 0;
    TMap<FString, int32> StructureImpactCountById;
    TMap<FString, double> MaxOutwardReboundSpeedById;
};
