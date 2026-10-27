#include "NaiwaP3BombActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ANaiwaP3BombActor::ANaiwaP3BombActor()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(Root);
	if (CylinderFinder.Succeeded()) BaseMesh->SetStaticMesh(CylinderFinder.Object);
	BaseMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 8.0f));
	BaseMesh->SetRelativeScale3D(FVector(0.34f, 0.34f, 0.07f));
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BaseMesh->SetCastShadow(false);
	BaseMesh->SetMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));

	BombMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BombMesh"));
	BombMesh->SetupAttachment(Root);
	if (SphereFinder.Succeeded()) BombMesh->SetStaticMesh(SphereFinder.Object);
	BombMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 33.0f));
	BombMesh->SetRelativeScale3D(FVector(0.32f, 0.32f, 0.32f));
	BombMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BombMesh->SetCastShadow(false);

	FuseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FuseMesh"));
	FuseMesh->SetupAttachment(Root);
	if (CubeFinder.Succeeded()) FuseMesh->SetStaticMesh(CubeFinder.Object);
	FuseMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 56.0f));
	FuseMesh->SetRelativeScale3D(FVector(0.08f, 0.035f, 0.07f));
	FuseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FuseMesh->SetCastShadow(false);

	SparkMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SparkMesh"));
	SparkMesh->SetupAttachment(Root);
	if (SphereFinder.Succeeded()) SparkMesh->SetStaticMesh(SphereFinder.Object);
	SparkMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));
	SparkMesh->SetRelativeScale3D(FVector(0.065f, 0.065f, 0.065f));
	SparkMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SparkMesh->SetCastShadow(false);

	BombSpriteMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BombSpriteMesh"));
	BombSpriteMesh->SetupAttachment(Root);
	if (PlaneFinder.Succeeded()) BombSpriteMesh->SetStaticMesh(PlaneFinder.Object);
	BombSpriteMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 16.0f));
	BombSpriteMesh->SetRelativeScale3D(FVector(0.82f, 0.82f, 1.0f));
	BombSpriteMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BombSpriteMesh->SetGenerateOverlapEvents(false);
	BombSpriteMesh->SetCastShadow(false);
	BombSpriteMesh->SetVisibility(false);

	UStaticMeshComponent* SoyCrossHorizontal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SoyCrossHorizontal"));
	SoyCrossHorizontal->SetupAttachment(Root);
	if (CubeFinder.Succeeded()) SoyCrossHorizontal->SetStaticMesh(CubeFinder.Object);
	SoyCrossHorizontal->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	SoyCrossHorizontal->SetRelativeScale3D(FVector(0.31f, 0.055f, 0.04f));
	SoyCrossHorizontal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SoyCrossHorizontal->SetCastShadow(false);
	TypeMarkerComponents.Add(SoyCrossHorizontal);

	UStaticMeshComponent* SoyCrossVertical = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SoyCrossVertical"));
	SoyCrossVertical->SetupAttachment(Root);
	if (CubeFinder.Succeeded()) SoyCrossVertical->SetStaticMesh(CubeFinder.Object);
	SoyCrossVertical->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	SoyCrossVertical->SetRelativeScale3D(FVector(0.055f, 0.31f, 0.04f));
	SoyCrossVertical->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SoyCrossVertical->SetCastShadow(false);
	TypeMarkerComponents.Add(SoyCrossVertical);

	UStaticMeshComponent* TeaStraw = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TeaStraw"));
	TeaStraw->SetupAttachment(Root);
	if (CylinderFinder.Succeeded()) TeaStraw->SetStaticMesh(CylinderFinder.Object);
	TeaStraw->SetRelativeLocation(FVector(23.0f, 0.0f, 48.0f));
	TeaStraw->SetRelativeRotation(FRotator(0.0f, 0.0f, -18.0f));
	TeaStraw->SetRelativeScale3D(FVector(0.07f, 0.07f, 0.22f));
	TeaStraw->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TeaStraw->SetCastShadow(false);
	TypeMarkerComponents.Add(TeaStraw);

	UStaticMeshComponent* TeaPearl = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TeaPearl"));
	TeaPearl->SetupAttachment(Root);
	if (SphereFinder.Succeeded()) TeaPearl->SetStaticMesh(SphereFinder.Object);
	TeaPearl->SetRelativeLocation(FVector(-18.0f, 12.0f, 48.0f));
	TeaPearl->SetRelativeScale3D(FVector(0.09f, 0.09f, 0.09f));
	TeaPearl->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TeaPearl->SetCastShadow(false);
	TypeMarkerComponents.Add(TeaPearl);

	UStaticMeshComponent* RatSplitLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RatSplitLeft"));
	RatSplitLeft->SetupAttachment(Root);
	if (CubeFinder.Succeeded()) RatSplitLeft->SetStaticMesh(CubeFinder.Object);
	RatSplitLeft->SetRelativeLocation(FVector(-14.0f, 0.0f, 49.0f));
	RatSplitLeft->SetRelativeRotation(FRotator(0.0f, 35.0f, 0.0f));
	RatSplitLeft->SetRelativeScale3D(FVector(0.20f, 0.045f, 0.04f));
	RatSplitLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RatSplitLeft->SetCastShadow(false);
	TypeMarkerComponents.Add(RatSplitLeft);

	UStaticMeshComponent* RatSplitRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RatSplitRight"));
	RatSplitRight->SetupAttachment(Root);
	if (CubeFinder.Succeeded()) RatSplitRight->SetStaticMesh(CubeFinder.Object);
	RatSplitRight->SetRelativeLocation(FVector(14.0f, 0.0f, 49.0f));
	RatSplitRight->SetRelativeRotation(FRotator(0.0f, -35.0f, 0.0f));
	RatSplitRight->SetRelativeScale3D(FVector(0.20f, 0.045f, 0.04f));
	RatSplitRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RatSplitRight->SetCastShadow(false);
	TypeMarkerComponents.Add(RatSplitRight);
	for (const TObjectPtr<UStaticMeshComponent>& Marker : TypeMarkerComponents)
	{
		Marker->SetVisibility(false);
	}

	UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Naiwa/P3/Materials/M_P3M1FlatColor.M_P3M1FlatColor"));
	BombMaterial = ParentMaterial ? UMaterialInstanceDynamic::Create(ParentMaterial, this) : nullptr;
	SparkMaterial = ParentMaterial ? UMaterialInstanceDynamic::Create(ParentMaterial, this) : nullptr;
	TypeMarkerMaterial = ParentMaterial ? UMaterialInstanceDynamic::Create(ParentMaterial, this) : nullptr;
	UMaterialInterface* SpriteParent = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Naiwa/P3/Materials/M_P3Portrait.M_P3Portrait"));
	SpriteMaterial = SpriteParent ? UMaterialInstanceDynamic::Create(SpriteParent, this) : nullptr;
	if (SpriteMaterial) BombSpriteMesh->SetMaterial(0, SpriteMaterial);
	if (BombMaterial)
	{
		BombMaterial->SetVectorParameterValue(TEXT("Color"), GetNaiwaP3BombColor(BombType));
		BombMesh->SetMaterial(0, BombMaterial);
		FuseMesh->SetMaterial(0, BombMaterial);
		BaseMesh->SetMaterial(0, BombMaterial);
	}
	if (SparkMaterial)
	{
		SparkMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(FColor(255, 200, 90)));
		SparkMesh->SetMaterial(0, SparkMaterial);
	}
	if (TypeMarkerMaterial)
	{
		for (const TObjectPtr<UStaticMeshComponent>& Marker : TypeMarkerComponents)
		{
			Marker->SetMaterial(0, TypeMarkerMaterial);
		}
	}
}

