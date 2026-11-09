#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NaiwaP5PrototypeTrackActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class USceneComponent;
struct FNaiwaP5RouteFixture;

/** Temporary visible blockout. Collision and event resolution live in the run subsystem. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP5PrototypeTrackActor final : public AActor
{
	GENERATED_BODY()

public:
	ANaiwaP5PrototypeTrackActor();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UStaticMeshComponent* AddBlockoutMesh(FName Name, UStaticMesh* Mesh, const FVector& Location,
		const FVector& Scale, const FLinearColor& Color);
	UStaticMeshComponent* AddA0Sprite(FName Name, UMaterialInterface* Material,
		const FVector& Location, const FVector2D& SizeCm);
	void BuildA0BackgroundLayers(double RouteLengthCm);
	bool BuildA0RouteSurface(const FNaiwaP5RouteFixture& Fixture);
	void HideLegacyA0SampleActors();
	void EmitA0VisualProbe(const FString& Trigger);

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> GroundMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> AvalancheWallMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> AvalancheCrestMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> AvalanchePlumeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> A0SkyMesh;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> A0SnowBackdropMesh;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> A0RouteSurfaceMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> A0PlayerShadowMesh;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> A0BackgroundMeshes;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> A0KangarooEventMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> A0BackgroundMaterials;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaneMesh;

	bool bA0ArtSample = false;
	int32 NextA0VisualProbeIndex = 0;
};
