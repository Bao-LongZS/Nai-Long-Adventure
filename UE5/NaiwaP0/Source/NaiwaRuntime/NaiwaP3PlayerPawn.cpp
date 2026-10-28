#include "NaiwaP3PlayerPawn.h"

#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NaiwaP3GridSubsystem.h"
#include "NaiwaP3M2GameMode.h"
#include "NaiwaP3M2RuntimeSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ANaiwaP3PlayerPawn::ANaiwaP3PlayerPawn()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	bFindCameraComponentWhenViewTarget = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));

	ShadowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShadowMesh"));
	ShadowMesh->SetupAttachment(SceneRoot);
	if (CylinderFinder.Succeeded()) ShadowMesh->SetStaticMesh(CylinderFinder.Object);
	ShadowMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
	ShadowMesh->SetRelativeScale3D(FVector(0.31f, 0.19f, 0.012f));
	ShadowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShadowMesh->SetCastShadow(false);
	if (UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Naiwa/P3/Materials/M_P3M1FlatColor.M_P3M1FlatColor")))
	{
		if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, this))
		{
			Material->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(FColor(23, 31, 39)));
			ShadowMesh->SetMaterial(0, Material);
		}
	}
	const FName FrameNames[] = { TEXT("CellFrameNorth"), TEXT("CellFrameSouth"),
		TEXT("CellFrameWest"), TEXT("CellFrameEast") };
	const FVector FrameLocations[] = { FVector(0.0f, -44.0f, 4.0f), FVector(0.0f, 44.0f, 4.0f),
		FVector(-44.0f, 0.0f, 4.0f), FVector(44.0f, 0.0f, 4.0f) };
	const FVector FrameScales[] = { FVector(0.40f, 0.025f, 0.008f), FVector(0.40f, 0.025f, 0.008f),
		FVector(0.025f, 0.40f, 0.008f), FVector(0.025f, 0.40f, 0.008f) };
	const FLinearColor FrameColor = FLinearColor::FromSRGBColor(FColor(78, 208, 182));
	UMaterialInterface* FlatColorParent = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Naiwa/P3/Materials/M_P3M1FlatColor.M_P3M1FlatColor"));
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FrameNames); ++Index)
	{
		UStaticMeshComponent* Frame = CreateDefaultSubobject<UStaticMeshComponent>(FrameNames[Index]);
		Frame->SetupAttachment(SceneRoot);
		if (CubeFinder.Succeeded()) Frame->SetStaticMesh(CubeFinder.Object);
		Frame->SetRelativeLocation(FrameLocations[Index]);
		Frame->SetRelativeScale3D(FrameScales[Index]);
		Frame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Frame->SetCastShadow(false);
		UMaterialInstanceDynamic* FrameMaterial = FlatColorParent ?
			UMaterialInstanceDynamic::Create(FlatColorParent, this) : nullptr;
		if (FrameMaterial)
		{
			FrameMaterial->SetVectorParameterValue(TEXT("Color"), FrameColor);
			Frame->SetMaterial(0, FrameMaterial);
			CellFrameMaterials.Add(FrameMaterial);
		}
		CellFrameComponents.Add(Frame);
	}

	PortraitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortraitMesh"));
	PortraitMesh->SetupAttachment(SceneRoot);
	if (PlaneFinder.Succeeded()) PortraitMesh->SetStaticMesh(PlaneFinder.Object);
	PortraitMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 14.0f));
	PortraitMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PortraitMesh->SetGenerateOverlapEvents(false);
	PortraitMesh->SetCastShadow(false);
	PortraitMesh->SetVisibility(false);
}

void ANaiwaP3PlayerPawn::BeginPlay()
{
	Super::BeginPlay();
	NeutralPortrait = LoadObject<UTexture2D>(nullptr,
		TEXT("/Game/Naiwa/P3/Portraits/Processed/SPR_Player_Neutral_A.SPR_Player_Neutral_A"));
	HitPortrait = LoadObject<UTexture2D>(nullptr,
		TEXT("/Game/Naiwa/P3/Portraits/Processed/SPR_Player_Hit_A.SPR_Player_Hit_A"));
	SetPortrait(NeutralPortrait);
	M2RuntimeSubsystem = GetWorld()->GetSubsystem<UNaiwaP3M2RuntimeSubsystem>();
	if (M2RuntimeSubsystem.IsValid() && M2RuntimeSubsystem->GetSession())
	{
		M2RuntimeSubsystem->RegisterPlayerPawn(this);
		return;
	}
	GridSubsystem = GetWorld()->GetSubsystem<UNaiwaP3GridSubsystem>();
	if (GridSubsystem.IsValid())
	{
		GridSubsystem->RegisterPlayerPawn(this);
	}
}

