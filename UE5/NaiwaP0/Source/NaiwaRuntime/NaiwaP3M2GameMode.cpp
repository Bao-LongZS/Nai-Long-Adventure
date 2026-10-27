#include "NaiwaP3M2GameMode.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Parse.h"
#include "NaiwaP3M1Actors.h"
#include "NaiwaP3M2HUD.h"
#include "NaiwaP3M2RuntimeSubsystem.h"
#include "NaiwaP3M2WorldPresentation.h"
#include "NaiwaP3PlayerPawn.h"

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
	FString Error;
	if (!Runtime->StartSession(LevelId, Runtime->GetStartAttemptCount(), bFogEnabled, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("M2 本局重开失败：%s"), *Error);
		return;
	}
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
	if (!M2Camera || !GetWorld() || DeltaSeconds <= 0.0f) return;
	const UNaiwaP3M2RuntimeSubsystem* Runtime = GetWorld()->GetSubsystem<UNaiwaP3M2RuntimeSubsystem>();
	const FNaiwaP3M2LevelSession* Session = Runtime ? Runtime->GetSession() : nullptr;
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

void ANaiwaP3M2GameMode::BeginPlay()
{
	Super::BeginPlay();
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
