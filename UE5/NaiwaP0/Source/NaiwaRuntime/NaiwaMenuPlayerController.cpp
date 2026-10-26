#include "NaiwaMenuPlayerController.h"

#include "NaiwaMenuWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Widgets/SWidget.h"

ANaiwaMenuPlayerController::ANaiwaMenuPlayerController()
{
    static ConstructorHelpers::FObjectFinder<USoundBase> MenuMusicAsset(
        TEXT("/Game/Naiwa/UI/Audio/SW_Menu_JinFengYuLu.SW_Menu_JinFengYuLu"));
    if (MenuMusicAsset.Succeeded())
    {
        MenuMusic = MenuMusicAsset.Object;
    }
}

void ANaiwaMenuPlayerController::BeginPlay()
{
    Super::BeginPlay();
    bShowMouseCursor = true;
    MenuWidget = CreateWidget<UNaiwaMenuWidget>(this, UNaiwaMenuWidget::StaticClass());
    if (MenuWidget)
    {
        MenuWidget->SetIsFocusable(true);
        MenuWidget->SetVisibility(ESlateVisibility::Visible);
        MenuWidget->AddToViewport(100);
        FInputModeUIOnly InputMode;
        InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(InputMode);
    }

    if (MenuMusic)
    {
        MenuMusicComponent = UGameplayStatics::SpawnSound2D(
            this, MenuMusic, 0.35f, 1.0f, 0.0f, nullptr, false, true);
    }
}
