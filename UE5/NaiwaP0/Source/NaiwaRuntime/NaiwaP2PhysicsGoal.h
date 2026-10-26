#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NaiwaP2PhysicsGoal.generated.h"

class UMaterialInterface;
class UCapsuleComponent;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Physical target egg for full-physics P2 levels. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP2PhysicsGoal : public AActor
{
    GENERATED_BODY()

public:
    ANaiwaP2PhysicsGoal();

    void InitializeVisual(UStaticMesh* Mesh, UMaterialInterface* Material,
        const FVector& VisualScale, float CollisionRadiusCm, float CollisionHalfHeightCm);
    void InitializeGeoEggVisual(UMaterialInterface* Material, const FVector& VisualScale);

    UCapsuleComponent* GetCollisionCapsule() const { return CollisionCapsule; }
    UStaticMeshComponent* GetVisualMesh() const { return VisualMesh; }

private:
    UPROPERTY(VisibleAnywhere, Category="P2 Physics Goal")
    TObjectPtr<UCapsuleComponent> CollisionCapsule;

    UPROPERTY(VisibleAnywhere, Category="P2 Physics Goal")
    TObjectPtr<UStaticMeshComponent> VisualMesh;

    UPROPERTY(VisibleAnywhere, Category="P2 Physics Goal")
    TObjectPtr<UProceduralMeshComponent> GeoEggVisualMesh;
};
