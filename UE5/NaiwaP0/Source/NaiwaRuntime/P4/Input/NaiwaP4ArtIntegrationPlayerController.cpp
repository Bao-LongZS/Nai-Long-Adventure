#include "NaiwaP4ArtIntegrationPlayerController.h"

#include "../Presentation/NaiwaP4ArtIntegrationWidget.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"

void ANaiwaP4ArtIntegrationPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ThisClass::ToggleUpgradeOffer);
		InputComponent->BindKey(EKeys::T, IE_Pressed, this, &ThisClass::CycleTelegraphPreview);
	}
}

void ANaiwaP4ArtIntegrationPlayerController::CycleTelegraphPreview()
{
	if (ArtIntegrationWidget)
	{
		ArtIntegrationWidget->CycleTelegraphPreview();
	}
}

void ANaiwaP4ArtIntegrationPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}

	ArtIntegrationWidget = CreateWidget<UNaiwaP4ArtIntegrationWidget>(this, UNaiwaP4ArtIntegrationWidget::StaticClass());
	if (ArtIntegrationWidget)
	{
		ArtIntegrationWidget->AddToViewport(5);
	}
}

void ANaiwaP4ArtIntegrationPlayerController::ToggleUpgradeOffer()
{
	if (!ArtIntegrationWidget)
	{
		return;
	}

	ArtIntegrationWidget->ToggleUpgradeOffer();
	if (ArtIntegrationWidget->IsUpgradeOfferVisible())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(ArtIntegrationWidget->TakeWidget());
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
		bShowMouseCursor = true;
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
}
