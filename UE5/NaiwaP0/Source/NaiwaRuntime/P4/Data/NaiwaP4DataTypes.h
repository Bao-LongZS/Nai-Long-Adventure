#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NaiwaP4DataTypes.generated.h"

class UTexture2D;

/** Table-driven enemy archetype data consumed by the deterministic W3 enemy brain. */
USTRUCT(BlueprintType)
struct FNaiwaP4EnemyRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="1"))
	int32 MaxHP = 36;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="1"))
	int32 Damage = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="0"))
	float MoveSpeed = 132.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="1"))
	float AttackRange = 72.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="0.05"))
	float AttackIntervalSeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="0.05"))
	float TelegraphSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy", meta=(ClampMin="1"))
	float CollisionRadius = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Enemy")
	bool bFrontShield75 = false;
};

/** One data row in the ordered wave spawn plan; repeated rows compose a wave. */
USTRUCT(BlueprintType)
struct FNaiwaP4WaveSpawnRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Wave", meta=(ClampMin="1"))
	int32 WaveIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Wave", meta=(ClampMin="0"))
	int32 SpawnOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Wave")
	FName EnemyTypeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Wave", meta=(ClampMin="1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Wave", meta=(ClampMin="0"))
	float SpawnIntervalSeconds = 0.55f;
};

/** Upgrade row shape used by the W1 import probe and the P4 upgrade registry. */
USTRUCT(BlueprintType)
struct FNaiwaP4UpgradeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FText TriggerRule;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FText ComboRule;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FName CategoryId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FText CategoryLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FName EffectKind;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	float P0 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	float P1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	float P2 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	bool bUnique = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	TArray<FName> PrerequisiteCardIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	TArray<FName> ExcludedCardIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FName RedlineGroup;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	FName Rarity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	TSoftObjectPtr<UTexture2D> IconSoftRef;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Upgrade")
	TSoftObjectPtr<UTexture2D> CategoryMarkSoftRef;
};

/** Runtime presentation metadata generated from the approved A1 import manifest. */
USTRUCT(BlueprintType)
struct FNaiwaP4A1FrameRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Animation")
	FName ActionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Animation")
	FName PoseId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Animation")
	FName ViewId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Animation")
	TSoftObjectPtr<UTexture2D> FrameTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Animation", meta=(ClampMin="0", ClampMax="723"))
	int32 FootAnchorYPx = 662;
};

/** One transform-only W4 effect preview frame and its authored timing metadata. */
USTRUCT(BlueprintType)
struct FNaiwaP4VfxFrameRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|VFX")
	FName EffectId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|VFX", meta=(ClampMin="0"))
	int32 FrameIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|VFX", meta=(ClampMin="0.001"))
	float DurationSeconds = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|VFX")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|VFX")
	TSoftObjectPtr<UTexture2D> FrameTexture;
};

/** One W4 Boss action preview frame. This table remains art-preview data, not gameplay state. */
USTRUCT(BlueprintType)
struct FNaiwaP4BossFrameRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Boss Art")
	FName StateId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Boss Art")
	FName ViewId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Boss Art", meta=(ClampMin="0"))
	int32 FrameIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Boss Art", meta=(ClampMin="0.001"))
	float DurationSeconds = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Boss Art")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="P4|Boss Art")
	TSoftObjectPtr<UTexture2D> FrameTexture;
};
