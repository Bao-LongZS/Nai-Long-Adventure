#include "NaiwaP3M2GameMode.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Parse.h"
#include "NaiwaMenuProgress.h"
#include "NaiwaP3M1Actors.h"
#include "NaiwaP3M2HUD.h"
#include "NaiwaP3M2RuntimeSubsystem.h"
#include "NaiwaP3M2WorldPresentation.h"
#include "NaiwaP3PlayerPawn.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace
{
	constexpr float P3NormalMusicPitch = 1.0f;
	// Two semitones above normal: raise urgency while keeping the same BGM track.
	constexpr float P3RageMusicPitch = 1.122462f;

	float P3DbToLinear(const float Db)
	{
		return FMath::Pow(10.0f, Db / 20.0f);
	}
}

namespace
{
	UTexture2D* GetEnemyPortrait(const ENaiwaP3M2EnemyKind Kind)
	{
		const TCHAR* AssetPath = nullptr;
		switch (Kind)
		{
		case ENaiwaP3M2EnemyKind::NaiCha:
			AssetPath = TEXT("/Game/Naiwa/P3/Portraits/Processed/SPR_Naicha_Chase_A.SPR_Naicha_Chase_A");
			break;
		case ENaiwaP3M2EnemyKind::NaiSha:
			AssetPath = TEXT("/Game/Naiwa/P3/Portraits/Processed/SPR_Naisha_Rage_A.SPR_Naisha_Rage_A");
			break;
		case ENaiwaP3M2EnemyKind::NaiHao:
			AssetPath = TEXT("/Game/Naiwa/P3/Portraits/Processed/SPR_Naihao_Neutral_A.SPR_Naihao_Neutral_A");
			break;
		case ENaiwaP3M2EnemyKind::NaiYu:
			AssetPath = TEXT("/Game/Naiwa/P3/Portraits/Processed/SPR_Naiyu_Neutral_A.SPR_Naiyu_Neutral_A");
			break;
		default:
			return nullptr;
		}
		return LoadObject<UTexture2D>(nullptr, AssetPath);
	}
}

ANaiwaP3M2GameMode::ANaiwaP3M2GameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ANaiwaP3PlayerPawn::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass();
	HUDClass = ANaiwaP3M2HUD::StaticClass();
}

void ANaiwaP3M2GameMode::InitGame(const FString& MapName, const FString& Options,
	FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	bCampaignRun = FParse::Param(*Options, TEXT("NaiwaLinkRun")) ||
		(GetWorld() && GetWorld()->URL.HasOption(TEXT("NaiwaLinkRun")));
	bFormalP3Level = MapName.Contains(TEXT("L_NaiwaP3_31")) || MapName.Contains(TEXT("L_NaiwaP3_32"));
	int32 StartAttemptCount = 0;
	FParse::Value(*Options, TEXT("Attempt="), StartAttemptCount);
	LevelId = MapName.Contains(TEXT("31")) ? 31 : 32;
	FParse::Value(*Options, TEXT("Level="), LevelId);
	LevelId = LevelId == 31 ? 31 : 32;
	bFogEnabled = LevelId == 32 && !FParse::Param(*Options, TEXT("NoFog"));
	if (UWorld* World = GetWorld())
	{
		if (UNaiwaP3M2RuntimeSubsystem* Runtime = World->GetSubsystem<UNaiwaP3M2RuntimeSubsystem>())
		{
			FString StartError;
			if (!Runtime->StartSession(LevelId, StartAttemptCount, bFogEnabled, StartError))
			{
				ErrorMessage = ErrorMessage.IsEmpty() ? StartError : ErrorMessage + TEXT("\n") + StartError;
				UE_LOG(LogTemp, Error, TEXT("M2 会话启动失败：%s"), *StartError);
			}
		}
	}
}

