#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NaiwaP3M2GameMode.generated.h"

class ACameraActor;
class APlayerController;
class ANaiwaP3M1EnemyActor;
class ANaiwaP3M1MapActor;
class ANaiwaP3M2WorldPresentation;

UCLASS()
class NAIWARUNTIME_API ANaiwaP3M2GameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANaiwaP3M2GameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	void RestartSession();

private:
	void EnsurePresentation();
	void EnsureCamera(APlayerController* PlayerController);
	FVector GridToWorld(const FIntPoint& Cell) const;

	UPROPERTY()
	TObjectPtr<ACameraActor> M2Camera;

	UPROPERTY()
	TObjectPtr<ANaiwaP3M1MapActor> MapActor;

	UPROPERTY()
	TObjectPtr<ANaiwaP3M2WorldPresentation> WorldPresentation;

	UPROPERTY()
	TArray<TObjectPtr<ANaiwaP3M1EnemyActor>> EnemyActors;

	bool bFogEnabled = true;
	bool bPresentationBuilt = false;
	int32 LevelId = 32;
};