void ANaiwaP3PlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!PlayerInputComponent)
	{
		return;
	}
	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &ANaiwaP3PlayerPawn::MoveUpPressed);
	PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &ANaiwaP3PlayerPawn::MoveUpReleased);
	PlayerInputComponent->BindKey(EKeys::Up, IE_Pressed, this, &ANaiwaP3PlayerPawn::MoveUpPressed);
	PlayerInputComponent->BindKey(EKeys::Up, IE_Released, this, &ANaiwaP3PlayerPawn::MoveUpReleased);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &ANaiwaP3PlayerPawn::MoveDownPressed);
	PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &ANaiwaP3PlayerPawn::MoveDownReleased);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Pressed, this, &ANaiwaP3PlayerPawn::MoveDownPressed);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Released, this, &ANaiwaP3PlayerPawn::MoveDownReleased);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed, this, &ANaiwaP3PlayerPawn::MoveLeftPressed);
	PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &ANaiwaP3PlayerPawn::MoveLeftReleased);
	PlayerInputComponent->BindKey(EKeys::Left, IE_Pressed, this, &ANaiwaP3PlayerPawn::MoveLeftPressed);
	PlayerInputComponent->BindKey(EKeys::Left, IE_Released, this, &ANaiwaP3PlayerPawn::MoveLeftReleased);
	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed, this, &ANaiwaP3PlayerPawn::MoveRightPressed);
	PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &ANaiwaP3PlayerPawn::MoveRightReleased);
	PlayerInputComponent->BindKey(EKeys::Right, IE_Pressed, this, &ANaiwaP3PlayerPawn::MoveRightPressed);
	PlayerInputComponent->BindKey(EKeys::Right, IE_Released, this, &ANaiwaP3PlayerPawn::MoveRightReleased);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ANaiwaP3PlayerPawn::PlaceBombPressed);
	PlayerInputComponent->BindKey(EKeys::J, IE_Pressed, this, &ANaiwaP3PlayerPawn::PlaceBombPressed);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &ANaiwaP3PlayerPawn::ChickenJumpPressed);
	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &ANaiwaP3PlayerPawn::SelectStandardBomb);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ANaiwaP3PlayerPawn::SelectSoyBomb);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ANaiwaP3PlayerPawn::SelectTeaBomb);
	PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ANaiwaP3PlayerPawn::SelectRatBomb);
	PlayerInputComponent->BindKey(EKeys::T, IE_Pressed, this, &ANaiwaP3PlayerPawn::StageChainDrillPressed);
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &ANaiwaP3PlayerPawn::ResetPressed);
	PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ANaiwaP3PlayerPawn::ReturnToLevelSelect);
}

void ANaiwaP3PlayerPawn::ReturnToLevelSelect()
{
	if (GetWorld())
	{
		UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Naiwa/UI/L_NaiwaMainMenu")),
			true, TEXT("NaiwaMenuPage=levels"));
	}
}

void ANaiwaP3PlayerPawn::SetGridWorldPosition(const FVector& Position)
{
	SetActorLocation(Position);
}

void ANaiwaP3PlayerPawn::SetDamageFeedback(const float Amount, const bool bShielded)
{
	SetPortrait(Amount > 0.0f && HitPortrait ? HitPortrait.Get() : NeutralPortrait.Get());
	const FLinearColor AlertColor = bShielded ?
		FLinearColor::FromSRGBColor(FColor(93, 218, 239)) :
		FLinearColor::FromSRGBColor(FColor(255, 83, 63));
	const FLinearColor FrameColor = FMath::Lerp(DefaultFrameColor, AlertColor,
		FMath::Clamp(Amount, 0.0f, 1.0f));
	for (const TObjectPtr<UMaterialInstanceDynamic>& Material : CellFrameMaterials)
	{
		if (Material) Material->SetVectorParameterValue(TEXT("Color"), FrameColor);
	}
}

void ANaiwaP3PlayerPawn::SetPortrait(UTexture2D* Portrait)
{
	if (!Portrait || !PortraitMesh) return;
	if (ActivePortrait == Portrait) return;
	if (!PortraitMaterial)
	{
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Naiwa/P3/Materials/M_P3Portrait.M_P3Portrait"));
		PortraitMaterial = Parent ? UMaterialInstanceDynamic::Create(Parent, this) : nullptr;
	}
	if (!PortraitMaterial) return;
	PortraitMaterial->SetTextureParameterValue(TEXT("Portrait"), Portrait);
	PortraitMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
	PortraitMesh->SetMaterial(0, PortraitMaterial);
	const float Height = 76.0f;
	const float Aspect = static_cast<float>(Portrait->GetSizeX()) /
		FMath::Max(static_cast<float>(Portrait->GetSizeY()), 1.0f);
	PortraitMesh->SetRelativeScale3D(FVector(FMath::Min(72.0f, Height * Aspect) / 100.0f,
		Height / 100.0f, 1.0f));
	PortraitMesh->SetVisibility(true);
	ActivePortrait = Portrait;
}

