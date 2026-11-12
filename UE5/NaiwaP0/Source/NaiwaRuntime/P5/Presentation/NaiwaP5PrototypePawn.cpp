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

namespace
{
	// The v06 WIP manifests use cell-space UV anchors. Convert them through the
	// selected crop so the A4 rider and A5 mount stay attached as the crop changes.
	constexpr double A0RideSeatCellV = 0.64;
	// Measured against the v06 atlas crops: 0.48 from the WIP manifest lands
	// above the saddle on the mount's head/ears for these selected poses.
	constexpr double A0RideSaddleCellV = 0.61;
	constexpr double A0BounceSaddleCellV = 0.53;
	constexpr double A0MountCellSizePx = 512.0;
	constexpr double A0MountArtBottomFromPawnCm = -80.0;
	constexpr double A0A4RideCellSizePx = 512.0;
	constexpr double A0A4RideSpriteTopPx = 31.0;
	constexpr double A0A4RideSpriteHeightPx = 439.0;
	constexpr double A0FallbackRideHeroBottomCm = 15.0;
	void SetA0AtlasRegion(UMaterialInstanceDynamic* Material, const int32 TextureWidth,
		const int32 TextureHeight, const int32 X, const int32 Y, const int32 Width,
		const int32 Height)
	{
		if (!Material || TextureWidth <= 0 || TextureHeight <= 0 || Width <= 0 || Height <= 0)
		{
			return;
		}
		Material->SetVectorParameterValue(TEXT("A0UVScale"), FLinearColor(
			static_cast<double>(Width) / TextureWidth,
			-static_cast<double>(Height) / TextureHeight, 0.0f, 0.0f));
		Material->SetVectorParameterValue(TEXT("A0UVOffset"), FLinearColor(
			static_cast<double>(X) / TextureWidth,
			1.0 - static_cast<double>(Y) / TextureHeight, 0.0f, 0.0f));
	}

