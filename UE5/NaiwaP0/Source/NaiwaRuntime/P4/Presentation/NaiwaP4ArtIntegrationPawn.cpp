#include "NaiwaP4ArtIntegrationPawn.h"

#include "../Core/NaiwaP4ArenaState.h"
#include "../Core/NaiwaP4ArenaSubsystem.h"
#include "../Data/NaiwaP4DataTypes.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* FrameCatalogPath = TEXT("/Game/Naiwa/P4/Data/DT_P4_A1_RuntimeFrames.DT_P4_A1_RuntimeFrames");
	const TCHAR* SpriteMaterialPath = TEXT("/Game/Naiwa/P4/Materials/Alpha/A1/M_P4_A1_SpriteRuntime.M_P4_A1_SpriteRuntime");
	const TCHAR* ShadowMaterialPath = TEXT("/Game/Naiwa/P4/Materials/W1/M_P4_W1_SoftShadow_RadialV6.M_P4_W1_SoftShadow_RadialV6");
	const TCHAR* FrameTextureParameter = TEXT("P4FrameTexture");
	constexpr int32 MoveFrameTicks = 8;
	constexpr int32 TwoFrameActionTicks = 7;
}

ANaiwaP4ArtIntegrationPawn::ANaiwaP4ArtIntegrationPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("P4ArtPreviewRoot"));
	RootComponent = SceneRoot;

	SpritePlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("P4PlayerSprite"));
	SpritePlane->SetupAttachment(SceneRoot);
	SpritePlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpritePlane->SetCastShadow(false);
	SpritePlane->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
	SpritePlane->SetRelativeScale3D(FVector(1.8f, 1.8f, 1.0f));

	ContactShadow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("P4PlayerContactShadow"));
	ContactShadow->SetupAttachment(SceneRoot);
	ContactShadow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ContactShadow->SetCastShadow(false);
	ContactShadow->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
	ContactShadow->SetRelativeScale3D(FVector(1.35f, 0.48f, 1.0f));
}

void ANaiwaP4ArtIntegrationPawn::BeginPlay()
{
	Super::BeginPlay();

	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane)
	{
		SpritePlane->SetStaticMesh(Plane);
		ContactShadow->SetStaticMesh(Plane);
	}

	if (UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, SpriteMaterialPath))
	{
		SpriteMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		SpritePlane->SetMaterial(0, SpriteMaterial);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("P4 art preview could not load the approved A1 sprite material: %s"), SpriteMaterialPath);
	}

	if (UMaterialInterface* Shadow = LoadObject<UMaterialInterface>(nullptr, ShadowMaterialPath))
	{
		ContactShadow->SetMaterial(0, Shadow);
	}
	else
	{
		ContactShadow->SetVisibility(false);
		UE_LOG(LogTemp, Warning, TEXT("P4 art preview is missing the calibrated W1 soft-shadow material."));
	}

	LoadFrameCatalog();
	SyncFromSimulation();
}

void ANaiwaP4ArtIntegrationPawn::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SyncFromSimulation();
}

void ANaiwaP4ArtIntegrationPawn::LoadFrameCatalog()
{
	FrameCatalog = LoadObject<UDataTable>(nullptr, FrameCatalogPath);
	if (!FrameCatalog || FrameCatalog->GetRowStruct() != FNaiwaP4A1FrameRow::StaticStruct())
	{
		UE_LOG(LogTemp, Error, TEXT("P4 A1 animation DataTable is missing or has the wrong row struct: %s"), FrameCatalogPath);
		return;
	}

	for (const FName RowName : FrameCatalog->GetRowNames())
	{
		const FNaiwaP4A1FrameRow* Row = FrameCatalog->FindRow<FNaiwaP4A1FrameRow>(RowName, TEXT("P4 A1 frame catalog load"), false);
		UTexture2D* Texture = Row ? Row->FrameTexture.LoadSynchronous() : nullptr;
		if (Row && Texture)
		{
			LoadedFrames.Add(RowName, Texture);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("P4 A1 frame '%s' is absent or failed to load."), *RowName.ToString());
		}
	}

	UE_LOG(LogTemp, Display, TEXT("P4 A1 art presenter loaded %d/%d approved frames."), LoadedFrames.Num(), FrameCatalog->GetRowNames().Num());
}

