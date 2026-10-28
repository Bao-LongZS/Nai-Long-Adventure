#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NaiwaP4ArtIntegrationWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class UHorizontalBox;
class UProgressBar;
class UTextBlock;

/** Native UMG implementation of the approved P4 A3 HUD and card-choice layout. */
UCLASS()
class NAIWARUNTIME_API UNaiwaP4ArtIntegrationWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	void ToggleUpgradeOffer();
	void CycleTelegraphPreview();
	bool IsUpgradeOfferVisible() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildBattleHud();
	void BuildTelegraphPreview();
	void BuildUpgradeOffer();
	void AddCard(const FName CardId, int32 CardIndex);
	void SelectCard(int32 CardIndex);
	UFUNCTION()
	void OnCard0Clicked();
	UFUNCTION()
	void OnCard1Clicked();
	UFUNCTION()
	void OnCard2Clicked();
	UFUNCTION()
	void OnConfirmPreviewClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> UpgradeOfferPanel;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> UpgradeCardRow;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TelegraphPreviewText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmPreviewText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> UpgradeButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> UpgradeCardSurfaces;

	UPROPERTY(Transient)
	TArray<FName> UpgradeCardIds;

	int32 SelectedCardIndex = INDEX_NONE;
	int32 TelegraphPreviewIndex = INDEX_NONE;
};
