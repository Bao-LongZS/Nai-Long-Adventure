#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NaiwaP3M1GameMode.generated.h"

class ACameraActor;
class APlayerController;

UCLASS()
class NAIWARUNTIME_API ANaiwaP3M1GameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANaiwaP3M1GameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

private:
	void EnsureCamera(APlayerController* PlayerController);

	UPROPERTY()
	TObjectPtr<ACameraActor> M1Camera;
};
