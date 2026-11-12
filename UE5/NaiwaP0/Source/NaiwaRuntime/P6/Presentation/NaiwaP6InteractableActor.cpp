#include "NaiwaP6InteractableActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "P6/Core/NaiwaP6InvestigationSubsystem.h"
#include "P6/Data/NaiwaP6CaseDefinition.h"
#include "P5/Data/NaiwaP5SkinCatalog.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UMaterialInstanceDynamic* ApplyColor(UStaticMeshComponent* Component,
		const FLinearColor& Color, UObject* Outer)
	{
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Component || !Parent)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, Outer);
		if (Material)
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			Component->SetMaterial(0, Material);
		}
		return Material;
	}
}

ANaiwaP6InteractableActor::ANaiwaP6InteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		CubeMesh = CubeFinder.Object;
	}

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("InvestigationPointRoot"));
	SetRootComponent(SceneRoot);
	MainMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InteractableMesh"));
	MainMesh->SetupAttachment(SceneRoot);
	if (CubeMesh) MainMesh->SetStaticMesh(CubeMesh);
	MainMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MainMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	MainMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	MainMesh->SetGenerateOverlapEvents(false);
	MainMesh->SetCastShadow(false);

}

void ANaiwaP6InteractableActor::InitializePoint(const FName InPointId)
{
	PointId = InPointId;
	const FNaiwaP6InteractableDefinition* Definition = FNaiwaP6CaseDefinition::FindInteractable(PointId);
	if (!Definition)
	{
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		return;
	}

	SetActorLocation(Definition->Location);
	MainMesh->SetRelativeScale3D(Definition->Scale);
	ApplyColor(MainMesh, Definition->Color, this);
	for (const FNaiwaP6InfoLayerDefinition& LayerDefinition : FNaiwaP6CaseDefinition::GetM3InfoLayers())
	{
		if (LayerDefinition.PointId != PointId)
		{
			continue;
		}
		for (int32 Index = 0; Index < LayerDefinition.Marks.Num(); ++Index)
		{
			const FNaiwaP6InfoLayerMark& Mark = LayerDefinition.Marks[Index];
			UStaticMeshComponent* LayerMesh = NewObject<UStaticMeshComponent>(this,
				MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(),
					FName(*FString::Printf(TEXT("Info_%s_%d"), *LayerDefinition.ClueId.ToString(), Index))));
			if (!LayerMesh)
			{
				continue;
			}
			AddInstanceComponent(LayerMesh);
			LayerMesh->SetupAttachment(SceneRoot);
			if (CubeMesh)
			{
				LayerMesh->SetStaticMesh(CubeMesh);
			}
			LayerMesh->SetRelativeLocation(Mark.LocalOffsetCm);
			LayerMesh->SetRelativeRotation(Mark.Rotation);
			LayerMesh->SetRelativeScale3D(Mark.MeshScale);
			LayerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			LayerMesh->SetCastShadow(false);
			LayerMesh->SetVisibility(false);
			ApplyColor(LayerMesh, LayerDefinition.Color, this);
			LayerMesh->RegisterComponent();
			InfoLayerMeshes.Add(LayerMesh);
			InfoLayerMeshClueIds.Add(LayerDefinition.ClueId);
		}
	}
}

FName ANaiwaP6InteractableActor::GetClueId() const
{
	if (const FNaiwaP6InteractableDefinition* Definition = FNaiwaP6CaseDefinition::FindInteractable(PointId))
	{
		return Definition->ClueId;
	}
	return NAME_None;
}

FString ANaiwaP6InteractableActor::GetDisplayName() const
{
	if (const FNaiwaP6InteractableDefinition* Definition = FNaiwaP6CaseDefinition::FindInteractable(PointId))
	{
		return Definition->DisplayName;
	}
	return TEXT("调查点");
}

FString ANaiwaP6InteractableActor::GetInfoLayerDescription(const FName SkinId) const
{
	if (const FNaiwaP6InfoLayerDefinition* Definition = FNaiwaP6CaseDefinition::FindInfoLayer(PointId, SkinId))
	{
		if (const FNaiwaP6ClueDefinition* Clue = FNaiwaP6CaseDefinition::FindClue(Definition->ClueId))
		{
			return Clue->Body;
		}
	}
	return FString();
}

bool ANaiwaP6InteractableActor::HasInfoLayer() const
{
	return FNaiwaP6CaseDefinition::GetM3InfoLayers().ContainsByPredicate(
		[this](const FNaiwaP6InfoLayerDefinition& Definition) { return Definition.PointId == PointId; });
}

bool ANaiwaP6InteractableActor::HasInfoLayer(const FName SkinId) const
{
	return FNaiwaP6CaseDefinition::FindInfoLayer(PointId, SkinId) != nullptr;
}

