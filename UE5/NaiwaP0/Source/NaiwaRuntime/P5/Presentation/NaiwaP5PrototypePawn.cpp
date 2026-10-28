#include "NaiwaP5PrototypePawn.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "../Core/NaiwaP5RunSubsystem.h"
#include "../Core/NaiwaP5Simulation.h"
#include "../Core/NaiwaP5TrackData.h"
#include "NaiwaP5A0ArtSampleUtils.h"

ANaiwaP5PrototypePawn::ANaiwaP5PrototypePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded()) CubeMesh = CubeFinder.Object;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded()) PlaneMesh = PlaneFinder.Object;

	CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
	SetRootComponent(CollisionCapsule);
	CollisionCapsule->InitCapsuleSize(32.0f, 80.0f);
	CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionCapsule->SetGenerateOverlapEvents(false);

	GrayboxBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GrayboxBody"));
	GrayboxBody->SetupAttachment(CollisionCapsule);
	if (CubeMesh) GrayboxBody->SetStaticMesh(CubeMesh);
	GrayboxBody->SetRelativeLocation(FVector(0.0, 0.0, 5.0));
	GrayboxBody->SetRelativeScale3D(FVector(0.42, 0.38, 0.72));
	GrayboxBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GrayboxBody->SetCastShadow(false);

	GrayboxSnowboard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GrayboxSnowboard"));
	GrayboxSnowboard->SetupAttachment(CollisionCapsule);
	if (CubeMesh) GrayboxSnowboard->SetStaticMesh(CubeMesh);
	GrayboxSnowboard->SetRelativeLocation(FVector(0.0, 0.0, -76.0));
	GrayboxSnowboard->SetRelativeScale3D(FVector(0.72, 0.18, 0.055));
	GrayboxSnowboard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GrayboxSnowboard->SetCastShadow(false);

	GrayboxMount = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GrayboxMount"));
	GrayboxMount->SetupAttachment(CollisionCapsule);
	if (CubeMesh) GrayboxMount->SetStaticMesh(CubeMesh);
	GrayboxMount->SetRelativeLocation(FVector(0.0, 0.0, -34.0));
	GrayboxMount->SetRelativeScale3D(FVector(0.92, 0.55, 0.42));
	GrayboxMount->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GrayboxMount->SetCastShadow(false);
	GrayboxMount->SetHiddenInGame(true);

	A0HeroArt = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("A0HeroArt"));
	A0HeroArt->SetupAttachment(CollisionCapsule);
	if (PlaneMesh) A0HeroArt->SetStaticMesh(PlaneMesh);
	A0HeroArt->SetRelativeLocation(FVector(0.0, 18.0, 24.0));
	A0HeroArt->SetRelativeRotation(FRotator(0.0, 0.0, 90.0));
	A0HeroArt->SetRelativeScale3D(FVector(2.0, 2.0, 1.0));
	A0HeroArt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	A0HeroArt->SetCastShadow(false);
	A0HeroArt->SetHiddenInGame(true);
	if (UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(ParentMaterial, this))
		{
			Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.42f, 0.72f, 0.46f, 1.0f));
			GrayboxMount->SetMaterial(0, Material);
		}
	}

	SideCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("SideCamera"));
	SideCamera->SetupAttachment(CollisionCapsule);
	SideCamera->SetRelativeLocation(FVector(0.0, 2500.0, 600.0));
	SideCamera->SetRelativeRotation(FRotator(0.0, 270.0, 0.0));
	SideCamera->ProjectionMode = ECameraProjectionMode::Orthographic;
	SideCamera->OrthoWidth = 2300.0f;
}

void ANaiwaP5PrototypePawn::BeginPlay()
{
	Super::BeginPlay();
	RunStartLocation = GetActorLocation();
	RunStartLocation.Z = FMath::Max(RunStartLocation.Z, 80.0);
	SetActorLocation(RunStartLocation);
	RunSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UNaiwaP5RunSubsystem>() : nullptr;
	bA0ArtPresentationActive = IsNaiwaP5A0ArtSampleWorld(GetWorld());
	if (bA0ArtPresentationActive)
	{
		// Frame the skier in the lower-middle of the view. The graybox camera offset
		// placed the route at the bottom edge once the run pawn followed the slope.
		SideCamera->SetRelativeLocation(FVector(0.0, 2500.0, 120.0));
		UMaterialInterface* HeroParentMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Naiwa/P5/ArtSample_v05/Materials/M_P5_A0_NaiwaSkiAtlas.M_P5_A0_NaiwaSkiAtlas"));
		bA0AtlasMaterialActive = HeroParentMaterial != nullptr;
		if (!HeroParentMaterial)
		{
			HeroParentMaterial = LoadObject<UMaterialInterface>(nullptr,
				TEXT("/Game/Naiwa/P5/ArtSample_v04/Materials/M_P5_A0_NaiwaGlide.M_P5_A0_NaiwaGlide"));
		}
		A0HeroArtMaterial = HeroParentMaterial
			? UMaterialInstanceDynamic::Create(HeroParentMaterial, this) : nullptr;
		if (A0HeroArtMaterial && A0HeroArt)
		{
			A0HeroArt->SetMaterial(0, A0HeroArtMaterial);
			A0HeroArt->SetHiddenInGame(false);
			A0HeroArt->SetVisibility(true, true);
			// The art proxy now follows the live pawn. Keep the collision capsule and simulation
			// intact while hiding only the two graybox display meshes in this sample map.
			GrayboxBody->SetHiddenInGame(true);
			GrayboxSnowboard->SetHiddenInGame(true);
		}
	}
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->SetViewTarget(this);
	}
}

