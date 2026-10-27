#include "NaiwaP3M1GameMode.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NaiwaP3GridSubsystem.h"
#include "NaiwaP3M1HUD.h"
#include "NaiwaP3PlayerPawn.h"

ANaiwaP3M1GameMode::ANaiwaP3M1GameMode()
{
	DefaultPawnClass = ANaiwaP3PlayerPawn::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass();
	HUDClass = ANaiwaP3M1HUD::StaticClass();
}

void ANaiwaP3M1GameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	if (UWorld* World = GetWorld())
	{
		if (UNaiwaP3GridSubsystem* Grid = World->GetSubsystem<UNaiwaP3GridSubsystem>())
		{
			Grid->InitializeM1();
		}
	}
}

void ANaiwaP3M1GameMode::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (UNaiwaP3GridSubsystem* Grid = World->GetSubsystem<UNaiwaP3GridSubsystem>())
		{
			Grid->InitializeM1();
		}
	}
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		EnsureCamera(PlayerController);
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("NaiwaP3M1SelfTest")))
	{
		UNaiwaP3GridSubsystem* Grid = GetWorld()->GetSubsystem<UNaiwaP3GridSubsystem>();
		FString Details;
		const bool bPassed = Grid && Grid->RunM1SelfTest(Details);
		UE_LOG(LogTemp, Display, TEXT("NAIWA_P3_M1_%s %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Details);
		FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1, TEXT("P3 M1 self-test complete"));
	}
}

void ANaiwaP3M1GameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	EnsureCamera(NewPlayer);
}

void ANaiwaP3M1GameMode::EnsureCamera(APlayerController* PlayerController)
{
	if (!PlayerController || !GetWorld())
	{
		return;
	}
	if (!M1Camera)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		M1Camera = GetWorld()->SpawnActor<ACameraActor>(
			ACameraActor::StaticClass(), FVector(0.0f, 0.0f, 1450.0f), FRotator(-90.0f, 0.0f, -90.0f), SpawnParameters);
		if (M1Camera)
		{
			UCameraComponent* Camera = M1Camera->GetCameraComponent();
			Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
			Camera->OrthoWidth = 1568.0f * 1.1f;
			Camera->bConstrainAspectRatio = false;
			M1Camera->SetActorLocation(FVector(0.0f, 0.0f, 1450.0f) +
				M1Camera->GetActorRightVector() * 360.0f);
		}
	}
	if (M1Camera)
	{
		PlayerController->SetViewTarget(M1Camera);
	}
}