void ANaiwaP3M2GameMode::RestartSession()
{
	if (!GetWorld())
	{
		return;
	}
	UNaiwaP3M2RuntimeSubsystem* Runtime = GetWorld()->GetSubsystem<UNaiwaP3M2RuntimeSubsystem>();
	if (!Runtime)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(P3CampaignAdvanceTimer);
	bP3ResultRecorded = false;
	FString Error;
	if (!Runtime->StartSession(LevelId, Runtime->GetStartAttemptCount(), bFogEnabled, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("M2 本局重开失败：%s"), *Error);
		return;
	}
	bMusicLoopEnabled = true;
	PendingMusicFadeAtSeconds = -1.0;
	StartP3MusicSegment(GetWorld()->GetTimeSeconds(), 0.5f);
	if (MapActor)
	{
		MapActor->Destroy();
		MapActor = nullptr;
	}
	if (WorldPresentation)
	{
		WorldPresentation->Destroy();
		WorldPresentation = nullptr;
	}
	for (TObjectPtr<ANaiwaP3M1EnemyActor>& Enemy : EnemyActors)
	{
		if (Enemy)
		{
			Enemy->Destroy();
		}
	}
	EnemyActors.Reset();
	bPresentationBuilt = false;
	EnsurePresentation();
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		Runtime->RegisterPlayerPawn(Cast<ANaiwaP3PlayerPawn>(PlayerController->GetPawn()));
	}
}

void ANaiwaP3M2GameMode::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!GetWorld() || DeltaSeconds <= 0.0f) return;
	TickP3Audio(GetWorld()->GetTimeSeconds());
	const UNaiwaP3M2RuntimeSubsystem* Runtime = GetWorld()->GetSubsystem<UNaiwaP3M2RuntimeSubsystem>();
	const FNaiwaP3M2LevelSession* Session = Runtime ? Runtime->GetSession() : nullptr;
	if (Runtime && Runtime->GetRunState() == ENaiwaP3M2RunState::Won && !bP3ResultRecorded)
	{
		bP3ResultRecorded = true;
		if (bFormalP3Level && GetGameInstance())
		{
			if (UNaiwaProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UNaiwaProgressSubsystem>())
			{
				const FString LevelKey = LevelId == 31 ? TEXT("3-1") : TEXT("3-2");
				Progress->RecordLevelClear(LevelKey, 0, Runtime->GetCurrentTick() * 0.1f,
					3 - Runtime->GetPlayerLives(), Runtime->GetCollectedEggCount());
			}
		}
		if (bFormalP3Level && (bCampaignRun || LevelId == 32))
		{
			GetWorldTimerManager().SetTimer(P3CampaignAdvanceTimer, this,
				&ANaiwaP3M2GameMode::AdvanceP3Campaign, 1.5f, false);
		}
	}
	if (!M2Camera) return;
	if (!Runtime || !Session) return;

	const FVector BoardCenter = GridToWorld(Runtime->GetPlayerCell());
	FVector ScreenRight = M2Camera->GetActorRightVector();
	ScreenRight.Z = 0.0f;
	ScreenRight.Normalize();
	const FVector2D Shake = Runtime->GetExplosionShakeOffset();
	const FVector Target = FVector(BoardCenter.X, BoardCenter.Y, 2050.0f) +
		ScreenRight * 260.0f + FVector(Shake.X, Shake.Y, 0.0f);
	M2Camera->SetActorLocation(FMath::VInterpTo(M2Camera->GetActorLocation(), Target,
		DeltaSeconds, 5.5f));
}

void ANaiwaP3M2GameMode::AdvanceP3Campaign()
{
	if (!GetWorld()) return;
	FString MapPath;
	FString Options;
	if (LevelId == 31 && bCampaignRun)
	{
		MapPath = TEXT("/Game/Naiwa/P3/Maps/L_NaiwaP3_32");
		Options = TEXT("NaiwaLinkRun=1");
	}
	else if (LevelId == 32)
	{
		MapPath = TEXT("/Game/Naiwa/UI/L_NaiwaMainMenu");
		Options = TEXT("NaiwaMenuPage=levels");
	}
	else
	{
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("NAIWA_CAMPAIGN transition=P3 level=%d map=%s"), LevelId, *MapPath);
	UGameplayStatics::OpenLevel(this, FName(*MapPath), true, Options);
}