void ANaiwaP5PrototypePawn::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bRestartHeld && !bRestartTriggered)
	{
		RestartHeldSeconds += FMath::Max(0.0f, DeltaSeconds);
		if (RestartHeldSeconds >= 0.6)
		{
			if (RunSubsystem.IsValid()) RunSubsystem->RestartPrototypeRun();
			SetActorLocation(RunStartLocation);
			SetActorRotation(FRotator::ZeroRotator);
			bPaused = false;
			bRestartTriggered = true;
		}
	}

	SubmitInput();
	if (RunSubsystem.IsValid())
	{
		const FNaiwaP5RunState& State = RunSubsystem->GetState();
		FVector Location = RunStartLocation;
		Location.X += State.DistanceMeters * 100.0;
		const FNaiwaP5RouteFixture& RouteFixture = RunSubsystem->GetActivePrototypeFixture();
		const int32 DistanceMm = FMath::FloorToInt(State.DistanceMeters * 1000.0);
		const double RouteHeightMeters = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(RouteFixture, DistanceMm);
		const double StartHeightMeters = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(RouteFixture, 0);
		Location.Z += (RouteHeightMeters - StartHeightMeters + State.JumpHeightMeters) * 100.0;
		SetActorLocation(Location);
		SetActorRotation(FRotator::ZeroRotator);
		GrayboxMount->SetHiddenInGame(!State.bRiding);
		const double FallTiltDegrees = State.bFallen ? 72.0 : 0.0;
		const FRotator VisualRotation(State.RotationDegrees + FallTiltDegrees, 0.0, 0.0);
		GrayboxBody->SetRelativeRotation(VisualRotation);
		GrayboxSnowboard->SetRelativeRotation(VisualRotation);
		UpdateA0ArtPresentation(State, DeltaSeconds);
	}
}

void ANaiwaP5PrototypePawn::UpdateA0ArtPresentation(const FNaiwaP5RunState& State,
	const double DeltaSeconds)
{
	if (!bA0ArtPresentationActive || !A0HeroArtMaterial || !A0HeroArt)
	{
		return;
	}

	if (bA0WasAirborne && !State.bAirborne && !State.bFallen)
	{
		A0LandingPoseSecondsRemaining = 0.18;
	}
	else
	{
		A0LandingPoseSecondsRemaining = FMath::Max(0.0,
			A0LandingPoseSecondsRemaining - FMath::Max(0.0, DeltaSeconds));
	}
	bA0WasAirborne = State.bAirborne;

	A0HeroArt->SetVisibility(!State.bRiding, true);
	A0HeroArt->SetHiddenInGame(State.bRiding);
	if (State.bRiding)
	{
		return;
	}

	// Cropped, owner-approved sprite rectangles from P5_A4_NaiwaSki_manifest_v01.json.
	// The fallback v04 material is a pre-cropped Glide cell and uses its full UV range.
	static const FIntRect SpriteRects[] = {
		FIntRect(76, 58, 607, 476),    // Glide
		FIntRect(786, 141, 1261, 476), // Preload
		FIntRect(1498, 109, 1914, 476),// AirTuck
		FIntRect(92, 559, 591, 988),   // AirTurn
		FIntRect(775, 669, 1273, 988), // Land
		FIntRect(1477, 667, 1935, 988) // Fall
	};
	int32 FrameIndex = 0;
	if (State.bFallen)
	{
		FrameIndex = 5;
	}
	else if (A0LandingPoseSecondsRemaining > 0.0)
	{
		FrameIndex = 4;
	}
	else if (State.bAirborne)
	{
		FrameIndex = FMath::Abs(State.RotationDegrees) > 20.0 ? 3 : 2;
	}
	else if (State.bJumpCharging)
	{
		FrameIndex = 1;
	}

	if (bA0AtlasMaterialActive)
	{
		const FIntRect& Rect = SpriteRects[FrameIndex];
		const double WidthPx = static_cast<double>(Rect.Width());
		const double HeightPx = static_cast<double>(Rect.Height());
		A0HeroArtMaterial->SetVectorParameterValue(TEXT("A0UVScale"), FLinearColor(
			WidthPx / 2048.0, HeightPx / 1024.0, 0.0f, 0.0f));
		A0HeroArtMaterial->SetVectorParameterValue(TEXT("A0UVOffset"), FLinearColor(
			static_cast<double>(Rect.Min.X) / 2048.0,
			1.0 - static_cast<double>(Rect.Max.Y) / 1024.0, 0.0f, 0.0f));
		const double SpriteWidthCm = 220.0;
		const double SpriteHeightCm = SpriteWidthCm * HeightPx / WidthPx;
		A0HeroArt->SetRelativeScale3D(FVector(SpriteWidthCm / 100.0, SpriteHeightCm / 100.0, 1.0));
			A0HeroArt->SetRelativeLocation(FVector(0.0, 20.0,
			-80.0 + SpriteHeightCm * 0.5));
	}

	const double SpinRadians = FMath::DegreesToRadians(State.RotationDegrees +
		(State.bFallen ? 72.0 : 0.0));
	const FQuat FaceCamera(FVector::ForwardVector, FMath::DegreesToRadians(90.0));
	const FQuat InPlaneSpin(-FVector::RightVector, SpinRadians);
	A0HeroArt->SetRelativeRotation((InPlaneSpin * FaceCamera).Rotator());
}

