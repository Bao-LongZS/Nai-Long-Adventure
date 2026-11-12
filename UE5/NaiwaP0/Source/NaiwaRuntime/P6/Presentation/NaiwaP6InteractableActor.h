#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NaiwaP6InteractableActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class NAIWARUNTIME_API ANaiwaP6InteractableActor final : public AActor
{
	GENERATED_BODY()

public:
	ANaiwaP6InteractableActor();
	void InitializePoint(FName InPointId);
	FString Interact();
	FString ToggleInfoLayer(FName CurrentSkinId);
	/** Legacy M1 probe adapter for the original trophy-impact layer. */
	bool ToggleInfoLayer();
	void HideInfoLayer();
	FName GetPointId() const { return PointId; }
	FName GetClueId() const;
	FString GetDisplayName() const;
	FString GetInfoLayerDescription(FName SkinId) const;
	bool HasInfoLayer() const;
	bool HasInfoLayer(FName SkinId) const;
	bool IsInfoLayerVisible() const { return bInfoLayerVisible; }

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> MainMesh;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> InfoLayerMeshes;

	TArray<FName> InfoLayerMeshClueIds;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	FName PointId = NAME_None;
	FName ActiveInfoLayerClueId = NAME_None;
	bool bInfoLayerVisible = false;
};
