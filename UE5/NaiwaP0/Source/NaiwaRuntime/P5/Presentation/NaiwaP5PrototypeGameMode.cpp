#include "NaiwaP5PrototypeGameMode.h"

#include "Engine/World.h"
#include "NaiwaP5HUD.h"
#include "NaiwaP5PrototypePawn.h"
#include "NaiwaP5PrototypeTrackActor.h"
#include "../Core/NaiwaP5RunSubsystem.h"

ANaiwaP5PrototypeGameMode::ANaiwaP5PrototypeGameMode()
{
	DefaultPawnClass = ANaiwaP5PrototypePawn::StaticClass();
	HUDClass = ANaiwaP5HUD::StaticClass();
}

void ANaiwaP5PrototypeGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (UNaiwaP5RunSubsystem* RunSubsystem = World->GetSubsystem<UNaiwaP5RunSubsystem>())
		{
			RunSubsystem->StartM1PrototypeRun(0u);
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<ANaiwaP5PrototypeTrackActor>(ANaiwaP5PrototypeTrackActor::StaticClass(),
			FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	}
}