void ANaiwaP3PlayerPawn::SetDirection(const FIntPoint& Direction)
{
	InputDirection = Direction;
	if (GridSubsystem.IsValid())
	{
		GridSubsystem->SetPlayerDirection(Direction);
	}
	if (M2RuntimeSubsystem.IsValid() && M2RuntimeSubsystem->GetSession())
	{
		M2RuntimeSubsystem->SetPlayerDirection(Direction);
	}
}

void ANaiwaP3PlayerPawn::ClearDirection(const FIntPoint& Direction)
{
	if (InputDirection == Direction)
	{
		SetDirection(FIntPoint::ZeroValue);
	}
}

void ANaiwaP3PlayerPawn::MoveUpPressed() { SetDirection(FIntPoint(0, -1)); }
void ANaiwaP3PlayerPawn::MoveUpReleased() { ClearDirection(FIntPoint(0, -1)); }
void ANaiwaP3PlayerPawn::MoveDownPressed() { SetDirection(FIntPoint(0, 1)); }
void ANaiwaP3PlayerPawn::MoveDownReleased() { ClearDirection(FIntPoint(0, 1)); }
void ANaiwaP3PlayerPawn::MoveLeftPressed() { SetDirection(FIntPoint(-1, 0)); }
void ANaiwaP3PlayerPawn::MoveLeftReleased() { ClearDirection(FIntPoint(-1, 0)); }
void ANaiwaP3PlayerPawn::MoveRightPressed() { SetDirection(FIntPoint(1, 0)); }
void ANaiwaP3PlayerPawn::MoveRightReleased() { ClearDirection(FIntPoint(1, 0)); }

void ANaiwaP3PlayerPawn::PlaceBombPressed()
{
	if (M2RuntimeSubsystem.IsValid() && M2RuntimeSubsystem->GetSession())
	{
		M2RuntimeSubsystem->PlacePlayerBomb();
		return;
	}
	if (GridSubsystem.IsValid())
	{
		GridSubsystem->PlacePlayerBomb();
	}
}

void ANaiwaP3PlayerPawn::ChickenJumpPressed()
{
	if (M2RuntimeSubsystem.IsValid() && M2RuntimeSubsystem->GetSession() &&
		InputDirection != FIntPoint::ZeroValue)
	{
		FString Message;
		M2RuntimeSubsystem->TryChickenJump(InputDirection, Message);
	}
}

void ANaiwaP3PlayerPawn::SelectStandardBomb()
{
	if (M2RuntimeSubsystem.IsValid() && M2RuntimeSubsystem->GetSession())
		M2RuntimeSubsystem->SelectPlayerBombType(ENaiwaP3BombType::Standard);
	if (GridSubsystem.IsValid()) GridSubsystem->SelectPlayerBombType(ENaiwaP3BombType::Standard);
}

void ANaiwaP3PlayerPawn::SelectSoyBomb()
{
	if (M2RuntimeSubsystem.IsValid() && M2RuntimeSubsystem->GetSession())
		M2RuntimeSubsystem->SelectPlayerBombType(ENaiwaP3BombType::SoyBigCross);
	if (GridSubsystem.IsValid()) GridSubsystem->SelectPlayerBombType(ENaiwaP3BombType::SoyBigCross);
}

void ANaiwaP3PlayerPawn::SelectTeaBomb()
{
	if (M2RuntimeSubsystem.IsValid() && M2RuntimeSubsystem->GetSession())
		M2RuntimeSubsystem->SelectPlayerBombType(ENaiwaP3BombType::TeaRadial);
	if (GridSubsystem.IsValid()) GridSubsystem->SelectPlayerBombType(ENaiwaP3BombType::TeaRadial);
}

void ANaiwaP3PlayerPawn::SelectRatBomb()
{
	if (M2RuntimeSubsystem.IsValid() && M2RuntimeSubsystem->GetSession())
		M2RuntimeSubsystem->SelectPlayerBombType(ENaiwaP3BombType::RatSplit);
	if (GridSubsystem.IsValid()) GridSubsystem->SelectPlayerBombType(ENaiwaP3BombType::RatSplit);
}

void ANaiwaP3PlayerPawn::StageChainDrillPressed()
{
	InputDirection = FIntPoint::ZeroValue;
	if (GridSubsystem.IsValid())
	{
		GridSubsystem->StageChainDrill();
	}
}

void ANaiwaP3PlayerPawn::ResetPressed()
{
	InputDirection = FIntPoint::ZeroValue;
	if (M2RuntimeSubsystem.IsValid() && M2RuntimeSubsystem->GetSession())
	{
		if (GetWorld())
		{
			if (ANaiwaP3M2GameMode* M2GameMode = Cast<ANaiwaP3M2GameMode>(GetWorld()->GetAuthGameMode()))
			{
				M2GameMode->RestartSession();
			}
		}
		return;
	}
	if (GridSubsystem.IsValid())
	{
		GridSubsystem->ResetM1();
	}
}