void ANaiwaP3BombActor::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bVisualSliding)
	{
		VisualSlideElapsed = FMath::Min(VisualSlideDuration, VisualSlideElapsed + DeltaTime);
		const float Alpha = FMath::Clamp(VisualSlideElapsed / FMath::Max(VisualSlideDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
		SetActorLocation(FMath::Lerp(VisualSlideStart, VisualSlideEnd, Alpha));
		if (Alpha >= 1.0f) bVisualSliding = false;
	}
	if (!GetWorld()) return;
	const float Age = GetWorld()->GetTimeSeconds();
	const float Urgency = 1.0f - RemainingFraction;
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Age * (4.0f + 18.0f * Urgency));
	const float Scale = 1.0f + Urgency * Pulse * 0.24f;
	if (BombSpriteMesh && BombSpriteMesh->IsVisible())
	{
		BombSpriteMesh->SetRelativeScale3D(FVector(0.82f * Scale, 0.82f * Scale, 1.0f));
	}
	else
	{
		BombMesh->SetRelativeScale3D(FVector(0.32f * Scale, 0.32f * Scale, 0.32f * Scale));
		SparkMesh->SetRelativeScale3D(FVector(0.045f + Pulse * 0.04f, 0.045f + Pulse * 0.04f, 0.045f + Pulse * 0.04f));
	}
	if (SparkMaterial)
	{
		const FLinearColor SparkColor = FMath::Lerp(FLinearColor::FromSRGBColor(FColor(255, 200, 90)),
			FLinearColor::FromSRGBColor(FColor(255, 88, 36)), Urgency * Pulse);
		SparkMaterial->SetVectorParameterValue(TEXT("Color"), SparkColor);
	}
}