	void SetA0FacingRotation(UStaticMeshComponent* Component, const double SpinDegrees)
	{
		if (!Component)
		{
			return;
		}
		const FQuat FaceCamera(FVector::ForwardVector, FMath::DegreesToRadians(90.0));
		const FQuat InPlaneSpin(-FVector::RightVector, FMath::DegreesToRadians(SpinDegrees));
		Component->SetRelativeRotation((InPlaneSpin * FaceCamera).Rotator());
	}
}

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
	GrayboxSnowboard->SetRelativeLocation(FVector(0.0, 220.0, -68.0));
	GrayboxSnowboard->SetRelativeScale3D(FVector(2.8, 0.10, 0.24));
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
	A0HeroArt->SetRelativeLocation(FVector(0.0, 260.0, 24.0));
	A0HeroArt->SetRelativeRotation(FRotator(0.0, 0.0, 90.0));
	A0HeroArt->SetRelativeScale3D(FVector(2.0, 2.0, 1.0));
	A0HeroArt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	A0HeroArt->SetCastShadow(false);
	A0HeroArt->SetHiddenInGame(true);

	A0MountArt = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("A0MountArt"));
	A0MountArt->SetupAttachment(CollisionCapsule);
	if (PlaneMesh) A0MountArt->SetStaticMesh(PlaneMesh);
	A0MountArt->SetRelativeLocation(FVector(0.0, 240.0, 5.0));
	A0MountArt->SetRelativeRotation(FRotator(0.0, 0.0, 90.0));
	A0MountArt->SetRelativeScale3D(FVector(2.0, 3.0, 1.0));
	A0MountArt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	A0MountArt->SetCastShadow(false);
	A0MountArt->SetHiddenInGame(true);

	A0SnowboardArt = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("A0SnowboardArt"));
	A0SnowboardArt->SetupAttachment(CollisionCapsule);
	if (PlaneMesh) A0SnowboardArt->SetStaticMesh(PlaneMesh);
	A0SnowboardArt->SetRelativeLocation(FVector(0.0, 230.0, -62.0));
	A0SnowboardArt->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	A0SnowboardArt->SetRelativeScale3D(FVector(2.5, 0.52, 1.0));
	A0SnowboardArt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	A0SnowboardArt->SetCastShadow(false);
	A0SnowboardArt->SetHiddenInGame(true);

	A0SnowTrackArt = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("A0SnowTrackArt"));
	A0SnowTrackArt->SetupAttachment(CollisionCapsule);
	if (PlaneMesh) A0SnowTrackArt->SetStaticMesh(PlaneMesh);
	A0SnowTrackArt->SetRelativeLocation(FVector(-170.0, 220.0, -76.0));
	A0SnowTrackArt->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	A0SnowTrackArt->SetRelativeScale3D(FVector(3.2, 0.40, 1.0));
	A0SnowTrackArt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	A0SnowTrackArt->SetCastShadow(false);
	A0SnowTrackArt->SetHiddenInGame(true);

	for (int32 SnowflakeIndex = 0; SnowflakeIndex < 4; ++SnowflakeIndex)
	{
		UStaticMeshComponent* Snowflake = CreateDefaultSubobject<UStaticMeshComponent>(
			FName(*FString::Printf(TEXT("A0SnowflakeArt_%d"), SnowflakeIndex)));
		Snowflake->SetupAttachment(CollisionCapsule);
		if (PlaneMesh) Snowflake->SetStaticMesh(PlaneMesh);
		Snowflake->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Snowflake->SetCastShadow(false);
		Snowflake->SetHiddenInGame(true);
		A0SnowflakeArts.Add(Snowflake);
	}

	A0LandingPuffArt = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("A0LandingPuffArt"));
	A0LandingPuffArt->SetupAttachment(CollisionCapsule);
	if (PlaneMesh) A0LandingPuffArt->SetStaticMesh(PlaneMesh);
	A0LandingPuffArt->SetRelativeLocation(FVector(0.0, 220.0, -106.0));
	A0LandingPuffArt->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	A0LandingPuffArt->SetRelativeScale3D(FVector(1.8, 1.2, 1.0));
	A0LandingPuffArt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	A0LandingPuffArt->SetCastShadow(false);
	A0LandingPuffArt->SetHiddenInGame(true);
	if (UMaterialInterface* ParentMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(ParentMaterial, this))
		{
			Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.48f, 0.27f, 0.12f, 1.0f));
			GrayboxSnowboard->SetMaterial(0, Material);
		}
		if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(ParentMaterial, this))
		{
			Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.42f, 0.72f, 0.46f, 1.0f));
			GrayboxMount->SetMaterial(0, Material);
		}
	}

	SideCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("SideCamera"));
	SideCamera->SetupAttachment(CollisionCapsule);
	SideCamera->SetRelativeLocation(FVector(0.0, 2500.0, 900.0));
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
		SideCamera->SetRelativeLocation(FVector(0.0, 2500.0, 900.0));
		FPostProcessSettings& A0PostProcess = SideCamera->PostProcessSettings;
		A0PostProcess.bOverride_DepthOfFieldScale = true;
		A0PostProcess.DepthOfFieldScale = 0.0f;
		A0PostProcess.bOverride_MotionBlurAmount = true;
		A0PostProcess.MotionBlurAmount = 0.0f;
		A0PostProcess.bOverride_Sharpen = true;
		A0PostProcess.Sharpen = 0.55f;
		UMaterialInterface* HeroParentMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Naiwa/P5/ArtSample_v06/Materials/M_P5_A4_NaiwaBodyAtlas_v05_WIP.M_P5_A4_NaiwaBodyAtlas_v05_WIP"));
		bA0ContinuousAtlasActive = HeroParentMaterial != nullptr;
		if (!HeroParentMaterial)
		{
			HeroParentMaterial = LoadObject<UMaterialInterface>(nullptr,
				TEXT("/Game/Naiwa/P5/ArtSample_v05/Materials/M_P5_A0_NaiwaSkiAtlas.M_P5_A0_NaiwaSkiAtlas"));
			bA0AtlasMaterialActive = HeroParentMaterial != nullptr;
		}
		A0HeroArtMaterial = HeroParentMaterial
			? UMaterialInstanceDynamic::Create(HeroParentMaterial, this) : nullptr;

		UMaterialInterface* MountParentMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Naiwa/P5/ArtSample_v06/Materials/M_P5_A5_KangarooMount_v02_WIP.M_P5_A5_KangarooMount_v02_WIP"));
		bA0ContinuousMountAtlasActive = MountParentMaterial != nullptr;
		if (!MountParentMaterial)
		{
			MountParentMaterial = LoadObject<UMaterialInterface>(nullptr,
				TEXT("/Game/Naiwa/P5/ArtSample_v05/Materials/M_P5_A0_KangarooRideAtlas.M_P5_A0_KangarooRideAtlas"));
		}
		A0MountArtMaterial = MountParentMaterial
			? UMaterialInstanceDynamic::Create(MountParentMaterial, this) : nullptr;
		if (bA0ContinuousMountAtlasActive && A0MountArtMaterial)
		{
			// The first candidate frame is the isolated available-idle pose. Runtime
			// ride-state cropping is updated below; alignment remains WIP.
			SetA0AtlasRegion(A0MountArtMaterial, 1024, 1024, 28, 150, 285, 334);
		}
		else if (A0MountArtMaterial)
		{
			// Preserve the existing v05 ride atlas fallback while the candidate package
			// is absent or has not yet been imported.
			SetA0AtlasRegion(A0MountArtMaterial, 1024, 1024, 683, 0, 341, 512);
		}
		if (A0MountArtMaterial && A0MountArt)
		{
			A0MountArt->SetMaterial(0, A0MountArtMaterial);
		}

		UMaterialInterface* SnowboardParentMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Naiwa/P5/ArtSample_v06/Materials/M_P5_A4_Snowboard_v01_WIP.M_P5_A4_Snowboard_v01_WIP"));
		A0SnowboardArtMaterial = SnowboardParentMaterial
			? UMaterialInstanceDynamic::Create(SnowboardParentMaterial, this) : nullptr;
		if (A0SnowboardArtMaterial && A0SnowboardArt)
		{
			// Crop the transparent padding in the delivered 1024x256 board image.
			SetA0AtlasRegion(A0SnowboardArtMaterial, 1024, 256, 204, 84, 633, 77);
			A0SnowboardArt->SetMaterial(0, A0SnowboardArtMaterial);
		}

		UMaterialInterface* SnowTrackParentMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Naiwa/P5/ArtSample_v06/Materials/M_P5_A2_SnowTrack_v01_WIP.M_P5_A2_SnowTrack_v01_WIP"));
		A0SnowTrackArtMaterial = SnowTrackParentMaterial
			? UMaterialInstanceDynamic::Create(SnowTrackParentMaterial, this) : nullptr;
		if (A0SnowTrackArtMaterial && A0SnowTrackArt)
		{
			A0SnowTrackArt->SetMaterial(0, A0SnowTrackArtMaterial);
		}

		UMaterialInterface* SnowflakeParentMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Naiwa/P5/ArtSample_v06/Materials/M_P5_A6_Snowflake_v01_WIP.M_P5_A6_Snowflake_v01_WIP"));
		A0SnowflakeArtMaterials.Reset(A0SnowflakeArts.Num());
		for (UStaticMeshComponent* Snowflake : A0SnowflakeArts)
		{
			UMaterialInstanceDynamic* SnowflakeMaterial = SnowflakeParentMaterial
				? UMaterialInstanceDynamic::Create(SnowflakeParentMaterial, this) : nullptr;
			if (Snowflake && SnowflakeMaterial)
			{
				Snowflake->SetMaterial(0, SnowflakeMaterial);
			}
			A0SnowflakeArtMaterials.Add(SnowflakeMaterial);
		}

		UMaterialInterface* LandingPuffParentMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Naiwa/P5/ArtSample_v06/Materials/M_P5_A6_LandPuff_v01_WIP.M_P5_A6_LandPuff_v01_WIP"));
		A0LandingPuffArtMaterial = LandingPuffParentMaterial
			? UMaterialInstanceDynamic::Create(LandingPuffParentMaterial, this) : nullptr;
		if (A0LandingPuffArtMaterial && A0LandingPuffArt)
		{
			A0LandingPuffArt->SetMaterial(0, A0LandingPuffArtMaterial);
		}
		if (A0HeroArtMaterial && A0HeroArt)
		{
			A0HeroArt->SetMaterial(0, A0HeroArtMaterial);
			A0HeroArt->SetHiddenInGame(false);
			A0HeroArt->SetVisibility(true, true);
			// Keep simulation/collision intact and hide graybox displays only when their
			// corresponding WIP art replacement is present.
			GrayboxBody->SetHiddenInGame(true);
			// Keep the broad, low-profile board backing visible beneath the cutout art.
			// The WIP sprite alone was too thin at gameplay scale and could vanish on fallback.
			GrayboxSnowboard->SetHiddenInGame(false);
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
		if (bA0ArtPresentationActive && SideCamera)
		{
			// Keep some vertical camera lag so a jump moves against the scenery,
			// while moving the route lower in frame to reveal more of the landscape.
			FVector CameraLocation = SideCamera->GetRelativeLocation();
			const double BoostAlpha = State.TrickBoostSeconds > 0.0
				? FMath::Clamp(State.TrickBoostSeconds / NaiwaP5Rules::TrickBoostTier2DurationSeconds, 0.0, 1.0)
				: 0.0;
			const double WarningAlpha = State.bAvalancheWarning
				? FMath::Clamp((NaiwaP5Rules::AvalancheWarningGapMeters - State.AvalancheGapMeters) /
					NaiwaP5Rules::AvalancheWarningGapMeters, 0.0, 1.0)
				: 0.0;
			const double ShakePhase = GetWorld()->GetTimeSeconds() * 17.0;
			CameraLocation.X = FMath::Sin(ShakePhase) * 12.0 * WarningAlpha;
			CameraLocation.Z = 900.0 - State.JumpHeightMeters * 100.0 * 0.40 +
				FMath::Sin(ShakePhase * 1.31) * 8.0 * WarningAlpha;
			SideCamera->SetRelativeLocation(CameraLocation);
			SideCamera->OrthoWidth = static_cast<float>(2300.0 - 140.0 * BoostAlpha - 100.0 * WarningAlpha);
		}
		GrayboxMount->SetHiddenInGame(!State.bRiding);
		const double FallTiltDegrees = State.bFallen ? 72.0 : 0.0;
		const FRotator VisualRotation(State.RotationDegrees + FallTiltDegrees, 0.0, 0.0);
		GrayboxBody->SetRelativeRotation(VisualRotation);
		GrayboxSnowboard->SetRelativeRotation(VisualRotation);
		double RouteSlopeDegrees = 0.0;
		if (bA0ArtPresentationActive)
		{
			const int32 BeforeDistanceMm = FMath::Max(0, DistanceMm - 250);
			const int32 AfterDistanceMm = FMath::Min(RouteFixture.RouteLengthMm, DistanceMm + 250);
			if (AfterDistanceMm > BeforeDistanceMm)
			{
				const double BeforeHeight = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(
					RouteFixture, BeforeDistanceMm);
				const double AfterHeight = FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(
					RouteFixture, AfterDistanceMm);
				const double HorizontalMeters = static_cast<double>(AfterDistanceMm - BeforeDistanceMm) / 1000.0;
				RouteSlopeDegrees = FMath::RadiansToDegrees(FMath::Atan2(
					AfterHeight - BeforeHeight, HorizontalMeters));
			}
		}
		UpdateA0ArtPresentation(State, DeltaSeconds, RouteSlopeDegrees);
	}
}

