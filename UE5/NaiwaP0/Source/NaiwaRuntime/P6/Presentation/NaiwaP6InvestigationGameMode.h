#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NaiwaP6InvestigationGameMode.generated.h"

class ACameraActor;
class APlayerController;

UCLASS()
class NAIWARUNTIME_API ANaiwaP6InvestigationGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANaiwaP6InvestigationGameMode();
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

private:
	void EnsureCamera(APlayerController* PlayerController);
	void BuildM2Graybox();
	void RunM1ProbeIfRequested();
	void RunM2ProbeIfRequested();
	void RunM3ProbeIfRequested();
	void RunM4ProbeIfRequested();

	UPROPERTY()
	TObjectPtr<ACameraActor> InvestigationCamera;
};
