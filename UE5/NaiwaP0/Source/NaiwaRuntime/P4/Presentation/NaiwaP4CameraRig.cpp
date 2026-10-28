#include "NaiwaP4CameraRig.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "../Core/NaiwaP4Settings.h"

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

	const float PitchRadians = FMath::DegreesToRadians(Settings->CameraPitchDegrees);
	const float HalfDepth = Settings->LogicalArenaDepth * 0.5f;
	const FVector ArenaCenter(Settings->LogicalArenaWidth * 0.5f, HalfDepth, 0.0f);
	const FVector CameraOffset(0.0f,
		-Settings->CameraDistance * FMath::Cos(PitchRadians),
		 Settings->CameraDistance * FMath::Sin(PitchRadians));

	SetActorLocation(ArenaCenter + CameraOffset);
	SetActorRotation(FRotator(-Settings->CameraPitchDegrees, 90.0f, 0.0f));
	CameraComponent->SetFieldOfView(Settings->CameraHorizontalFOVDegrees);
}
