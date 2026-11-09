#include "NaiwaP3PlayerController.h"

#include "GameFramework/Pawn.h"

void ANaiwaP3PlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	UE_LOG(LogTemp, Display, TEXT("NAIWA_P3_INPUT_READY controller=%s pawn=%s mode=GameOnly"),
		*GetNameSafe(this), *GetNameSafe(GetPawn()));
}
