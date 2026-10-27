#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NaiwaP3M2LevelSession.h"
#include "NaiwaP3M2WorldPresentation.generated.h"

class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class UTexture2D;

UCLASS()
class NAIWARUNTIME_API ANaiwaP3M2WorldPresentation final : public AActor
{
	GENERATED_BODY()

public:
	ANaiwaP3M2WorldPresentation();
	void Build(const FNaiwaP3M2SeededLevel& Level);
	void BuildEditorCharacterPreview(const FIntPoint& PlayerCell,
		const TArray<FNaiwaP3M2EnemyPlacement>& Enemies);
	void Sync(const FNaiwaP3M2LevelSession& Session, const FIntPoint& PlayerCell,
		const TArray<FIntPoint>& EnemyCells, const TArray<uint8>& EnemyAlive);
	void SetSlowSplash(const FIntPoint& CenterCell, bool bActive, bool bImpacted,
		const FNaiwaP3FogOfWar& Fog);
	void MarkEggCollected(const FIntPoint& Cell);
	void SetItemAt(const FIntPoint& Cell, ENaiwaP3LootType Loot);
	void RemoveItemAt(const FIntPoint& Cell);

private:
	struct FMechanicVisual
	{
		FIntPoint Cell = FIntPoint::ZeroValue;
		ENaiwaP3M2MechanicKind Kind = ENaiwaP3M2MechanicKind::PressurePlate;
		int32 Variant = 0;
		int32 StateComponent = INDEX_NONE;
		TArray<int32> Components;
		TArray<int32> ActiveComponents;
	};

	int32 AddMesh(UStaticMesh* Mesh, const FVector& Position, const FVector& Scale,
		const FLinearColor& Color, float Yaw = 0.0f);
	int32 AddCube(const FVector& Position, const FVector& Scale, const FLinearColor& Color,
		float Yaw = 0.0f);
	int32 AddSphere(const FVector& Position, const FVector& Scale, const FLinearColor& Color);
	int32 AddCylinder(const FVector& Position, const FVector& Scale, const FLinearColor& Color);
	int32 AddCone(const FVector& Position, const FVector& Scale, const FLinearColor& Color);
	int32 AddSprite(const FIntPoint& Cell, const TCHAR* AssetName, float Z, float MaxSize,
		const FLinearColor& Tint, bool bVisible = true);
	int32 AddPortrait(const FIntPoint& Cell, UTexture2D* Texture, const FLinearColor& FrameColor);
	void AddCellFrame(const FIntPoint& Cell, const FLinearColor& Color);
	void AddPatrolOutline(const FIntPoint& MinCell, const FIntPoint& MaxCell,
		const FLinearColor& Color);
	void AddMechanicVisual(const FNaiwaP3M2MechanicPlacement& Placement);
	void SetComponentVisible(int32 ComponentIndex, bool bVisible);
	void SetComponentColor(int32 ComponentIndex, const FLinearColor& Color);
	void SetComponentBrightness(int32 ComponentIndex, float Brightness);
	FVector GridToWorld(const FIntPoint& Cell, float Z = 0.0f) const;
 static FLinearColor LootColor(ENaiwaP3LootType Loot);

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Components;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
	TArray<FLinearColor> ComponentColors;
	TArray<float> ComponentBrightness;

	TMap<FIntPoint, TArray<int32>> EggVisuals;
	TMap<FIntPoint, TArray<int32>> ItemVisuals;
	TArray<FMechanicVisual> MechanicVisuals;
	TArray<int32> ExitComponents;
	TArray<int32> SlowSplashComponents;
	int32 ExitDoorComponent = INDEX_NONE;
	int32 GridWidth = FNaiwaP3M2SeededLevel::Width;
	int32 GridHeight = FNaiwaP3M2SeededLevel::Height;
};
