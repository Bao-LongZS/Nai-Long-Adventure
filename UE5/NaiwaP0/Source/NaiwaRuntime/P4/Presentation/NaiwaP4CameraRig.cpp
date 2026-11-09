#include "NaiwaP4CameraRig.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "../Core/NaiwaP4Settings.h"
#include "NaiwaP4ArtIntegrationGameMode.h"

ANaiwaP4CameraRig::ANaiwaP4CameraRig()
{
	PrimaryActorTick.bCanEverTick = false;
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("P4Camera"));
	RootComponent = CameraComponent;
	CameraComponent->bUsePawnControlRotation = false;
}

void ANaiwaP4CameraRig::BeginPlay()
{
	Super::BeginPlay();
	ApplyCalibration();

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			PlayerController->SetViewTargetWithBlend(this, 0.0f);
		}
	}
}

void ANaiwaP4CameraRig::ApplyCalibration()
{
	const UNaiwaP4Settings* Settings = GetDefault<UNaiwaP4Settings>();
	if (!Settings)
	{
		return;
	}

	float ArenaDepth = Settings->LogicalArenaDepth;
	float CameraPitch = Settings->CameraPitchDegrees;
	if (const UWorld* World = GetWorld())
	{
		if (const ANaiwaP4ArtIntegrationGameMode* PreviewGameMode = Cast<ANaiwaP4ArtIntegrationGameMode>(World->GetAuthGameMode()))
		{
			ArenaDepth = PreviewGameMode->GetPreviewArenaDepth();
			CameraPitch = PreviewGameMode->GetPreviewCameraPitch();
		}
	}

	const float PitchRadians = FMath::DegreesToRadians(CameraPitch);
	const float HalfDepth = ArenaDepth * 0.5f;
	const FVector ArenaCenter(Settings->LogicalArenaWidth * 0.5f, HalfDepth, 0.0f);
	const FVector CameraOffset(0.0f,
		-Settings->CameraDistance * FMath::Cos(PitchRadians),
		 Settings->CameraDistance * FMath::Sin(PitchRadians));

	SetActorLocation(ArenaCenter + CameraOffset);
	SetActorRotation(FRotator(-CameraPitch, 90.0f, 0.0f));
	CameraComponent->SetFieldOfView(Settings->CameraHorizontalFOVDegrees);
	FPostProcessSettings& PostProcess = CameraComponent->PostProcessSettings;
	PostProcess.bOverride_DepthOfFieldScale = true;
	PostProcess.DepthOfFieldScale = 0.0f;
	PostProcess.bOverride_MotionBlurAmount = true;
	PostProcess.MotionBlurAmount = 0.0f;
	PostProcess.bOverride_BloomIntensity = true;
	PostProcess.BloomIntensity = 0.25f;
	PostProcess.bOverride_Sharpen = true;
	PostProcess.Sharpen = 0.7f;
}