void ANaiwaP5PrototypePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!PlayerInputComponent)
	{
		return;
	}
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ANaiwaP5PrototypePawn::JumpPressed);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &ANaiwaP5PrototypePawn::JumpReleased);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed, this, &ANaiwaP5PrototypePawn::RotateLeftPressed);
	PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &ANaiwaP5PrototypePawn::RotateLeftReleased);
	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed, this, &ANaiwaP5PrototypePawn::RotateRightPressed);
	PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &ANaiwaP5PrototypePawn::RotateRightReleased);
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &ANaiwaP5PrototypePawn::RestartPressed);
	PlayerInputComponent->BindKey(EKeys::R, IE_Released, this, &ANaiwaP5PrototypePawn::RestartReleased);
	PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ANaiwaP5PrototypePawn::TogglePause);
}

void ANaiwaP5PrototypePawn::JumpPressed()
{
	bJumpHeld = true;
	bJumpPressedPending = true;
	SubmitInput();
}

void ANaiwaP5PrototypePawn::JumpReleased()
{
	bJumpHeld = false;
	bJumpReleasedPending = true;
	SubmitInput();
}

void ANaiwaP5PrototypePawn::RotateLeftPressed()
{
	bRotateLeft = true;
	SubmitInput();
}

void ANaiwaP5PrototypePawn::RotateLeftReleased()
{
	bRotateLeft = false;
	SubmitInput();
}

void ANaiwaP5PrototypePawn::RotateRightPressed()
{
	bRotateRight = true;
	SubmitInput();
}

void ANaiwaP5PrototypePawn::RotateRightReleased()
{
	bRotateRight = false;
	SubmitInput();
}

void ANaiwaP5PrototypePawn::RestartPressed()
{
	bRestartHeld = true;
	bRestartTriggered = false;
	RestartHeldSeconds = 0.0;
}

void ANaiwaP5PrototypePawn::RestartReleased()
{
	bRestartHeld = false;
	bRestartTriggered = false;
	RestartHeldSeconds = 0.0;
}

void ANaiwaP5PrototypePawn::TogglePause()
{
	bPaused = !bPaused;
	if (RunSubsystem.IsValid())
	{
		RunSubsystem->SetPaused(bPaused);
	}
}

void ANaiwaP5PrototypePawn::SubmitInput()
{
	if (!RunSubsystem.IsValid())
	{
		bJumpPressedPending = false;
		bJumpReleasedPending = false;
		return;
	}

	FNaiwaP5InputCommand Command;
	Command.bJumpHeld = bJumpHeld;
	Command.bJumpPressed = bJumpPressedPending;
	Command.bJumpReleased = bJumpReleasedPending;
	Command.bRotateLeft = bRotateLeft;
	Command.bRotateRight = bRotateRight;
	RunSubsystem->SetInputCommand(Command);
	bJumpPressedPending = false;
	bJumpReleasedPending = false;
}
