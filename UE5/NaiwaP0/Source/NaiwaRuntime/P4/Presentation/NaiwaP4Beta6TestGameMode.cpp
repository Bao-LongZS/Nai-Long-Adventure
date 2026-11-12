#include "NaiwaP4Beta6TestGameMode.h"

#include "../Core/NaiwaP4ArenaSubsystem.h"
#include "../Input/NaiwaP4ArtIntegrationPlayerController.h"
#include "../Progress/NaiwaP4ProgressSubsystem.h"
#include "NaiwaP4ArtIntegrationPawn.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

namespace
{
	TSoftObjectPtr<UDataTable> MakeDataTableRef(const TCHAR* ObjectPath)
	{
		return TSoftObjectPtr<UDataTable>(FSoftObjectPath(ObjectPath));
	}
}

ANaiwaP4Beta6TestGameMode::ANaiwaP4Beta6TestGameMode()
{
	DefaultPawnClass = ANaiwaP4ArtIntegrationPawn::StaticClass();
	PlayerControllerClass = ANaiwaP4ArtIntegrationPlayerController::StaticClass();
	HUDClass = nullptr;

	ChapterTable = MakeDataTableRef(TEXT("/Game/Naiwa/P4/Data/DT_P4_Beta6_Chapters.DT_P4_Beta6_Chapters"));
	ThemeTable = MakeDataTableRef(TEXT("/Game/Naiwa/P4/Data/DT_P4_Beta6_Themes.DT_P4_Beta6_Themes"));
	WaveSpawnTable = MakeDataTableRef(TEXT("/Game/Naiwa/P4/Data/DT_P4_Beta6_WaveSpawns.DT_P4_Beta6_WaveSpawns"));
	EnemyTable = MakeDataTableRef(TEXT("/Game/Naiwa/P4/Data/DT_P4_Beta6_Enemies.DT_P4_Beta6_Enemies"));
	BossTable = MakeDataTableRef(TEXT("/Game/Naiwa/P4/Data/DT_P4_Beta6_Bosses.DT_P4_Beta6_Bosses"));
	BossAttackTable = MakeDataTableRef(TEXT("/Game/Naiwa/P4/Data/DT_P4_Beta6_BossAttacks.DT_P4_Beta6_BossAttacks"));
	UpgradeTable = MakeDataTableRef(TEXT("/Game/Naiwa/P4/Data/DT_P4_Upgrade_Alpha.DT_P4_Upgrade_Alpha"));
}

void ANaiwaP4Beta6TestGameMode::StartPlay()
{
	Super::StartPlay();

	UWorld* World = GetWorld();
	UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UNaiwaP4ProgressSubsystem* Progress = GameInstance
		? GameInstance->GetSubsystem<UNaiwaP4ProgressSubsystem>() : nullptr;
	if (!Arena || !Progress)
	{
		UE_LOG(LogTemp, Error, TEXT("P4 Beta6 test map could not find the arena or P4 progress subsystem."));
		return;
	}

	const UDataTable* Chapter = ChapterTable.LoadSynchronous();
	const UDataTable* Theme = ThemeTable.LoadSynchronous();
	const UDataTable* Waves = WaveSpawnTable.LoadSynchronous();
	const UDataTable* Enemies = EnemyTable.LoadSynchronous();
	const UDataTable* Bosses = BossTable.LoadSynchronous();
	const UDataTable* BossAttacks = BossAttackTable.LoadSynchronous();
	const UDataTable* Upgrades = UpgradeTable.LoadSynchronous();
	if (!Chapter || !Theme || !Waves || !Enemies || !Bosses || !BossAttacks || !Upgrades)
	{
		UE_LOG(LogTemp, Error, TEXT("P4 Beta6 test map is missing one or more required DataTables."));
		return;
	}

	FString Error;
	if (!Arena->LoadBeta6ProgressionTables(Chapter, Theme, Waves, Enemies, Bosses, BossAttacks, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("P4 Beta6 test map rejected chapter tables: %s"), *Error);
		return;
	}
	if (!Arena->LoadUpgradeDataTable(Upgrades, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("P4 Beta6 test map rejected the P4 upgrade table: %s"), *Error);
		return;
	}

	const bool bStarted = bResumeSavedRun && Progress->HasActiveRun()
		? Arena->ResumeBeta6Run(Error)
		: Arena->StartBeta6Run(RunSeed, Error);
	if (!bStarted)
	{
		UE_LOG(LogTemp, Error, TEXT("P4 Beta6 test run did not start: %s"), *Error);
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("P4 Beta6 test run started at chapter %d with seed %lld."),
		Arena->GetCurrentBeta6Chapter() ? Arena->GetCurrentBeta6Chapter()->Chapter.ChapterIndex : 0,
		RunSeed);
}