FString ANaiwaP6InteractableActor::ToggleInfoLayer(const FName CurrentSkinId)
{
	const FNaiwaP6InfoLayerDefinition* Layer = FNaiwaP6CaseDefinition::FindInfoLayer(PointId, CurrentSkinId);
	if (!Layer)
	{
		TArray<FString> AvailableSkinNames;
		for (const FNaiwaP6InfoLayerDefinition& Candidate : FNaiwaP6CaseDefinition::GetM3InfoLayers())
		{
			if (Candidate.PointId != PointId)
			{
				continue;
			}
			if (const FNaiwaP5SkinRow* Skin = FNaiwaP5SkinCatalog::GetRows().FindByPredicate(
				[&Candidate](const FNaiwaP5SkinRow& Row) { return Row.SkinId == Candidate.SkinId; }))
			{
				AvailableSkinNames.AddUnique(Skin->DisplayName);
			}
		}
		if (!AvailableSkinNames.IsEmpty())
		{
			return FString::Printf(TEXT("这里的信息层可由 %s 查看；按 Tab 切换后再按 F。"),
				*FString::Join(AvailableSkinNames, TEXT("、")));
		}
		return TEXT("附近没有可查看的信息层。到戏台边的奖杯调查点按 F。");
	}

	if (bInfoLayerVisible && ActiveInfoLayerClueId == Layer->ClueId)
	{
		HideInfoLayer();
		return TEXT("已关闭当前局部信息层；已收集的证据卡仍保留。");
	}

	HideInfoLayer();
	ActiveInfoLayerClueId = Layer->ClueId;
	bInfoLayerVisible = true;
	for (int32 Index = 0; Index < InfoLayerMeshes.Num(); ++Index)
	{
		if (InfoLayerMeshes[Index] && InfoLayerMeshClueIds.IsValidIndex(Index))
		{
			InfoLayerMeshes[Index]->SetVisibility(InfoLayerMeshClueIds[Index] == ActiveInfoLayerClueId);
		}
	}

	UNaiwaP6InvestigationSubsystem* Investigation = GetWorld()
		? GetWorld()->GetSubsystem<UNaiwaP6InvestigationSubsystem>() : nullptr;
	const FNaiwaP6ClueDefinition* Clue = FNaiwaP6CaseDefinition::FindClue(Layer->ClueId);
	const FString ClueBody = Clue ? Clue->Body : TEXT("局部痕迹已显示。");
	if (!Investigation)
	{
		return ClueBody;
	}
	const bool bAlreadyCollected = Investigation->HasClue(Layer->ClueId);
	if (!bAlreadyCollected)
	{
		Investigation->CollectClue(Layer->ClueId, PointId);
	}
	const FString CollectionState = bAlreadyCollected
		? TEXT("证据卡已在案卷中。") : FString::Printf(TEXT("已收集 %s，符合条件的交叉记录会自动归档。"), *Layer->ClueId.ToString());
	return FString::Printf(TEXT("%s %s"), *ClueBody, *CollectionState);
}

bool ANaiwaP6InteractableActor::ToggleInfoLayer()
{
	const FName LegacyImpactSkinId(TEXT("NaiwaSkin_NaiSha"));
	if (!FNaiwaP6CaseDefinition::FindInfoLayer(PointId, LegacyImpactSkinId))
	{
		return false;
	}
	ToggleInfoLayer(LegacyImpactSkinId);
	return true;
}

void ANaiwaP6InteractableActor::HideInfoLayer()
{
	bInfoLayerVisible = false;
	ActiveInfoLayerClueId = NAME_None;
	for (const TObjectPtr<UStaticMeshComponent>& Layer : InfoLayerMeshes)
	{
		if (Layer)
		{
			Layer->SetVisibility(false);
		}
	}
}

FString ANaiwaP6InteractableActor::Interact()
{
	const FNaiwaP6InteractableDefinition* Definition = FNaiwaP6CaseDefinition::FindInteractable(PointId);
	if (!Definition)
	{
		return TEXT("这里暂时没有可检查的内容。");
	}
	const FName ClueId = Definition->ClueId;
	const FString& Dialogue = Definition->InteractionText;
	UNaiwaP6InvestigationSubsystem* Investigation = GetWorld()
		? GetWorld()->GetSubsystem<UNaiwaP6InvestigationSubsystem>() : nullptr;
	if (Investigation)
	{
		Investigation->RecordPointVisited(PointId);
	}

	if (ClueId.IsNone())
	{
		return Dialogue.IsEmpty() ? TEXT("这只是院子里的小发现，没有新增案件证据。") : Dialogue;
	}

	if (Investigation)
	{
		if (Investigation->HasClue(ClueId))
		{
			return Dialogue.IsEmpty() ? TEXT("这张证据卡已经收进线索板。")
				: FString::Printf(TEXT("这张证据卡已经收进线索板。%s"), *Dialogue);
		}
		if (Investigation->CollectClue(ClueId, PointId))
		{
			return Dialogue.IsEmpty()
				? FString::Printf(TEXT("已收集证据 %s。按 C 打开线索板。"), *ClueId.ToString())
				: FString::Printf(TEXT("已收集证据 %s。%s"), *ClueId.ToString(), *Dialogue);
		}
		return TEXT("暂时无法记录这项证据。");
	}
	return TEXT("调查记录暂不可用。");
}

