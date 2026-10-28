#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NaiwaP3M2GameMode.generated.h"

class ACameraActor;
class APlayerController;
class ANaiwaP3M1EnemyActor;
class ANaiwaP3M1MapActor;
class ANaiwaP3M2WorldPresentation;
class UAudioComponent;
class USoundAttenuation;
class USoundBase;

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
	void StartP3Audio();
	void PlayP3Audio2D(FName AssetName, float VolumeDb = 0.0f, float Pitch = 1.0f, float StartTime = 0.0f);
	void PlayP3AudioAtLocation(FName AssetName, const FVector& Location,
		float VolumeDb = 0.0f, float Pitch = 1.0f, float StartTime = 0.0f);
	void SetP3ConveyorAudio(bool bEnabled);
	void SetP3RainAudio(bool bEnabled);
	void SetP3RageAudio(bool bEnabled);
	void PlayP3FailureAudio();
	void PlayP3LevelClearAudio();
	bool IsCampaignRun() const { return bCampaignRun; }

private:
	void EnsurePresentation();
	void EnsureCamera(APlayerController* PlayerController);
	FVector GridToWorld(const FIntPoint& Cell) const;
	USoundBase* ResolveP3Audio(FName AssetName);
	USoundAttenuation* ResolveP3Attenuation(FName AssetName);
	void TickP3Audio(float NowSeconds);
	void StartP3MusicSegment(float NowSeconds, float FadeSeconds);
	void StartP3ConveyorLoop(float NowSeconds, float FadeSeconds);
	void StartP3RainLoop(float NowSeconds, float FadeSeconds);
	void AdvanceP3Campaign();

	UPROPERTY()
	TObjectPtr<ACameraActor> M2Camera;

	UPROPERTY()
	TObjectPtr<ANaiwaP3M1MapActor> MapActor;

	UPROPERTY()
	TObjectPtr<ANaiwaP3M2WorldPresentation> WorldPresentation;

	UPROPERTY()
	TArray<TObjectPtr<ANaiwaP3M1EnemyActor>> EnemyActors;

	UPROPERTY()
	TObjectPtr<UAudioComponent> BackgroundMusicComponent;

	UPROPERTY()
	TObjectPtr<UAudioComponent> ConveyorAmbientComponent;

	UPROPERTY()
	TObjectPtr<UAudioComponent> RainAmbientComponent;

	UPROPERTY()
	TMap<FName, TObjectPtr<USoundBase>> P3AudioAssets;

	UPROPERTY()
	TMap<FName, TObjectPtr<USoundAttenuation>> P3AttenuationAssets;

	FTimerHandle P3CampaignAdvanceTimer;

	double NextMusicLoopAtSeconds = 0.0;
	double NextMusicRecoveryAtSeconds = 0.0;
	double MusicSectionStartedAtSeconds = 0.0;
	double NextConveyorLoopAtSeconds = 0.0;
	double ConveyorAudioStartedAtSeconds = 0.0;
	double ConveyorStopAtSeconds = -1.0;
	double NextRainLoopAtSeconds = 0.0;
	float MusicSectionStartSourceSeconds = 1.5f;
	double NextRageDroneAtSeconds = 0.0;
	double PendingMusicFadeAtSeconds = -1.0;
	bool bP3AudioStarted = false;
	bool bFormalP3Level = false;
	bool bCampaignRun = false;
	bool bP3ResultRecorded = false;
	bool bP3ConveyorAudioActive = false;
	bool bP3RainAudioActive = false;
	bool bMusicLoopEnabled = true;
	bool bP3RageAudioActive = false;

	bool bFogEnabled = true;
	bool bPresentationBuilt = false;
	int32 LevelId = 32;
};
