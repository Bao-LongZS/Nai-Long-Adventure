#include "NaiwaGameMode.h"
#include "NaiwaCharacter.h"
#include "NaiwaHUD.h"

ANaiwaGameMode::ANaiwaGameMode()
{
    DefaultPawnClass = ANaiwaCharacter::StaticClass();
    HUDClass = ANaiwaHUD::StaticClass();
}
