#include "NaiwaP4ArtIntegrationGameMode.h"

#include "../Core/NaiwaP4ArenaSubsystem.h"
#include "../Core/NaiwaP4Settings.h"
#include "../Input/NaiwaP4ArtIntegrationPlayerController.h"
#include "NaiwaP4ArtIntegrationPawn.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"

namespace
{
	const TCHAR* UpgradeDataTablePath = TEXT("/Game/Naiwa/P4/Data/DT_P4_Upgrade_Alpha.DT_P4_Upgrade_Alpha");
	const TCHAR* EnemyDataTablePath = TEXT("/Game/Naiwa/P4/Data/DT_P4_Enemy_Alpha.DT_P4_Enemy_Alpha");
	const TCHAR* WaveDataTablePath = TEXT("/Game/Naiwa/P4/Data/DT_P4_Waves_Alpha.DT_P4_Waves_Alpha");
	const FName GeneratedA4ActorTag(TEXT("NaiwaP4ArtIntegrationGenerated"));
	constexpr uint32 ArtPreviewSeed = 0x50344131u;
	constexpr float BaseA4ArenaDepth = 185.0f;

	void ApplyPreviewDepthScale(UWorld* World, const float DepthScale)
	{
		if (!World || FMath::IsNearlyEqual(DepthScale, 1.0f))
		{
			return;
		}

		int32 ScaledActorCount = 0;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			AStaticMeshActor* Actor = *It;
			if (!Actor || !Actor->ActorHasTag(GeneratedA4ActorTag))
			{
				continue;
			}

			FVector Location = Actor->GetActorLocation();
			Location.Y *= DepthScale;
			Actor->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);

			FVector Scale = Actor->GetActorScale3D();
			Scale.Y *= DepthScale;
			Actor->SetActorScale3D(Scale);
			++ScaledActorCount;
		}

		UE_LOG(LogTemp, Display, TEXT("P4 A4 preview scaled %d generated stage actors to %.1fx depth."),
			ScaledActorCount, DepthScale);
	}
}

ANaiwaP4ArtIntegrationGameMode::ANaiwaP4ArtIntegrationGameMode()
{
	DefaultPawnClass = ANaiwaP4ArtIntegrationPawn::StaticClass();
	PlayerControllerClass = ANaiwaP4ArtIntegrationPlayerController::StaticClass();
	HUDClass = nullptr;
}

void ANaiwaP4ArtIntegrationGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UWorld* World = GetWorld();
	const UNaiwaP4Settings* Settings = GetDefault<UNaiwaP4Settings>();
	if (Settings)
	{
		const float PreviewDepthScale = FMath::Max(1.0f, PreviewArenaDepth / BaseA4ArenaDepth);
		ApplyPreviewDepthScale(World, PreviewDepthScale);
	}

	UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	if (!Arena)
	{
		UE_LOG(LogTemp, Error, TEXT("P4 art integration preview could not find its arena subsystem."));
		return;
	}

	Arena->SetArenaDepthForPreview(PreviewArenaDepth);
	Arena->StartSimulation(ArtPreviewSeed);
	const UDataTable* UpgradeTable = LoadObject<UDataTable>(nullptr, UpgradeDataTablePath);
	FString CatalogError;
	if (!UpgradeTable || !Arena->LoadUpgradeDataTable(UpgradeTable, CatalogError))
	{
		UE_LOG(LogTemp, Error, TEXT("P4 art integration preview could not load Alpha upgrades: %s"),
			UpgradeTable ? *CatalogError : TEXT("DataTable asset is missing"));
	}
	const FString SourceDataDirectory = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT("../SourceAssets/P4/Data")));
	const FString UpgradeCsvPath = FPaths::Combine(SourceDataDirectory, TEXT("P4UpgradeAlpha.csv"));
	FString UpgradeCsvError;
	if (Arena->LoadUpgradeCsvFile(UpgradeCsvPath, UpgradeCsvError))
	{
		UE_LOG(LogTemp, Display, TEXT("P4 preview loaded Alpha upgrade pool from workspace CSV."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("P4 Alpha upgrade CSV was not loaded; using the DataTable fallback: %s"), *UpgradeCsvError);
	}
	const UDataTable* EnemyTable = LoadObject<UDataTable>(nullptr, EnemyDataTablePath);
	const UDataTable* WaveTable = LoadObject<UDataTable>(nullptr, WaveDataTablePath);
	bool bCombatDataLoaded = EnemyTable && WaveTable
		&& Arena->LoadCombatDataTables(EnemyTable, WaveTable, CatalogError);
	if (!bCombatDataLoaded)
	{
		const FString EnemyCsvPath = FPaths::Combine(SourceDataDirectory, TEXT("P4EnemyAlpha.csv"));
		const FString WaveCsvPath = FPaths::Combine(SourceDataDirectory, TEXT("P4WaveAlpha.csv"));
		bCombatDataLoaded = Arena->LoadCombatCsvFiles(EnemyCsvPath, WaveCsvPath, CatalogError);
		if (bCombatDataLoaded)
		{
			UE_LOG(LogTemp, Display, TEXT("P4 W3 loaded workspace CSV sources; native DataTable assets are not yet imported."));
		}
	}
	if (!bCombatDataLoaded)
	{
		UE_LOG(LogTemp, Error, TEXT("P4 W3 wave data could not be loaded: %s"), *CatalogError);
		Arena->StartGrayboxCombatPreview();
	}
	else if (!Arena->StartWaveCombatPreview())
	{
		UE_LOG(LogTemp, Error, TEXT("P4 W3 preview could not start the table-driven wave run."));
		Arena->StartGrayboxCombatPreview();
	}
}
