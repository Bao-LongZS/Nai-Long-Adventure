#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "NaiwaP4ArtIntegrationPawn.generated.h"

class UDataTable;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTexture2D;

/** Art-only presenter for the deterministic player entity in the P4 integration preview. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP4ArtIntegrationPawn final : public APawn
{
	GENERATED_BODY()

public:
	ANaiwaP4ArtIntegrationPawn();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void LoadFrameCatalog();
	void SyncFromSimulation();
	FName ResolveFrameId(const struct FNaiwaP4EntityState& Player) const;
	void ApplyFrame(FName FrameId, bool bFacingRight);

	UPROPERTY(VisibleAnywhere, Category="P4|Presentation")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category="P4|Presentation")
	TObjectPtr<UStaticMeshComponent> SpritePlane;

	UPROPERTY(VisibleAnywhere, Category="P4|Presentation")
	TObjectPtr<UStaticMeshComponent> ContactShadow;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> FrameCatalog;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SpriteMaterial;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> LoadedFrames;

	FName CurrentFrameId = NAME_None;
	float SpriteHeightUU = 180.0f;
};
