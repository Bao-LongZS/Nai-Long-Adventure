#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NaiwaP3BombTypes.h"
#include "NaiwaP3BombActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;

UCLASS()
class NAIWARUNTIME_API ANaiwaP3BombActor final : public AActor
{
	GENERATED_BODY()

public:
	ANaiwaP3BombActor();
	virtual void Tick(float DeltaTime) override;
	void SetWorldPosition(const FVector& Position);
	void SetFuseFraction(float RemainingFraction);
	void SetBombType(ENaiwaP3BombType Type);
	void BeginVisualSlide(const FVector& Start, const FVector& End, float DurationSeconds);
	bool IsVisualSliding() const { return bVisualSliding; }

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BombMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> FuseMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SparkMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BombSpriteMesh;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TypeMarkerComponents;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BombMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SparkMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> TypeMarkerMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SpriteMaterial;

	ENaiwaP3BombType BombType = ENaiwaP3BombType::Standard;

	float RemainingFraction = 1.0f;
	FVector VisualSlideStart = FVector::ZeroVector;
	FVector VisualSlideEnd = FVector::ZeroVector;
	float VisualSlideElapsed = 0.0f;
	float VisualSlideDuration = 0.18f;
	bool bVisualSliding = false;
};