void ANaiwaP3M2GameMode::BeginPlay()
{
	Super::BeginPlay();
	StartP3Audio();
	EnsurePresentation();
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		EnsureCamera(PlayerController);
		if (UNaiwaP3M2RuntimeSubsystem* Runtime = GetWorld()->GetSubsystem<UNaiwaP3M2RuntimeSubsystem>())
		{
			Runtime->RegisterPlayerPawn(Cast<ANaiwaP3PlayerPawn>(PlayerController->GetPawn()));
		}
	}
}

USoundBase* ANaiwaP3M2GameMode::ResolveP3Audio(const FName AssetName)
{
	if (TObjectPtr<USoundBase>* Existing = P3AudioAssets.Find(AssetName)) return Existing->Get();
	FString AssetPath;
	if (AssetName == TEXT("SFX_P2_Miss"))
	{
		AssetPath = TEXT("/Game/Naiwa/P2/Audio/SFX_P2_Miss.SFX_P2_Miss");
	}
	else if (AssetName == TEXT("SFX_P2_FailureTrumpet"))
	{
		AssetPath = TEXT("/Game/Naiwa/P2/Audio/SFX_P2_FailureTrumpet.SFX_P2_FailureTrumpet");
	}
	else
	{
		AssetPath = FString::Printf(TEXT("/Game/Naiwa/P3/Audio/%s.%s"), *AssetName.ToString(), *AssetName.ToString());
	}
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *AssetPath);
	P3AudioAssets.Add(AssetName, Sound);
	if (!Sound) UE_LOG(LogTemp, Warning, TEXT("P3 audio asset missing: %s"), *AssetPath);
	return Sound;
}

USoundAttenuation* ANaiwaP3M2GameMode::ResolveP3Attenuation(const FName AssetName)
{
	FName AttenuationName = TEXT("ATT_P3_World");
	if (AssetName == TEXT("AMB_P3_Conveyor") || AssetName == TEXT("AMB_P3_Rain")) AttenuationName = TEXT("ATT_P3_Area");
	if (TObjectPtr<USoundAttenuation>* Existing = P3AttenuationAssets.Find(AttenuationName)) return Existing->Get();
	const FString Path = FString::Printf(TEXT("/Game/Naiwa/P3/Audio/Attenuation/%s.%s"),
		*AttenuationName.ToString(), *AttenuationName.ToString());
	USoundAttenuation* Attenuation = LoadObject<USoundAttenuation>(nullptr, *Path);
	P3AttenuationAssets.Add(AttenuationName, Attenuation);
	return Attenuation;
}

void ANaiwaP3M2GameMode::StartP3Audio()
{
	if (bP3AudioStarted || !GetWorld()) return;
	bP3AudioStarted = true;
	StartP3MusicSegment(GetWorld()->GetTimeSeconds(), 0.5f);
}