void ANaiwaP4ArtIntegrationPawn::SyncFromSimulation()
{
	UWorld* World = GetWorld();
	UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	if (!Arena || !FrameCatalog)
	{
		return;
	}

	const FNaiwaP4EntityState* Player = Arena->GetState().Entities.FindByPredicate([](const FNaiwaP4EntityState& Entity)
	{
		return Entity.Kind == ENaiwaP4EntityKind::Player;
	});
	if (!Player || !Player->bAlive)
	{
		return;
	}

	SetActorLocation(FVector(Player->Position.X, Player->Position.Y, Player->HeightZ));
	ApplyFrame(ResolveFrameId(*Player), Player->bFacingRight);
}

FName ANaiwaP4ArtIntegrationPawn::ResolveFrameId(const FNaiwaP4EntityState& Player) const
{
	FString Action;
	FString Pose;
	switch (Player.ActionCue)
	{
	case ENaiwaP4ActionCue::Move:
		Action = TEXT("move");
		Pose = ((Player.ActionTick / MoveFrameTicks) % 2 == 0) ? TEXT("step_a") : TEXT("step_b");
		break;
	case ENaiwaP4ActionCue::Attack:
		Action = TEXT("attack");
		Pose = (Player.ActionTick < TwoFrameActionTicks) ? TEXT("windup") : TEXT("contact");
		break;
	case ENaiwaP4ActionCue::Dash:
		Action = TEXT("dash");
		Pose = (Player.ActionTick < TwoFrameActionTicks) ? TEXT("launch") : TEXT("stride");
		break;
	case ENaiwaP4ActionCue::Defend:
		Action = TEXT("defend");
		Pose = TEXT("guard");
		break;
	case ENaiwaP4ActionCue::Hit:
		Action = TEXT("hit");
		Pose = TEXT("recoil");
		break;
	case ENaiwaP4ActionCue::Laugh:
		Action = TEXT("laugh");
		Pose = ((Player.ActionTick / TwoFrameActionTicks) % 2 == 0) ? TEXT("open") : TEXT("recover");
		break;
	case ENaiwaP4ActionCue::Death:
	case ENaiwaP4ActionCue::Idle:
	default:
		Action = TEXT("idle");
		Pose = TEXT("neutral");
		break;
	}

	FString View = TEXT("front");
	if (Player.Facing == ENaiwaP4Facing::Back)
	{
		View = TEXT("back");
	}
	else if (Player.Facing == ENaiwaP4Facing::Side)
	{
		View = TEXT("side_right");
	}
	return FName(*FString::Printf(TEXT("%s_%s_%s"), *Action, *Pose, *View));
}

void ANaiwaP4ArtIntegrationPawn::ApplyFrame(const FName FrameId, const bool bFacingRight)
{
	if (!SpriteMaterial || !FrameCatalog)
	{
		return;
	}

	const FNaiwaP4A1FrameRow* Row = FrameCatalog->FindRow<FNaiwaP4A1FrameRow>(FrameId, TEXT("P4 A1 frame selection"), false);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("P4 A1 frame catalog has no entry for '%s'."), *FrameId.ToString());
		return;
	}

	UTexture2D* Texture = LoadedFrames.FindRef(FrameId);
	if (!Texture)
	{
		Texture = Row->FrameTexture.LoadSynchronous();
	}
	if (!Texture)
	{
		return;
	}
	if (CurrentFrameId != FrameId)
	{
		SpriteMaterial->SetTextureParameterValue(FrameTextureParameter, Texture);
		CurrentFrameId = FrameId;
	}

	const float AnchorY = static_cast<float>(Row->FootAnchorYPx);
	const float AnchorCenterZ = ((AnchorY / 724.0f) - 0.5f) * SpriteHeightUU;
	SpritePlane->SetRelativeLocation(FVector(0.0f, 0.0f, AnchorCenterZ));
	SpritePlane->SetRelativeScale3D(FVector(bFacingRight ? 1.8f : -1.8f, 1.8f, 1.0f));
}