void ANaiwaP3BombActor::SetWorldPosition(const FVector& Position)
{
	bVisualSliding = false;
	SetActorLocation(Position);
}

void ANaiwaP3BombActor::SetFuseFraction(const float InRemainingFraction)
{
	RemainingFraction = FMath::Clamp(InRemainingFraction, 0.0f, 1.0f);
}

void ANaiwaP3BombActor::SetBombType(const ENaiwaP3BombType Type)
{
	BombType = Type;
	const TCHAR* SpriteName = nullptr;
	switch (BombType)
	{
	case ENaiwaP3BombType::SoyBigCross: SpriteName = TEXT("SPR_Bomb_SoyBigCross_A"); break;
	case ENaiwaP3BombType::TeaRadial: SpriteName = TEXT("SPR_Bomb_TeaRadial_A"); break;
	case ENaiwaP3BombType::RatSplit:
	case ENaiwaP3BombType::RatFragment: SpriteName = TEXT("SPR_Bomb_RatSplit_A"); break;
	case ENaiwaP3BombType::Standard:
	default: SpriteName = TEXT("SPR_Bomb_Standard_A"); break;
	}
	const FString SpritePath = FString::Printf(
		TEXT("/Game/Naiwa/P3/Sprites/%s.%s"), SpriteName, SpriteName);
	UTexture2D* SpriteTexture = LoadObject<UTexture2D>(nullptr, *SpritePath);
	const bool bHasSprite = SpriteMaterial && SpriteTexture;
	if (bHasSprite)
	{
		SpriteMaterial->SetTextureParameterValue(TEXT("Portrait"), SpriteTexture);
		SpriteMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
		BombSpriteMesh->SetMaterial(0, SpriteMaterial);
	}
	if (BombSpriteMesh) BombSpriteMesh->SetVisibility(bHasSprite);
	if (BombMesh) BombMesh->SetVisibility(!bHasSprite);
	if (BaseMesh) BaseMesh->SetVisibility(!bHasSprite);
	if (FuseMesh) FuseMesh->SetVisibility(!bHasSprite);
	if (SparkMesh) SparkMesh->SetVisibility(!bHasSprite);
	if (BombMaterial)
	{
		BombMaterial->SetVectorParameterValue(TEXT("Color"), GetNaiwaP3BombColor(BombType));
	}
	const FLinearColor MarkerColor = BombType == ENaiwaP3BombType::SoyBigCross ?
		FLinearColor::FromSRGBColor(FColor(255, 203, 83)) :
		BombType == ENaiwaP3BombType::TeaRadial ? FLinearColor::FromSRGBColor(FColor(213, 163, 244)) :
		FLinearColor::FromSRGBColor(FColor(105, 182, 255));
	if (TypeMarkerMaterial) TypeMarkerMaterial->SetVectorParameterValue(TEXT("Color"), MarkerColor);
	for (int32 Index = 0; Index < TypeMarkerComponents.Num(); ++Index)
	{
		const bool bVisible = (BombType == ENaiwaP3BombType::SoyBigCross && Index < 2) ||
			(BombType == ENaiwaP3BombType::TeaRadial && (Index == 2 || Index == 3)) ||
			((BombType == ENaiwaP3BombType::RatSplit || BombType == ENaiwaP3BombType::RatFragment) && Index >= 4);
		TypeMarkerComponents[Index]->SetVisibility(bVisible && !bHasSprite);
	}
}

void ANaiwaP3BombActor::BeginVisualSlide(const FVector& Start, const FVector& End,
	const float DurationSeconds)
{
	VisualSlideStart = Start;
	VisualSlideEnd = End;
	VisualSlideElapsed = 0.0f;
	VisualSlideDuration = FMath::Max(DurationSeconds, KINDA_SMALL_NUMBER);
	bVisualSliding = true;
	SetActorLocation(Start);
}