void ANaiwaP3M2GameMode::StartP3MusicSegment(const float NowSeconds, const float FadeSeconds)
{
	if (!bMusicLoopEnabled || !GetWorld()) return;
	USoundBase* Music = ResolveP3Audio(TEXT("BGM_P3_Level"));
	if (!Music)
	{
		NextMusicRecoveryAtSeconds = NowSeconds + 2.0;
		return;
	}

	const float Pitch = bP3RageAudioActive ? P3RageMusicPitch : P3NormalMusicPitch;
	UAudioComponent* Previous = BackgroundMusicComponent.Get();
	const bool bPreviousPlaying = IsValid(Previous) && Previous->IsPlaying();
	const float TargetVolume = P3DbToLinear(-8.0f);
	UAudioComponent* Next = UGameplayStatics::CreateSound2D(this, Music, 1.0f,
		Pitch, 1.5f, nullptr, false, false);
	if (!Next)
	{
		UE_LOG(LogTemp, Error, TEXT("P3 BGM: failed to create audio component for %s"), *GetNameSafe(Music));
		NextMusicRecoveryAtSeconds = NowSeconds + 2.0;
		return;
	}

	Next->SetLowPassFilterEnabled(false);
	if (FadeSeconds > 0.0f)
	{
		if (bPreviousPlaying)
		{
			Next->FadeIn(FadeSeconds, TargetVolume, 1.5f);
			Previous->FadeOut(FadeSeconds, 0.0f);
		}
		else
		{
			// Start the first segment directly. FadeIn also starts playback, but some
			// streamed SoundWaves can report stopped while their initial fade is queued.
			Next->SetVolumeMultiplier(TargetVolume);
			Next->Play(1.5f);
		}
	}
	else
	{
		Next->SetVolumeMultiplier(TargetVolume);
		Next->Play(1.5f);
	}

	BackgroundMusicComponent = Next;
	NextMusicRecoveryAtSeconds = NowSeconds + 2.0;
	MusicSectionStartedAtSeconds = NowSeconds;
	MusicSectionStartSourceSeconds = 1.5f;
	const float CrossfadeSourceSeconds = FMath::Min(FadeSeconds * Pitch, 0.5f * Pitch);
	const float LoopTriggerSourceSeconds = 195.5f - CrossfadeSourceSeconds;
	NextMusicLoopAtSeconds = NowSeconds + FMath::Max(0.0f,
		(LoopTriggerSourceSeconds - MusicSectionStartSourceSeconds) / Pitch);
	UE_LOG(LogTemp, Display,
		TEXT("P3 BGM segment requested: sound=%s playing=%d volume=%.3f pitch=%.3f start=1.50s"),
		*GetNameSafe(Music), Next->IsPlaying() ? 1 : 0, TargetVolume, Pitch);
}

void ANaiwaP3M2GameMode::PlayP3Audio2D(const FName AssetName, const float VolumeDb,
	const float Pitch, const float StartTime)
{
	if (USoundBase* Sound = ResolveP3Audio(AssetName))
	{
		UGameplayStatics::PlaySound2D(this, Sound, P3DbToLinear(VolumeDb), Pitch, StartTime);
	}
}

void ANaiwaP3M2GameMode::PlayP3AudioAtLocation(const FName AssetName, const FVector& Location,
	const float VolumeDb, const float Pitch, const float StartTime)
{
	if (USoundBase* Sound = ResolveP3Audio(AssetName))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, P3DbToLinear(VolumeDb), Pitch,
			StartTime, ResolveP3Attenuation(AssetName));
	}
}

void ANaiwaP3M2GameMode::SetP3ConveyorAudio(const bool bEnabled)
{
	if (!GetWorld()) return;
	if (!bEnabled)
	{
		bP3ConveyorAudioActive = false;
		NextConveyorLoopAtSeconds = 0.0;
		ConveyorStopAtSeconds = FMath::Max(GetWorld()->GetTimeSeconds(),
			ConveyorAudioStartedAtSeconds + 1.0);
		return;
	}

	if (!bP3ConveyorAudioActive)
	{
		ConveyorAudioStartedAtSeconds = GetWorld()->GetTimeSeconds();
	}
	bP3ConveyorAudioActive = true;
	ConveyorStopAtSeconds = -1.0;
	if (ConveyorAmbientComponent && ConveyorAmbientComponent->IsPlaying())
	{
		return;
	}
	StartP3ConveyorLoop(GetWorld()->GetTimeSeconds(), 0.3f);
}

