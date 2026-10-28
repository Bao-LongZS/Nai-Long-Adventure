#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NaiwaP4DataTypes.generated.h"

class UTexture2D;

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
