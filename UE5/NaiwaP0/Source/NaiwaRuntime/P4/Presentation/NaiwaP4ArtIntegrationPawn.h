#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "NaiwaP4ArtIntegrationPawn.generated.h"

class UDataTable;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UTexture2D;

USTRUCT()
struct FNaiwaP4ActiveDamageNumber
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> Component = nullptr;

	UPROPERTY(Transient)
	FVector InitialLocation = FVector::ZeroVector;

	float AgeSeconds = 0.0f;
};

USTRUCT()
struct FNaiwaP4RuntimeVfxFrame
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Texture = nullptr;

	int32 FrameIndex = 0;
	float DurationSeconds = 0.1f;
};

USTRUCT()
struct FNaiwaP4RuntimeVfxSequence
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<FNaiwaP4RuntimeVfxFrame> Frames;

	bool bLoop = false;
};

USTRUCT()
struct FNaiwaP4ActiveVfx
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Component = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material = nullptr;

	FName EffectId;
	FVector2D LogicalPosition = FVector2D::ZeroVector;
	FVector WorldScale = FVector(2.2f, 2.2f, 1.0f);
	float WorldZ = 6.0f;
	int32 FollowEntityId = INDEX_NONE;
	int32 FrameIndex = 0;
	float FrameElapsedSeconds = 0.0f;
	bool bFollowEntity = false;
	bool bDestroyIfTargetMissing = false;
	bool bFlipX = false;
};

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
	void SyncGrayboxEntities(float DeltaSeconds);
	void SyncDamageNumbers(float DeltaSeconds);
	void LoadVfxCatalog();
	void ProcessVfxEvents();
	void SyncActionVfx();
	void SyncActiveVfx(float DeltaSeconds);
	void LoadBossCatalog();
	void ApplyBossArt(const struct FNaiwaP4EntityState& Boss, UStaticMeshComponent* Mesh, float DeltaSeconds);
	FName ResolveBossSequenceId(const struct FNaiwaP4EntityState& Boss) const;
	void PlayVfx(FName EffectId, FVector2D LogicalPosition, int32 FollowEntityId,
		bool bFollowEntity, bool bDestroyIfTargetMissing, FVector WorldScale,
		bool bFlipX = false, float WorldZ = -1.0f);
	void StopVfx(FName EffectId);
	const struct FNaiwaP4EntityState* FindEntity(int32 EntityId) const;
	FVector ResolveVfxWorldLocation(const FNaiwaP4ActiveVfx& Vfx) const;
	FVector2D GetEntityWorldPresentationPosition(const struct FNaiwaP4EntityState& Entity) const;
	FName ResolveFrameId(const struct FNaiwaP4EntityState& Player) const;
	void ApplyFrame(FName FrameId, bool bFacingRight);

	UPROPERTY(VisibleAnywhere, Category="P4|Presentation")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category="P4|Presentation")
	TObjectPtr<UStaticMeshComponent> SpritePlane;

	UPROPERTY(VisibleAnywhere, Category="P4|Presentation")
	TObjectPtr<UStaticMeshComponent> ContactShadow;

	UPROPERTY(VisibleAnywhere, Category="P4|Presentation")
	TObjectPtr<UStaticMeshComponent> ArenaBackdrop;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> FrameCatalog;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> VfxFrameCatalog;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> BossFrameCatalog;

	UPROPERTY(Transient)
	TMap<FName, FNaiwaP4RuntimeVfxSequence> VfxSequences;

	UPROPERTY(Transient)
	TMap<FName, FNaiwaP4RuntimeVfxSequence> BossSequences;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SpriteMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SpriteMaterialBase;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DamageNumberTextMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SoftShadowMaterial;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> LoadedFrames;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> GrayboxSphereMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> GrayboxEnemyMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> EnemySpritePlaneMesh;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> NaiDingStaticPreviewTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> NaiDunStaticPreviewTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> NaiDingStaticPreviewMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> NaiDunStaticPreviewMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> NaiTongBossPreviewMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GrayboxProjectileMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> NaiDingEnemyMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> NaiDunEnemyMaterial;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UStaticMeshComponent>> GrayboxEntityMeshes;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UStaticMeshComponent>> EnemyContactShadows;

	UPROPERTY(Transient)
	TArray<FNaiwaP4ActiveDamageNumber> ActiveDamageNumbers;

	UPROPERTY(Transient)
	TArray<FNaiwaP4ActiveVfx> ActiveVfx;

	TMap<int32, int32> PreviousEntityHP;
	int64 LastObservedSimulationTick = -1;
	int32 LastProcessedVfxEventCount = 0;
	uint8 PreviousPlayerActionCue = MAX_uint8;
	float ImpactPresentationFreezeSecondsRemaining = 0.0f;

	FName CurrentFrameId = NAME_None;
	FName CurrentBossSequenceId = NAME_None;
	int32 CurrentBossFrameIndex = INDEX_NONE;
	float CurrentBossFrameElapsedSeconds = 0.0f;
	float BossImpactPoseRemainingSeconds = 0.0f;
	float BossDeathPoseRemainingSeconds = 0.0f;
	float SpriteHeightUU = 260.0f;
};