void ANaiwaP3M2GameMode::StartP3ConveyorLoop(const float NowSeconds, const float FadeSeconds)
{
	if (!bP3ConveyorAudioActive || !GetWorld()) return;
	USoundBase* Sound = ResolveP3Audio(TEXT("AMB_P3_Conveyor"));
	if (!Sound) return;
	UAudioComponent* Previous = ConveyorAmbientComponent;
	UAudioComponent* Next = UGameplayStatics::SpawnSound2D(this, Sound,
		0.0f, 1.0f, 0.20f, nullptr, false, false);
	if (!Next) return;
	Next->bAutoDestroy = false;
	const float FadeInSeconds = Previous ? FadeSeconds : FMath::Min(FadeSeconds, 0.08f);
	Next->FadeIn(FadeInSeconds, P3DbToLinear(-6.0f));
	ConveyorAmbientComponent = Next;
	if (Previous && Previous != Next)
	{
		Previous->bAutoDestroy = true;
		Previous->FadeOut(FadeSeconds, 0.0f);
	}
	// Source loop region is 0.20–44.70s; overlap the last 0.5s for a seamless handoff.
	NextConveyorLoopAtSeconds = NowSeconds + 44.0;
}

void ANaiwaP3M2GameMode::SetP3RainAudio(const bool bEnabled)
{
	if (!GetWorld() || bP3RainAudioActive == bEnabled) return;
	bP3RainAudioActive = bEnabled;
	if (!bEnabled)
	{
		NextRainLoopAtSeconds = 0.0;
		if (RainAmbientComponent)
		{
			RainAmbientComponent->bAutoDestroy = true;
			RainAmbientComponent->FadeOut(1.5f, 0.0f);
			RainAmbientComponent = nullptr;
		}
		return;
	}
	StartP3RainLoop(GetWorld()->GetTimeSeconds(), 1.5f);
}

void ANaiwaP3M2GameMode::StartP3RainLoop(const float NowSeconds, const float FadeSeconds)
{
	if (!bP3RainAudioActive || !GetWorld()) return;
	USoundBase* Sound = ResolveP3Audio(TEXT("AMB_P3_Rain"));
	if (!Sound) return;
	UAudioComponent* Previous = RainAmbientComponent;
	UAudioComponent* Next = UGameplayStatics::SpawnSound2D(this, Sound, 0.0f,
		1.0f, 3.0f, nullptr, false, false);
	if (!Next) return;
	Next->bAutoDestroy = false;
	Next->FadeIn(FadeSeconds, P3DbToLinear(-2.0f));
	RainAmbientComponent = Next;
	if (Previous && Previous != Next)
	{
		Previous->bAutoDestroy = true;
		Previous->FadeOut(FadeSeconds, 0.0f);
	}
	// Rain loop region is 3.00–334.00s; overlap the last 0.5s.
	NextRainLoopAtSeconds = NowSeconds + 330.5;
}

void ANaiwaP3M2GameMode::SetP3RageAudio(const bool bEnabled)
{
	if (bP3RageAudioActive == bEnabled) return;
	const float PreviousPitch = bP3RageAudioActive ? P3RageMusicPitch : P3NormalMusicPitch;
	const float NewPitch = bEnabled ? P3RageMusicPitch : P3NormalMusicPitch;
	const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (BackgroundMusicComponent && GetWorld())
	{
		const float CurrentSourceSeconds = MusicSectionStartSourceSeconds +
			static_cast<float>(NowSeconds - MusicSectionStartedAtSeconds) * PreviousPitch;
		MusicSectionStartSourceSeconds = FMath::Clamp(CurrentSourceSeconds, 1.5f, 195.5f);
		MusicSectionStartedAtSeconds = NowSeconds;
	}
	bP3RageAudioActive = bEnabled;
	if (BackgroundMusicComponent)
	{
		BackgroundMusicComponent->SetPitchMultiplier(NewPitch);
		BackgroundMusicComponent->SetLowPassFilterEnabled(false);
		const float CrossfadeSeconds = 0.5f;
		const float LoopTriggerSourceSeconds = 195.5f - CrossfadeSeconds * NewPitch;
		NextMusicLoopAtSeconds = NowSeconds + FMath::Max(0.0f,
			(LoopTriggerSourceSeconds - MusicSectionStartSourceSeconds) / NewPitch);
	}
	if (bEnabled)
	{
		PlayP3Audio2D(TEXT("SFX_P3_RageStinger"), -3.0f);
		PlayP3Audio2D(TEXT("AMB_P3_LowDrone"), -17.0f);
		if (GetWorld()) NextRageDroneAtSeconds = GetWorld()->GetTimeSeconds() + 2.0;
	}
	UE_LOG(LogTemp, Display, TEXT("P3 audio rage mode=%d; BGM remains on the same track"), bEnabled ? 1 : 0);
}

