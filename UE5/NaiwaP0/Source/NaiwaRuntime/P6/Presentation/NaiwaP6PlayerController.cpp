#include "NaiwaP6PlayerController.h"

void ANaiwaP6PlayerController::BeginPlay()
{
	Super::BeginPlay();
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	SetInputMode(FInputModeGameAndUI());
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(true);
}
