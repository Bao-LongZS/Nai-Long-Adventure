#pragma once

#include "CoreMinimal.h"

/** Authoring row for one visible clue card. The answer key is deliberately not stored here. */
struct FNaiwaP6ClueDefinition
{
	FName ClueId;
	FString Title;
	FString Body;
	FString Timestamp;
	FString Source;
};

/** Authoring row for one point in the runtime graybox. */
struct FNaiwaP6InteractableDefinition
{
	FName PointId;
	FString DisplayName;
	FVector Location = FVector::ZeroVector;
	FVector Scale = FVector(1.0f);
	FLinearColor Color = FLinearColor::White;
	FName ClueId = NAME_None;
	bool bM2Required = false;
	FString InteractionText;
	FString HintText;
};

struct FNaiwaP6AnswerOption
{
	FName AnswerId;
	FString Label;
};

/** One local mesh mark used to visualize a skin-specific evidence layer. */
struct FNaiwaP6InfoLayerMark
{
	/** Centimeter offset from the owning interactable's center, in actor-local axes. */
	FVector LocalOffsetCm = FVector::ZeroVector;
	/** Scale factor for the engine's 100 cm cube mesh in local axes. */
	FVector MeshScale = FVector(1.0f);
	FRotator Rotation = FRotator::ZeroRotator;
};

/** Skin-gated reconstruction data; the clue text remains in the clue fixture. */
struct FNaiwaP6InfoLayerDefinition
{
	FName ClueId = NAME_None;
	FName SkinId = NAME_None;
	FName PointId = NAME_None;
	FLinearColor Color = FLinearColor::White;
	TArray<FNaiwaP6InfoLayerMark> Marks;
};

/** Only the two explicitly authored evidence combinations are permitted. */
struct FNaiwaP6CrossClueDefinition
{
	FName ClueId = NAME_None;
	FName PointId = NAME_None;
	TArray<FName> RequiredClueIds;
};