void ANaiwaP5PrototypePawn::UpdateA0ArtPresentation(const FNaiwaP5RunState& State,
	const double DeltaSeconds, const double RouteSlopeDegrees)
{
	bA0RideAnchorProbeValid = false;
	if (!bA0ArtPresentationActive)
	{
		return;
	}

	if (bA0WasAirborne && !State.bAirborne && !State.bFallen)
	{
		A0LandingPoseSecondsRemaining = 0.40;
	}
	else
	{
		A0LandingPoseSecondsRemaining = FMath::Max(0.0,
			A0LandingPoseSecondsRemaining - FMath::Max(0.0, DeltaSeconds));
	}
	if (!bA0WasAirborne && State.bAirborne)
	{
		A0TakeoffPoseSecondsRemaining = 0.08;
	}
	else
	{
		A0TakeoffPoseSecondsRemaining = FMath::Max(0.0,
			A0TakeoffPoseSecondsRemaining - FMath::Max(0.0, DeltaSeconds));
	}
	bA0WasAirborne = State.bAirborne;

	const bool bUseMountArt = A0MountArtMaterial && A0MountArt;
	double MountHeightForRideAnchorCm = 0.0;
	double MountSaddleFromSpriteBottomCm = 0.0;
	if (A0MountArt)
	{
		A0MountArt->SetVisibility(State.bRiding && bUseMountArt, true);
		A0MountArt->SetHiddenInGame(!State.bRiding || !bUseMountArt);
	}
	GrayboxMount->SetHiddenInGame(!State.bRiding || bUseMountArt);
	const bool bUseSnowboardArt = A0SnowboardArtMaterial && A0SnowboardArt;
	const bool bShowSnowboard = !State.bRiding;
	GrayboxSnowboard->SetVisibility(bShowSnowboard, true);
	GrayboxSnowboard->SetHiddenInGame(!bShowSnowboard);
	GrayboxSnowboard->SetRelativeRotation(FRotator(State.RotationDegrees + RouteSlopeDegrees +
		(State.bFallen ? 72.0 : 0.0), 0.0, 0.0));
	if (A0SnowboardArt)
	{
		A0SnowboardArt->SetVisibility(bUseSnowboardArt && bShowSnowboard, true);
		A0SnowboardArt->SetHiddenInGame(!bUseSnowboardArt || !bShowSnowboard);
	}
	if (bUseSnowboardArt)
	{
		A0SnowboardArt->SetRelativeScale3D(FVector(2.8, 0.36, 1.0));
	A0SnowboardArt->SetRelativeLocation(FVector(0.0, 230.0, -62.0));
	}
	if (A0HeroArt)
	{
		const bool bShowHeroArt = A0HeroArtMaterial != nullptr;
		A0HeroArt->SetVisibility(bShowHeroArt, true);
		A0HeroArt->SetHiddenInGame(!bShowHeroArt);
	}
	const bool bShowLandingPuff = A0LandingPoseSecondsRemaining > 0.0 &&
		A0LandingPuffArtMaterial && A0LandingPuffArt;
	if (A0LandingPuffArt)
	{
		A0LandingPuffArt->SetVisibility(bShowLandingPuff, true);
		A0LandingPuffArt->SetHiddenInGame(!bShowLandingPuff);
	}
	if (bShowLandingPuff)
	{
		const double Elapsed = 0.40 - A0LandingPoseSecondsRemaining;
		const int32 PuffFrame = FMath::Clamp(FMath::FloorToInt(Elapsed * 30.0), 0, 11);
		SetA0AtlasRegion(A0LandingPuffArtMaterial, 512, 512,
			(PuffFrame % 4) * 128, (PuffFrame / 4) * 128, 128, 128);
	}
	const bool bTrickBoostActive = State.TrickBoostSeconds > 0.0;
	const bool bShowSnowTrack = A0SnowTrackArtMaterial && A0SnowTrackArt &&
		(!State.bRiding || bTrickBoostActive) &&
		!State.bFallen && State.PlayerSpeedMetersPerSecond > 1.0;
	if (A0SnowTrackArt)
	{
		A0SnowTrackArt->SetVisibility(bShowSnowTrack, true);
		A0SnowTrackArt->SetHiddenInGame(!bShowSnowTrack);
	}
	if (bShowSnowTrack)
	{
		const double TrailWidthCm = FMath::Clamp(State.PlayerSpeedMetersPerSecond *
			(bTrickBoostActive ? 28.0 : 22.0), 180.0, bTrickBoostActive ? 500.0 : 360.0);
		A0SnowTrackArt->SetRelativeLocation(FVector(bTrickBoostActive ? -280.0 : -170.0,
			State.bRiding ? 300.0 : 220.0, -76.0));
		A0SnowTrackArt->SetRelativeScale3D(FVector(TrailWidthCm / 100.0, 0.40, 1.0));
		SetA0FacingRotation(A0SnowTrackArt, RouteSlopeDegrees);
	}
	const bool bSnowflakesEnabled = !State.bFallen &&
		(State.PlayerSpeedMetersPerSecond >= 12.0 || State.bAvalancheWarning);
	for (int32 SnowflakeIndex = 0; SnowflakeIndex < A0SnowflakeArts.Num(); ++SnowflakeIndex)
	{
		UStaticMeshComponent* Snowflake = A0SnowflakeArts[SnowflakeIndex];
		const bool bShowSnowflake = bSnowflakesEnabled && Snowflake &&
			A0SnowflakeArtMaterials.IsValidIndex(SnowflakeIndex) &&
			A0SnowflakeArtMaterials[SnowflakeIndex];
		if (Snowflake)
		{
			Snowflake->SetVisibility(bShowSnowflake, true);
			Snowflake->SetHiddenInGame(!bShowSnowflake);
		}
		if (bShowSnowflake)
		{
			const double Phase = FMath::Fmod(State.DistanceMeters * 0.55 + SnowflakeIndex * 0.27, 1.0);
			const double DriftRangeCm = bTrickBoostActive ? 560.0 : (State.bAvalancheWarning ? 420.0 : 240.0);
			const double DriftXcm = (Phase * 2.0 - 1.0) * DriftRangeCm;
			const double BobZcm = (bTrickBoostActive || State.bAvalancheWarning ? 180.0 : 135.0) +
				35.0 * FMath::Sin(State.DistanceMeters * 1.8 + SnowflakeIndex * 1.7);
			const double SizeCm = (bTrickBoostActive ? 30.0 : (State.bAvalancheWarning ? 24.0 : 14.0)) +
				static_cast<double>(SnowflakeIndex % 3) * (bTrickBoostActive ? 6.0 : 3.0);
			Snowflake->SetRelativeLocation(FVector(DriftXcm, 245.0, BobZcm));
			Snowflake->SetRelativeScale3D(FVector(SizeCm / 100.0, SizeCm / 100.0, 1.0));
			SetA0FacingRotation(Snowflake, RouteSlopeDegrees);
		}
	}
	if (State.bRiding)
	{
		if (bUseMountArt && bA0ContinuousMountAtlasActive)
		{
			static const FIntRect MountRects[] = {
				FIntRect(28, 150, 313, 484),  // AvailableIdle
				FIntRect(369, 194, 655, 484), // ReadyToCarry
				FIntRect(711, 233, 996, 484), // DownhillGlide
				FIntRect(28, 637, 313, 996),  // ImpactReact
				FIntRect(369, 676, 655, 996), // Bounce
				FIntRect(711, 659, 996, 996)  // RecoveredIdle
			};
			int32 MountFrame = State.RideElapsedSeconds < 0.20 ? 1 : 2;
			if (A0LandingPoseSecondsRemaining > 0.0)
			{
				MountFrame = 4;
			}
			const FIntRect& Rect = MountRects[MountFrame];
			SetA0AtlasRegion(A0MountArtMaterial, 1024, 1024, Rect.Min.X, Rect.Min.Y,
				Rect.Width(), Rect.Height());
			const double MountWidthCm = 330.0;
			const double MountHeightCm = MountWidthCm * Rect.Height() / Rect.Width();
			MountHeightForRideAnchorCm = MountHeightCm;
			const int32 MountCellTopPx = MountFrame >= 3 ? 512 : 0;
			const double SaddleCellV = MountFrame == 4 ? A0BounceSaddleCellV : A0RideSaddleCellV;
			const double SaddleYInMountCropPx = SaddleCellV * A0MountCellSizePx -
				(static_cast<double>(Rect.Min.Y - MountCellTopPx));
			A0MountSaddleMeshLocalY = (0.5 - SaddleYInMountCropPx / Rect.Height()) * 100.0;
			MountSaddleFromSpriteBottomCm = MountHeightCm *
				(1.0 - SaddleYInMountCropPx / Rect.Height());
			A0MountArt->SetRelativeScale3D(FVector(MountWidthCm / 100.0, MountHeightCm / 100.0, 1.0));
			A0MountArt->SetRelativeLocation(FVector(0.0, 330.0,
				A0MountArtBottomFromPawnCm + MountHeightCm * 0.5));
			SetA0FacingRotation(A0MountArt, State.RotationDegrees + RouteSlopeDegrees);
		}
		else if (bUseMountArt)
		{
			SetA0FacingRotation(A0MountArt, State.RotationDegrees + RouteSlopeDegrees);
		}
	}
	if (!A0HeroArtMaterial || !A0HeroArt)
	{
		return;
	}
	if (bA0ContinuousAtlasActive)
	{
		// Per-frame source rectangles are copied from the v03 WIP manifest. The
		// sequence timing and attachment points remain provisional until review.
		static const FIntRect BodySpriteRects[] = {
			FIntRect(28, 150, 484, 470), FIntRect(28, 129, 484, 470),
			FIntRect(28, 150, 484, 470), FIntRect(28, 120, 484, 470),
			FIntRect(28, 121, 484, 470), FIntRect(28, 106, 484, 470),
			FIntRect(28, 57, 484, 470),  FIntRect(28, 88, 484, 470),
			FIntRect(28, 119, 484, 470), FIntRect(28, 104, 484, 470),
			FIntRect(28, 68, 484, 470),  FIntRect(28, 62, 484, 470),
			FIntRect(28, 85, 484, 470),  FIntRect(28, 103, 484, 470),
			FIntRect(28, 115, 484, 470), FIntRect(28, 93, 484, 470),
			FIntRect(28, 209, 484, 470), FIntRect(28, 182, 484, 470),
			FIntRect(28, 206, 484, 470), FIntRect(28, 158, 484, 470),
			FIntRect(28, 31, 484, 470),  FIntRect(32, 28, 480, 484),
			FIntRect(28, 28, 484, 478)
		};
		// Hold one pose for each run state. The WIP atlas has no approved
		// motion-matched loops; cycling large silhouettes reads as flashing.
		int32 FrameIndex = 0;
		if (State.bFallen)
		{
			FrameIndex = 19;
		}
		else if (State.bRiding)
		{
			FrameIndex = 20;
		}
		else if (A0LandingPoseSecondsRemaining > 0.0)
		{
			FrameIndex = 15;
		}
		else if (State.bJumpCharging)
		{
			FrameIndex = 5;
		}
		else if (State.bAirborne)
		{
			if (FMath::Abs(State.RotationDegrees) > 20.0)
			{
				// Use one turn pose throughout the spin; animation will return when a
				// motion-matched sequence is available.
				FrameIndex = 10;
			}
			else
			{
				FrameIndex = 9;
			}
		}
		else
		{
			FrameIndex = 0;
		}

		const FIntRect& SpriteRect = BodySpriteRects[FrameIndex];
		const int32 AtlasX = (FrameIndex % 4) * 512 + SpriteRect.Min.X;
		const int32 AtlasY = (FrameIndex / 4) * 512 + SpriteRect.Min.Y;
		const int32 SpriteWidthPx = SpriteRect.Width();
		const int32 SpriteHeightPx = SpriteRect.Height();
		SetA0AtlasRegion(A0HeroArtMaterial, 2048, 3072, AtlasX, AtlasY,
			SpriteWidthPx, SpriteHeightPx);
		const double SpriteWidthCm = State.bRiding ? 220.0 : 300.0;
		const double SpriteHeightCm = SpriteWidthCm * SpriteHeightPx / SpriteWidthPx;
		double HeroBottomZcm = -50.0;
		if (State.bRiding)
		{
			const double RideSeatYInSpritePx = A0RideSeatCellV * A0A4RideCellSizePx -
				A0A4RideSpriteTopPx;
			A0RideSeatMeshLocalY =
				(0.5 - RideSeatYInSpritePx / A0A4RideSpriteHeightPx) * 100.0;
			const double RiderSeatFromSpriteBottomCm = SpriteHeightCm *
				(1.0 - RideSeatYInSpritePx / A0A4RideSpriteHeightPx);
			if (MountHeightForRideAnchorCm > 0.0)
			{
				HeroBottomZcm = A0MountArtBottomFromPawnCm + MountSaddleFromSpriteBottomCm -
					RiderSeatFromSpriteBottomCm;
			}
			else
			{
				HeroBottomZcm = A0FallbackRideHeroBottomCm;
			}
		}
		A0HeroArt->SetRelativeScale3D(FVector(SpriteWidthCm / 100.0, SpriteHeightCm / 100.0, 1.0));
		A0HeroArt->SetRelativeLocation(FVector(0.0, State.bRiding ? 280.0 : 260.0,
			HeroBottomZcm + SpriteHeightCm * 0.5));
		const double HeroSpinDegrees = State.RotationDegrees + RouteSlopeDegrees +
			(State.bFallen ? 72.0 : 0.0);
		SetA0FacingRotation(A0HeroArt, HeroSpinDegrees);
		bA0RideAnchorProbeValid = State.bRiding && bUseMountArt &&
			bA0ContinuousMountAtlasActive && A0HeroArtMaterial != nullptr;
		if (bA0RideAnchorProbeValid)
		{
			const FVector RiderSeatWorld = A0HeroArt->GetComponentTransform().TransformPosition(
				FVector(0.0, A0RideSeatMeshLocalY, 0.0));
			const FVector MountSaddleWorld = A0MountArt->GetComponentTransform().TransformPosition(
				FVector(0.0, A0MountSaddleMeshLocalY, 0.0));
			FVector ScreenPlaneCorrection = MountSaddleWorld - RiderSeatWorld;
			// Preserve the depth offset so the mount remains in front of the rider,
			// while correcting the slope-induced X/Z drift in the side-camera view.
			ScreenPlaneCorrection.Y = 0.0;
			A0HeroArt->AddWorldOffset(ScreenPlaneCorrection);
		}
		if (bUseSnowboardArt)
		{
			SetA0FacingRotation(A0SnowboardArt, HeroSpinDegrees);
		}
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
	if (State.bRiding)
	{
		FrameIndex = 4;
	}
	else if (State.bFallen)
	{
		FrameIndex = 5;
	}
	else if (A0LandingPoseSecondsRemaining > 0.0)
	{
		FrameIndex = 4;
	}
	else if (A0TakeoffPoseSecondsRemaining > 0.0)
	{
		FrameIndex = 1;
	}
	else if (State.bAirborne)
	{
		// Use the full-board Glide silhouette for neutral airtime; the tighter
		// AirTuck crop lost board readability at the runtime camera scale.
		FrameIndex = FMath::Abs(State.RotationDegrees) > 20.0 ? 3 : 0;
	}

	if (bA0AtlasMaterialActive)
	{
		const FIntRect& Rect = SpriteRects[FrameIndex];
		const double WidthPx = static_cast<double>(Rect.Width());
		const double HeightPx = static_cast<double>(Rect.Height());
		// Preserve the approved right-facing +X direction and reverse V to keep
		// the pose upright in the side-camera atlas crop.
		A0HeroArtMaterial->SetVectorParameterValue(TEXT("A0UVScale"), FLinearColor(
			WidthPx / 2048.0, -HeightPx / 1024.0, 0.0f, 0.0f));
		A0HeroArtMaterial->SetVectorParameterValue(TEXT("A0UVOffset"), FLinearColor(
			static_cast<double>(Rect.Min.X) / 2048.0,
			1.0 - static_cast<double>(Rect.Min.Y) / 1024.0, 0.0f, 0.0f));
		const double SpriteWidthCm = State.bRiding ? 220.0 : 300.0;
		const double SpriteHeightCm = SpriteWidthCm * HeightPx / WidthPx;
		A0HeroArt->SetRelativeScale3D(FVector(SpriteWidthCm / 100.0, SpriteHeightCm / 100.0, 1.0));
		const double HeroBottomZcm = State.bRiding ? A0FallbackRideHeroBottomCm : -50.0;
		A0HeroArt->SetRelativeLocation(FVector(0.0, State.bRiding ? 280.0 : 260.0,
			HeroBottomZcm + SpriteHeightCm * 0.5));
	}

	const double SpinRadians = FMath::DegreesToRadians(State.RotationDegrees + RouteSlopeDegrees +
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

bool ANaiwaP5PrototypePawn::GetA0RideAnchorProbeWorldPoints(
	FVector& OutRiderSeatWorld, FVector& OutMountSaddleWorld) const
{
	if (!bA0RideAnchorProbeValid || !A0HeroArt || !A0MountArt)
	{
		return false;
	}

	OutRiderSeatWorld = A0HeroArt->GetComponentTransform().TransformPosition(
		FVector(0.0, A0RideSeatMeshLocalY, 0.0));
	OutMountSaddleWorld = A0MountArt->GetComponentTransform().TransformPosition(
		FVector(0.0, A0MountSaddleMeshLocalY, 0.0));
	return true;
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
