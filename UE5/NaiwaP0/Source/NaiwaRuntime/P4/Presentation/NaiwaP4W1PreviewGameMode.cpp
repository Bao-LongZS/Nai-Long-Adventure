#include "NaiwaP4W1PreviewGameMode.h"

#include "../Core/NaiwaP4ArenaSubsystem.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerController.h"

ANaiwaP4W1PreviewGameMode::ANaiwaP4W1PreviewGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = APlayerController::StaticClass();
	HUDClass = nullptr;
	UpgradeDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
		TEXT("/Game/Naiwa/P4/Data/DT_P4_Upgrade_Smoke.DT_P4_Upgrade_Smoke")));
}

void ANaiwaP4W1PreviewGameMode::StartPlay()
{
	Super::StartPlay();

	UWorld* World = GetWorld();
	UNaiwaP4ArenaSubsystem* ArenaSubsystem = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	const UDataTable* Table = UpgradeDataTable.LoadSynchronous();
	if (!ArenaSubsystem || !Table)
	{
		UE_LOG(LogTemp, Error, TEXT("P4 W1 upgrade runtime probe failed: subsystem or DataTable is unavailable."));
		return;
	}

	FString Error;
	if (!ArenaSubsystem->LoadUpgradeDataTable(Table, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("P4 W1 upgrade runtime probe rejected DataTable: %s"), *Error);
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("P4 W1 upgrade runtime probe loaded %d cards from %s."),
		ArenaSubsystem->GetUpgradeCardIds().Num(), *Table->GetPathName());
	for (const FName CardId : ArenaSubsystem->GetUpgradeCardIds())
	{
		const FNaiwaP4UpgradeRow* Row = ArenaSubsystem->FindUpgradeRow(CardId);
		if (Row)
		{
			UE_LOG(LogTemp, Display, TEXT("P4 W1 card %s effect=%s P0=%.3f P1=%.3f P2=%.3f"),
				*CardId.ToString(), *Row->EffectKind.ToString(), Row->P0, Row->P1, Row->P2);
		}
	}
}
