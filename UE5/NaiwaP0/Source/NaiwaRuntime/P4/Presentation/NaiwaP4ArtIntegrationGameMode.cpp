#include "NaiwaP4ArtIntegrationGameMode.h"

#include "../Core/NaiwaP4ArenaSubsystem.h"
#include "../Input/NaiwaP4ArtIntegrationPlayerController.h"
#include "NaiwaP4ArtIntegrationPawn.h"
#include "Engine/DataTable.h"

namespace
{
	const TCHAR* UpgradeDataTablePath = TEXT("/Game/Naiwa/P4/Data/DT_P4_Upgrade_Alpha.DT_P4_Upgrade_Alpha");
	constexpr uint32 ArtPreviewSeed = 0x50344131u;
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
	UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	if (!Arena)
	{
		UE_LOG(LogTemp, Error, TEXT("P4 art integration preview could not find its arena subsystem."));
		return;
	}

	Arena->StartSimulation(ArtPreviewSeed);
	const UDataTable* UpgradeTable = LoadObject<UDataTable>(nullptr, UpgradeDataTablePath);
	FString CatalogError;
	if (!UpgradeTable || !Arena->LoadUpgradeDataTable(UpgradeTable, CatalogError))
	{
		UE_LOG(LogTemp, Error, TEXT("P4 art integration preview could not load Alpha upgrades: %s"),
			UpgradeTable ? *CatalogError : TEXT("DataTable asset is missing"));
	}
}
