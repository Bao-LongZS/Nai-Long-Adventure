#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "NaiwaP3PlayerPawn.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;
class UNaiwaP3GridSubsystem;
class UNaiwaP3M2RuntimeSubsystem;

UCLASS()
class NAIWARUNTIME_API ANaiwaP3PlayerPawn final : public APawn
{
	GENERATED_BODY()

public:
	ANaiwaP3PlayerPawn();
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	void SetGridWorldPosition(const FVector& Position);
	void SetPortrait(UTexture2D* Portrait);
	void SetDamageFeedback(float Amount, bool bShielded);

private:
	void SetDirection(const FIntPoint& Direction);
	void ClearDirection(const FIntPoint& Direction);
	void MoveUpPressed();
	void MoveUpReleased();
	void MoveDownPressed();
	void MoveDownReleased();
	void MoveLeftPressed();
	void MoveLeftReleased();
	void MoveRightPressed();
	void MoveRightReleased();
	void PlaceBombPressed();
	void ChickenJumpPressed();
	void SelectStandardBomb();
	void SelectSoyBomb();
	void SelectTeaBomb();
	void SelectRatBomb();
	void StageChainDrillPressed();
	void ResetPressed();
	void ReturnToLevelSelect();

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ShadowMesh;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CellFrameComponents;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> CellFrameMaterials;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PortraitMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PortraitMaterial;

	UPROPERTY()
	TObjectPtr<UTexture2D> NeutralPortrait;

	UPROPERTY()
	TObjectPtr<UTexture2D> HitPortrait;

	UPROPERTY()
	TObjectPtr<UTexture2D> ActivePortrait;

	FLinearColor DefaultFrameColor = FLinearColor::FromSRGBColor(FColor(78, 208, 182));

	TWeakObjectPtr<UNaiwaP3GridSubsystem> GridSubsystem;
	TWeakObjectPtr<UNaiwaP3M2RuntimeSubsystem> M2RuntimeSubsystem;
	FIntPoint InputDirection = FIntPoint::ZeroValue;
};