void ANaiwaP3M2GameMode::PlayP3FailureAudio()
{
	PlayP3Audio2D(TEXT("SFX_P2_FailureTrumpet"), 0.0f);
}

void ANaiwaP3M2GameMode::PlayP3LevelClearAudio()
{
	bMusicLoopEnabled = false;
	PlayP3Audio2D(TEXT("SFX_P3_LevelClear"), -2.0f);
	if (GetWorld()) PendingMusicFadeAtSeconds = GetWorld()->GetTimeSeconds() + 0.3;
}

void ANaiwaP3M2GameMode::TickP3Audio(const float NowSeconds)
{
	if (bP3ConveyorAudioActive && NowSeconds >= NextConveyorLoopAtSeconds)
	{
		StartP3ConveyorLoop(NowSeconds, 0.5f);
	}
	if (!bP3ConveyorAudioActive && ConveyorAmbientComponent && ConveyorStopAtSeconds >= 0.0 &&
		NowSeconds >= ConveyorStopAtSeconds)
	{
		ConveyorAmbientComponent->bAutoDestroy = true;
		ConveyorAmbientComponent->FadeOut(0.3f, 0.0f);
		ConveyorAmbientComponent = nullptr;
		ConveyorStopAtSeconds = -1.0;
	}
	if (bP3RainAudioActive && NowSeconds >= NextRainLoopAtSeconds)
	{
		StartP3RainLoop(NowSeconds, 0.5f);
	}
	if (bP3RageAudioActive && NowSeconds >= NextRageDroneAtSeconds)
	{
		PlayP3Audio2D(TEXT("AMB_P3_LowDrone"), -17.0f);
		NextRageDroneAtSeconds = NowSeconds + 2.0;
	}
	if (PendingMusicFadeAtSeconds >= 0.0 && NowSeconds >= PendingMusicFadeAtSeconds)
	{
		if (BackgroundMusicComponent) BackgroundMusicComponent->FadeOut(2.0f, 0.0f);
		PendingMusicFadeAtSeconds = -1.0;
	}
	if (!bMusicLoopEnabled) return;
	if (!BackgroundMusicComponent || !BackgroundMusicComponent->IsPlaying())
	{
		if (NowSeconds < NextMusicRecoveryAtSeconds) return;
		UE_LOG(LogTemp, Warning, TEXT("P3 BGM component stopped; restarting the loop segment"));
		StartP3MusicSegment(NowSeconds, 0.5f);
		return;
	}
	if (NowSeconds >= NextMusicLoopAtSeconds)
	{
		StartP3MusicSegment(NowSeconds, 0.5f);
	}
}

void ANaiwaP3M2GameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	EnsurePresentation();
	EnsureCamera(NewPlayer);
	if (NewPlayer)
	{
		if (UNaiwaP3M2RuntimeSubsystem* Runtime = GetWorld()->GetSubsystem<UNaiwaP3M2RuntimeSubsystem>())
		{
			Runtime->RegisterPlayerPawn(Cast<ANaiwaP3PlayerPawn>(NewPlayer->GetPawn()));
		}
	}
}

