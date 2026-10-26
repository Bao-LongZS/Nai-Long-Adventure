#include "NaiwaP1PlayerController.h"
#include "NaiwaP1CameraManager.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

ANaiwaP1PlayerController::ANaiwaP1PlayerController()
{
    PlayerCameraManagerClass = ANaiwaP1CameraManager::StaticClass();
}

void ANaiwaP1PlayerController::BeginPlay()
{
    Super::BeginPlay();

    bShowMouseCursor = false;
    SetInputMode(FInputModeGameOnly());
    SetIgnoreMoveInput(false);
    SetIgnoreLookInput(false);
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_INPUT_READY controller=%s pawn=%s mode=GameOnly"),
        *GetNameSafe(this), *GetNameSafe(GetPawn()));
}

void ANaiwaP1PlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (InputComponent)
    {
        InputComponent->BindKey(EKeys::Escape, IE_Pressed, this,
            &ANaiwaP1PlayerController::ReturnToLevelSelect);
    }
}

void ANaiwaP1PlayerController::ReturnToLevelSelect()
{
    UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Naiwa/UI/L_NaiwaMainMenu")),
        true, TEXT("NaiwaMenuPage=levels"));
}
