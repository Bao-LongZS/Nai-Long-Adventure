#pragma once

#include "CoreMinimal.h"
#include "NaiwaP4PlayerController.h"
#include "NaiwaP4ArtIntegrationPlayerController.generated.h"

class UNaiwaP4ArtIntegrationWidget;

/** P4 integration-preview controller with a preview-only upgrade-card toggle. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP4ArtIntegrationPlayerController final : public ANaiwaP4PlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void ToggleUpgradeOffer();
	void CycleTelegraphPreview();

	UPROPERTY(Transient)
	TObjectPtr<UNaiwaP4ArtIntegrationWidget> ArtIntegrationWidget;
};