void ANaiwaP3M2GameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	EnsurePresentation();
	EnsureCamera(NewPlayer);
	if (NewPlayer)
	{
		if (UNaiwaP3M2RuntimeSubsystem* Runtime = GetWorld()->GetSubsystem<UNaiwaP3M2RuntimeSubsystem>())
		{
			Runtime->RegisterPlayerPawn(Cast<ANaiwaP3PlayerPawn>(NewPlayer->GetPawn()));
		}
	}
}

void ANaiwaP3M2GameMode::EnsurePresentation()
{
	if (bPresentationBuilt || !GetWorld())
	{
		return;
	}
	UNaiwaP3M2RuntimeSubsystem* Runtime = GetWorld()->GetSubsystem<UNaiwaP3M2RuntimeSubsystem>();
	const FNaiwaP3M2LevelSession* Session = Runtime ? Runtime->GetSession() : nullptr;
	if (!Session)
	{
		return;
	}

	const FNaiwaP3M2SeededLevel& Level = Session->GetLevel();
	TArray<FString> Rows;
	Rows.Reserve(Level.GridHeight);
	for (int32 Y = 0; Y < Level.GridHeight; ++Y)
	{
		FString Row;
		Row.Reserve(Level.GridWidth);
		for (int32 X = 0; X < Level.GridWidth; ++X)
		{
			switch (Level.GetCell(FIntPoint(X, Y)))
			{
			case ENaiwaP3M2Cell::HardBrick: Row.AppendChar(TCHAR('H')); break;
			case ENaiwaP3M2Cell::SoftBrick: Row.AppendChar(TCHAR('S')); break;
			case ENaiwaP3M2Cell::ItemBrick: Row.AppendChar(TCHAR('I')); break;
			case ENaiwaP3M2Cell::Floor:
			default: Row.AppendChar(TCHAR('.')); break;
			}
		}
		Rows.Add(MoveTemp(Row));
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MapActor = GetWorld()->SpawnActor<ANaiwaP3M1MapActor>(
		ANaiwaP3M1MapActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	if (!MapActor)
	{
		return;
	}
	MapActor->BuildMap(Rows, false, bFogEnabled);
	WorldPresentation = GetWorld()->SpawnActor<ANaiwaP3M2WorldPresentation>(
		ANaiwaP3M2WorldPresentation::StaticClass(), FVector::ZeroVector,
		FRotator::ZeroRotator, SpawnParameters);
	if (WorldPresentation)
	{
		WorldPresentation->Build(Level);
	}

	EnemyActors.Reset(Level.Enemies.Num());
	for (int32 Index = 0; Index < Level.Enemies.Num(); ++Index)
	{
		ANaiwaP3M1EnemyActor* Enemy = GetWorld()->SpawnActor<ANaiwaP3M1EnemyActor>(
			ANaiwaP3M1EnemyActor::StaticClass(), GridToWorld(Level.Enemies[Index].Cell),
			FRotator::ZeroRotator, SpawnParameters);
		if (Enemy)
		{
			Enemy->SetPortrait(GetEnemyPortrait(Level.Enemies[Index].Kind));
		}
		EnemyActors.Add(Enemy);
	}
	TArray<ANaiwaP3M1EnemyActor*> RawEnemyActors;
	RawEnemyActors.Reserve(EnemyActors.Num());
	for (const TObjectPtr<ANaiwaP3M1EnemyActor>& Enemy : EnemyActors)
	{
		RawEnemyActors.Add(Enemy.Get());
	}
	Runtime->RegisterPresentationActors(MapActor, RawEnemyActors, WorldPresentation);
	bPresentationBuilt = true;
	UE_LOG(LogTemp, Display, TEXT("M2 系统布局就绪：尝试=%d，种子=%d，布局=%s"),
		Level.AttemptCount, Level.Seed, *Level.LayoutHash);
}

void ANaiwaP3M2GameMode::EnsureCamera(APlayerController* PlayerController)
{
	if (!PlayerController || !GetWorld())
	{
		return;
	}
	if (!M2Camera)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		M2Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),
			FVector(0.0f, 0.0f, 2050.0f), FRotator(-90.0f, 0.0f, -90.0f), SpawnParameters);
		if (M2Camera)
		{
			UCameraComponent* Camera = M2Camera->GetCameraComponent();
			Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
			// Keep the board close enough for tile detail to read clearly around the player.
			const FNaiwaP3M2SeededLevel& Level = GetWorld()->GetSubsystem<UNaiwaP3M2RuntimeSubsystem>()->GetSession()->GetLevel();
			const float Aspect = 16.0f / 9.0f;
			const float PreviousOrthoWidth = LevelId == 31 ?
				FMath::Max((Level.GridWidth * 100.0f + 160.0f), (Level.GridHeight * 100.0f + 100.0f) * Aspect) : 1650.0f;
			Camera->OrthoWidth = PreviousOrthoWidth * 0.656f * 0.8f * 1.1f;
			Camera->bConstrainAspectRatio = false;
			FPostProcessSettings& PostProcess = Camera->PostProcessSettings;
			PostProcess.bOverride_AutoExposureMethod = true;
			PostProcess.AutoExposureMethod = AEM_Manual;
			PostProcess.bOverride_AutoExposureBias = true;
			PostProcess.AutoExposureBias = 0.0f;
			// Manual exposure must not use the default physical-camera EV100 (~10),
			// which would crush this deliberately dark, unlit board palette to black.
			PostProcess.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
			PostProcess.AutoExposureApplyPhysicalCameraExposure = false;
			PostProcess.bOverride_BloomIntensity = true;
			PostProcess.BloomIntensity = 0.0f;
			PostProcess.bOverride_MotionBlurAmount = true;
			PostProcess.MotionBlurAmount = 0.0f;
			PostProcess.bOverride_ToneCurveAmount = true;
			PostProcess.ToneCurveAmount = 0.0f;
			PostProcess.bOverride_ExpandGamut = true;
			PostProcess.ExpandGamut = 0.0f;
			UNaiwaP3M2RuntimeSubsystem* Runtime = GetWorld()->GetSubsystem<UNaiwaP3M2RuntimeSubsystem>();
			const FVector InitialFocus = Runtime ? GridToWorld(Runtime->GetPlayerCell()) : FVector::ZeroVector;
			FVector ScreenRight = M2Camera->GetActorRightVector();
			ScreenRight.Z = 0.0f;
			ScreenRight.Normalize();
			M2Camera->SetActorLocation(FVector(InitialFocus.X, InitialFocus.Y, 2050.0f) + ScreenRight * 260.0f);
		}
	}
	if (M2Camera)
	{
		PlayerController->SetViewTarget(M2Camera);
	}
}

FVector ANaiwaP3M2GameMode::GridToWorld(const FIntPoint& Cell) const
{
	const UNaiwaP3M2RuntimeSubsystem* Runtime = GetWorld() ?
		GetWorld()->GetSubsystem<UNaiwaP3M2RuntimeSubsystem>() : nullptr;
	const FNaiwaP3M2LevelSession* Session = Runtime ? Runtime->GetSession() : nullptr;
	const FNaiwaP3M2SeededLevel* Level = Session ? &Session->GetLevel() : nullptr;
	const int32 GridWidth = Level ? Level->GridWidth : FNaiwaP3M2SeededLevel::Width;
	const int32 GridHeight = Level ? Level->GridHeight : FNaiwaP3M2SeededLevel::Height;
	return FVector((Cell.X - (GridWidth - 1) * 0.5f) * 100.0f,
		(Cell.Y - (GridHeight - 1) * 0.5f) * 100.0f, 0.0f);
}
