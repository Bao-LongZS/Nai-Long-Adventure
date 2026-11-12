#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NaiwaP4ArtIntegrationWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class UHorizontalBox;
class UProgressBar;
class UTextBlock;
class UTexture2D;

/** Native UMG P4 combat HUD; its isolated Beta6 path adds chapter settlement and continuation. */
UCLASS()
class NAIWARUNTIME_API UNaiwaP4ArtIntegrationWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	void ToggleUpgradeOffer();
	void CycleTelegraphPreview();
	void RestartRun();
	bool IsUpgradeOfferVisible() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildBattleHud();
	void BuildTelegraphPreview();
	void BuildBossHud();
	void BuildRunResultPanel();
	void BuildUpgradeOffer();
	void AddCard(const FName CardId, int32 CardIndex);
	void RebuildUpgradeCardChoices(const TArray<FName>& CardIds);
	void SelectCard(int32 CardIndex);
	void RefreshUpgradeCardStates();
	void RefreshUpgradeRerollState();
	void UpdateAttackWarnings(class UNaiwaP4ArenaSubsystem* Arena);
	void UpdateEnemyHealthBars(class UNaiwaP4ArenaSubsystem* Arena);
	void UpdatePlayerPortrait(const struct FNaiwaP4EntityState& Player);
	void UpdateBossAndResultUI(class UNaiwaP4ArenaSubsystem* Arena);
	UFUNCTION()
	void OnCard0Clicked();
	UFUNCTION()
	void OnCard1Clicked();
	UFUNCTION()
	void OnCard2Clicked();
	UFUNCTION()
	void OnConfirmPreviewClicked();
	UFUNCTION()
	void OnRerollUpgradeClicked();
	UFUNCTION()
	void OnRetryRunClicked();

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
	TObjectPtr<UTextBlock> EnergyAndChargeText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LevelProgressText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WaveProgressText;
	UPROPERTY(Transient)
	TObjectPtr<UImage> PlayerPortraitImage;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlayerPortraitStatusText;
	UPROPERTY(Transient)
	TObjectPtr<UImage> RunResultPortraitImage;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> IdlePortraitTexture;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LaughPortraitTexture;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> VictoryPortraitTexture;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DistressPortraitTexture;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DisplayedPlayerPortraitTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TelegraphPreviewText;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> BossHudPanel;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> BossHealthBar;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BossStatusText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BossHealthText;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> RunResultPanel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RunResultTitle;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RunResultHint;
	UPROPERTY(Transient)
	TObjectPtr<UButton> RunResultActionButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RunResultActionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmPreviewText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> UpgradeRerollButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UpgradeRerollText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> UpgradeButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> UpgradeCardSurfaces;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> UpgradeCardFaces;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> UpgradeCardDescriptions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> UpgradeCardChoiceActions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> UpgradeCardChoiceLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> UpgradeCardChoiceStates;

	UPROPERTY(Transient)
	TArray<FName> UpgradeCardIds;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UTextBlock>> AttackWarningMarkers;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UCanvasPanelSlot>> AttackWarningMarkerSlots;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UProgressBar>> EnemyHealthBars;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UCanvasPanelSlot>> EnemyHealthBarSlots;

	int32 SelectedCardIndex = INDEX_NONE;
	int32 DisplayedUpgradeOfferRevision = INDEX_NONE;
	int32 TelegraphPreviewIndex = INDEX_NONE;
	float UpgradeOfferSlideElapsed = 0.0f;
	float UpgradeCardWidth = 0.0f;
	float UpgradeCardHeight = 0.0f;
	bool bUpgradeOfferSlidingIn = false;
};
