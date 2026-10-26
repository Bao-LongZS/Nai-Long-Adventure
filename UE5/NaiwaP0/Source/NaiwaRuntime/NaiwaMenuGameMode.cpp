#include "NaiwaMenuGameMode.h"

#include "NaiwaMenuPlayerController.h"

ANaiwaMenuGameMode::ANaiwaMenuGameMode()
{
    PlayerControllerClass = ANaiwaMenuPlayerController::StaticClass();
    DefaultPawnClass = nullptr;
    HUDClass = nullptr;
}
