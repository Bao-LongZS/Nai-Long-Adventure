#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NaiwaP3M1Actors.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class UMaterialInstanceDynamic;
class UTexture2D;

UENUM()
enum class ENaiwaP3Tile : uint8
{
	Floor,
	Hard,
	Soft,
	Item
};

UCLASS()
class NAIWARUNTIME_API ANaiwaP3M1MapActor final : public AActor
{
	GENERATED_BODY()

public:
	ANaiwaP3M1MapActor();
	virtual void Tick(float DeltaTime) override;

	void BuildMap(const TArray<FString>& InRows, bool bBuildM1TestMarkers = true,
		bool bBuildFogOverlay = false);
	void SetTile(int32 X, int32 Y, ENaiwaP3Tile Tile);
	void SetFlame(int32 X, int32 Y, bool bVisible);
	void SetFogState(int32 X, int32 Y, bool bVisible, bool bRemembered);
	FVector CellCenter(int32 X, int32 Y, float Z = 0.0f) const;
	int32 GetColumns() const { return Columns; }
	int32 GetRows() const { return RowCount; }

private:
	UStaticMeshComponent* AddCube(const TCHAR* Name, const FVector& RelativeLocation,
		const FVector& Scale, const FLinearColor& Color, float Yaw = 0.0f);
	void ApplyTileVisual(int32 Index, ENaiwaP3Tile Tile);

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> FloorComponents;

	UPROPERTY()
	TArray<FLinearColor> FloorBaseColors;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> HardComponents;

	UPROPERTY()
	TArray<FLinearColor> HardBaseColors;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SoftBodyComponents;

	UPROPERTY()
	TArray<FLinearColor> SoftBaseColors;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SoftBraceAComponents;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SoftBraceBComponents;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SoftBaseComponents;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SoftGrainAComponents;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SoftGrainBComponents;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SoftStudComponents;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ItemLockComponents;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> FlameComponents;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> FlameCoreComponents;

	UPROPERTY()
	TArray<uint8> FlameActiveCells;

	UPROPERTY()
	TArray<uint8> FogVisibleCells;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> DetailComponents;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> FogUnseenComponents;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> FogMemoryNorthComponents;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> FogMemorySouthComponents;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> FogMemoryWestComponents;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> FogMemoryEastComponents;

	int32 Columns = 0;
	int32 RowCount = 0;
};

UCLASS()
class NAIWARUNTIME_API ANaiwaP3M1EnemyActor final : public AActor
{
	GENERATED_BODY()

public:
	ANaiwaP3M1EnemyActor();
	void SetGridPosition(const FVector& Position);
	void SetPortrait(UTexture2D* Portrait);

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ShadowMesh;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CellFrameComponents;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PortraitMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PortraitMaterial;

	UPROPERTY()
	TObjectPtr<UTexture2D> ActivePortrait;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MissingPortraitMarker;
};
