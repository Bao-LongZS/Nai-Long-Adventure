#include "NaiwaP4ArtIntegrationWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "../Core/NaiwaP4ArenaState.h"
#include "../Core/NaiwaP4ArenaSubsystem.h"
#include "../Core/NaiwaP4Settings.h"
#include "../Data/NaiwaP4DataTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"

namespace
{
	const FLinearColor InkColor(0.286f, 0.216f, 0.180f, 1.0f);
	const FLinearColor PaperColor(1.0f, 0.973f, 0.914f, 0.96f);
	const FLinearColor MutedColor(0.43f, 0.37f, 0.31f, 1.0f);
	const FLinearColor BulletColor(0.835f, 0.604f, 0.231f, 1.0f);
	const FLinearColor ChargeColor(0.471f, 0.659f, 0.416f, 1.0f);
	const FLinearColor LaughColor(0.851f, 0.510f, 0.447f, 1.0f);
	const FLinearColor AttackHintColor(0.64f, 0.25f, 0.12f, 1.0f);
	const FLinearColor EnemyWarningColor(0.98f, 0.045f, 0.025f, 1.0f);
	constexpr float StaticEnemyFrameHeightPx = 400.0f;
	constexpr float StaticEnemyFrameSizePx = 512.0f;
	constexpr float NaiDingPreviewCanvasHeightUU = 215.0f;
	constexpr float NaiDunPreviewCanvasHeightUU = 309.0f;
	constexpr float CharacterPresentationScale = 0.82f;
	constexpr float EnemyWarningHeadroomUU = 18.0f;

	UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, const int32 Size,
		const FLinearColor& Color, const bool bBold = false)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Block->SetText(FText::FromString(Text));
		Block->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size));
		Block->SetColorAndOpacity(Color);
		Block->SetAutoWrapText(true);
		return Block;
	}

	void StyleButton(UButton* Button)
	{
		FButtonStyle Style;
		Style.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), 12.0f));
		Style.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.08f), 12.0f));
		Style.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.10f), 12.0f));
		Style.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.2f), 12.0f));
		Style.SetNormalPadding(FMargin(0.0f));
		Style.SetPressedPadding(FMargin(0.0f));
		Button->SetStyle(Style);
	}

	FLinearColor CategoryColor(const FName CategoryId)
	{
		if (CategoryId == TEXT("charge")) return ChargeColor;
		if (CategoryId == TEXT("laugh")) return LaughColor;
		return BulletColor;
	}

	FString CategoryTexturePath(const FName CategoryId)
	{
		return FString::Printf(TEXT("/Game/Naiwa/P4/UI/Alpha/RuntimeReady/T_P4_A3_CategoryMark_%s.T_P4_A3_CategoryMark_%s"),
			*CategoryId.ToString(), *CategoryId.ToString());
	}

	FString BuildUpgradeCardDescription(const FNaiwaP4UpgradeRow& Row, const int32 CurrentRank,
		const FNaiwaP4EntityState* Player)
	{
		const int32 Rank = FMath::Max(0, CurrentRank);
		const int32 NextRank = Rank + 1;
		if (Row.EffectKind == TEXT("charge_capacity_recovery"))
		{
			const int32 BaseCapacity = FMath::Max(1, FMath::RoundToInt(Row.P0) - 1);
			const int32 CurrentCapacity = Player ? Player->DashChargesMax : BaseCapacity + Rank;
			const int32 NextCapacity = CurrentCapacity + 1;
			const float CurrentRecovery = Rank > 0
				? Row.P1 + 0.10f * static_cast<float>(Rank - 1)
				: 0.24f;
			const float NextRecovery = Row.P1 + 0.10f * static_cast<float>(Rank);
			return FString::Printf(TEXT("冲撞上限：%d → %d 次\n撞后硬直：%.2f → %.2f 秒"),
				CurrentCapacity, NextCapacity, CurrentRecovery, NextRecovery);
		}
		if (Row.EffectKind == TEXT("projectile_bounce"))
		{
			const int32 BouncesPerRank = FMath::Max(1, FMath::RoundToInt(Row.P0));
			const int32 CurrentBounces = Player ? Player->ProjectileBouncesPerShot : Rank * BouncesPerRank;
			const int32 NextBounces = CurrentBounces + BouncesPerRank;
			const float NextDamageScale = Row.P1 + 0.10f * static_cast<float>(Rank);
			if (Rank == 0)
			{
				return FString::Printf(TEXT("首次获得：弹射 %d 次；跳弹伤害 ×%.2f。"),
					NextBounces, NextDamageScale);
			}
			const float CurrentDamageScale = Player
				? Player->ProjectileBounceDamageScale
				: Row.P1 + 0.10f * static_cast<float>(Rank - 1);
			return FString::Printf(TEXT("弹射次数：%d → %d 次\n跳弹伤害：×%.2f → ×%.2f"),
				CurrentBounces, NextBounces, CurrentDamageScale, NextDamageScale);
		}
		if (Row.EffectKind == TEXT("laugh_energy_on_hit"))
		{
			const int32 CurrentEnergy = Player ? Player->EnergyOnHitHundredths / 100
				: FMath::RoundToInt(Row.P0 * static_cast<float>(Rank));
			const int32 NextEnergy = FMath::RoundToInt(Row.P0 * static_cast<float>(NextRank));
			return FString::Printf(TEXT("受击回能：%d → %d 点\n无敌期内不重复（%.2f 秒）"),
				CurrentEnergy, NextEnergy, Row.P1);
		}
		if (Row.EffectKind == TEXT("melee_damage"))
		{
			const int32 CurrentDamage = Player ? Player->MeleeDamage : 20 + FMath::RoundToInt(Row.P0 * Rank);
			const int32 NextDamage = CurrentDamage + FMath::Max(0, FMath::RoundToInt(Row.P0));
			return FString::Printf(TEXT("J 近战伤害：%d → %d 点"), CurrentDamage, NextDamage);
		}

		return Row.Description.ToString();
	}
}

void UNaiwaP4ArtIntegrationWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("P4ArtIntegrationRoot"));
	WidgetTree->RootWidget = RootCanvas;
	BuildBattleHud();
	BuildTelegraphPreview();
	BuildBossHud();
	BuildUpgradeOffer();
	BuildRunResultPanel();
}

void UNaiwaP4ArtIntegrationWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bUpgradeOfferSlidingIn && UpgradeOfferPanel)
	{
		UpgradeOfferSlideElapsed += FMath::Max(InDeltaTime, 0.0f);
		const float Alpha = FMath::Clamp(UpgradeOfferSlideElapsed / 0.22f, 0.0f, 1.0f);
		const float EaseOut = 1.0f - FMath::Pow(1.0f - Alpha, 3.0f);
		UpgradeOfferPanel->SetRenderTranslation(FVector2D(0.0f, FMath::Lerp(-56.0f, 0.0f, EaseOut)));
		if (Alpha >= 1.0f)
		{
			bUpgradeOfferSlidingIn = false;
		}
	}

	UWorld* World = GetWorld();
	UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	if (!Arena || !HealthBar || !HealthText || !EnergyAndChargeText)
	{
		return;
	}
	if (Arena->GetState().PendingUpgradeChoices > 0
		&& (!IsUpgradeOfferVisible() || DisplayedUpgradeOfferRevision != Arena->GetState().UpgradeOfferRevision))
	{
		if (!IsUpgradeOfferVisible())
		{
			ToggleUpgradeOffer();
		}
		else
		{
			RebuildUpgradeCardChoices(Arena->GetState().CurrentUpgradeOfferCardIds);
			DisplayedUpgradeOfferRevision = Arena->GetState().UpgradeOfferRevision;
		}
	}

	const FNaiwaP4EntityState* Player = Arena->GetState().Entities.FindByPredicate([](const FNaiwaP4EntityState& Entity)
	{
		return Entity.Kind == ENaiwaP4EntityKind::Player;
	});
	if (!Player)
	{
		return;
	}

	const int32 MaxHP = FMath::Max(Player->MaxHP, 1);
	HealthBar->SetPercent(FMath::Clamp(static_cast<float>(Player->HP) / MaxHP, 0.0f, 1.0f));
	HealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Player->HP, MaxHP)));
	EnergyAndChargeText->SetText(FText::FromString(FString::Printf(
		TEXT("能量 %d / 100  ·  冲撞 %d / %d"), Player->EnergyHundredths / 100,
		Player->DashCharges, Player->DashChargesMax)));
	if (LevelProgressText)
	{
		LevelProgressText->SetText(FText::FromString(FString::Printf(
			TEXT("等级 %d  ·  经验 %d / %d"), Arena->GetState().PlayerLevel,
			Arena->GetState().PlayerExperience, Arena->GetState().PlayerExperienceForNextLevel)));
	}
	if (WaveProgressText)
	{
		const FNaiwaP4ArenaState& State = Arena->GetState();
		if (State.TotalWaveCount > 0)
		{
			const FString WaveLabel = State.bBossSpawned && !State.bBossDefeated
				? FString::Printf(TEXT("波次 %d / %d  ·  奶桶队长登场"), State.TotalWaveCount, State.TotalWaveCount)
				: State.RunResult == ENaiwaP4RunResult::Victory
					? FString::Printf(TEXT("波次 %d / %d  ·  全部清场"), State.TotalWaveCount, State.TotalWaveCount)
					: FString::Printf(TEXT("波次 %d / %d  ·  击败 %d / %d"), State.CurrentWaveIndex,
						State.TotalWaveCount, State.DefeatedWaveEnemyCount, State.TotalWaveEnemyCount);
			WaveProgressText->SetText(FText::FromString(WaveLabel));
		}
		else
		{
			WaveProgressText->SetText(FText::FromString(TEXT("训练预览")));
		}
	}
	UpdateBossAndResultUI(Arena);
	UpdateAttackWarnings(Arena);
	UpdateEnemyHealthBars(Arena);
}

void UNaiwaP4ArtIntegrationWidget::BuildBattleHud()
{
	UBorder* HudPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("P4BattleHudPanel"));
	HudPanel->SetBrush(FSlateRoundedBoxBrush(PaperColor, 12.0f));
	HudPanel->SetPadding(FMargin(18.0f, 14.0f));
	UCanvasPanelSlot* HudSlot = RootCanvas->AddChildToCanvas(HudPanel);
	HudSlot->SetAnchors(FAnchors(0.025f, 0.035f));
	HudSlot->SetAlignment(FVector2D::ZeroVector);
	HudSlot->SetPosition(FVector2D::ZeroVector);
	HudSlot->SetSize(FVector2D(400.0f, 244.0f));

	UVerticalBox* HudStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("P4BattleHudStack"));
	HudPanel->SetContent(HudStack);
	HudStack->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("P4 ARENA  ·  美术接入预览"), 18, InkColor, true));
	HudStack->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("奶蛙生命值"), 14, MutedColor));

	UHorizontalBox* HealthRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("P4HealthRow"));
	HudStack->AddChildToVerticalBox(HealthRow);
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("P4HealthBar"));
	HealthBar->SetPercent(1.0f);
	HealthBar->SetFillColorAndOpacity(FLinearColor(0.82f, 0.38f, 0.25f, 1.0f));
	if (UHorizontalBoxSlot* BarSlot = HealthRow->AddChildToHorizontalBox(HealthBar))
	{
		BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BarSlot->SetPadding(FMargin(0.0f, 7.0f, 12.0f, 4.0f));
	}
	HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("P4HealthText"));
	HealthText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16));
	HealthText->SetColorAndOpacity(InkColor);
	HealthRow->AddChildToHorizontalBox(HealthText);

	EnergyAndChargeText = MakeText(WidgetTree, TEXT("能量 0 / 100  ·  冲撞 2 / 2"), 13, MutedColor);
	HudStack->AddChildToVerticalBox(EnergyAndChargeText);
	LevelProgressText = MakeText(WidgetTree, TEXT("等级 1  ·  经验 0 / 4"), 14, InkColor, true);
	HudStack->AddChildToVerticalBox(LevelProgressText);
	WaveProgressText = MakeText(WidgetTree, TEXT("波次加载中"), 14, InkColor, true);
	HudStack->AddChildToVerticalBox(WaveProgressText);
	HudStack->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("奶蛋自动索敌发射  ·  按住 J 近战出击"), 15, AttackHintColor, true));
	HudStack->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("Shift 冲撞  ·  Space 防御  ·  E 大笑"), 13, MutedColor));
	HudStack->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("Tab 选择强化  ·  T 切换预警样式"), 13, MutedColor));
}

void UNaiwaP4ArtIntegrationWidget::BuildTelegraphPreview()
{
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("P4TelegraphStylePreview"));
	Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.10f, 0.09f, 0.08f, 0.92f), 12.0f));
	Panel->SetPadding(FMargin(18.0f, 14.0f));
	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.975f, 0.035f));
	PanelSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	PanelSlot->SetPosition(FVector2D::ZeroVector);
	PanelSlot->SetSize(FVector2D(390.0f, 92.0f));

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("P4TelegraphStylePreviewStack"));
	Panel->SetContent(Stack);
	Stack->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("预警样式预览 · T 切换"), 14, PaperColor, true));
	TelegraphPreviewText = MakeText(WidgetTree, TEXT("攻击范围：红色圆环"), 18,
		FLinearColor(0.95f, 0.27f, 0.21f, 1.0f), true);
	TelegraphPreviewText->SetAutoWrapText(false);
	Stack->AddChildToVerticalBox(TelegraphPreviewText);
}

void UNaiwaP4ArtIntegrationWidget::BuildBossHud()
{
	BossHudPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("P4BossHudPanel"));
	BossHudPanel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.12f, 0.10f, 0.09f, 0.91f), 12.0f));
	BossHudPanel->SetPadding(FMargin(18.0f, 10.0f));
	BossHudPanel->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* BossSlot = RootCanvas->AddChildToCanvas(BossHudPanel);
	BossSlot->SetAnchors(FAnchors(0.5f, 0.035f));
	BossSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	BossSlot->SetPosition(FVector2D(0.0f, 0.0f));
	BossSlot->SetSize(FVector2D(520.0f, 102.0f));

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("P4BossHudStack"));
	BossHudPanel->SetContent(Stack);
	BossStatusText = MakeText(WidgetTree, TEXT("奶桶队长"), 17, PaperColor, true);
	BossStatusText->SetAutoWrapText(false);
	Stack->AddChildToVerticalBox(BossStatusText);

	BossHealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("P4BossHealthBar"));
	BossHealthBar->SetPercent(1.0f);
	BossHealthBar->SetFillColorAndOpacity(FLinearColor(0.75f, 0.20f, 0.16f, 1.0f));
	if (UVerticalBoxSlot* BarSlot = Stack->AddChildToVerticalBox(BossHealthBar))
	{
		BarSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 3.0f));
	}
	BossHealthText = MakeText(WidgetTree, TEXT("480 / 480"), 13, PaperColor);
	BossHealthText->SetAutoWrapText(false);
	Stack->AddChildToVerticalBox(BossHealthText);
}

void UNaiwaP4ArtIntegrationWidget::BuildRunResultPanel()
{
	RunResultPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("P4RunResultPanel"));
	RunResultPanel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.985f, 0.955f, 0.885f, 0.98f), 22.0f));
	RunResultPanel->SetPadding(FMargin(36.0f, 32.0f));
	RunResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* ResultSlot = RootCanvas->AddChildToCanvas(RunResultPanel);
	ResultSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	ResultSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ResultSlot->SetPosition(FVector2D::ZeroVector);
	ResultSlot->SetSize(FVector2D(560.0f, 260.0f));

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("P4RunResultStack"));
	RunResultPanel->SetContent(Stack);
	RunResultTitle = MakeText(WidgetTree, TEXT("本局完成"), 30, InkColor, true);
	RunResultTitle->SetJustification(ETextJustify::Center);
	RunResultTitle->SetAutoWrapText(false);
	Stack->AddChildToVerticalBox(RunResultTitle);
	RunResultHint = MakeText(WidgetTree, TEXT("按 R 或点击按钮，以相同 seed 重开并保留本局强化"), 16, MutedColor);
	RunResultHint->SetJustification(ETextJustify::Center);
	Stack->AddChildToVerticalBox(RunResultHint);

	UButton* RetryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("P4RunRetryButton"));
	StyleButton(RetryButton);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RetryButton->IsFocusable = false;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	RetryButton->OnClicked.AddDynamic(this, &ThisClass::OnRetryRunClicked);
	UBorder* RetryFace = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("P4RunRetryButtonFace"));
	RetryFace->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.35f, 0.48f, 0.34f, 1.0f), 10.0f));
	RetryFace->SetPadding(FMargin(22.0f, 12.0f));
	UTextBlock* RetryText = MakeText(WidgetTree, TEXT("重试本章  ·  R"), 17,
		FLinearColor(1.0f, 0.973f, 0.914f, 1.0f), true);
	RetryText->SetJustification(ETextJustify::Center);
	RetryText->SetAutoWrapText(false);
	RetryFace->SetContent(RetryText);
	RetryButton->AddChild(RetryFace);
	if (UVerticalBoxSlot* ButtonSlot = Stack->AddChildToVerticalBox(RetryButton))
	{
		ButtonSlot->SetPadding(FMargin(0.0f, 20.0f, 0.0f, 0.0f));
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

void UNaiwaP4ArtIntegrationWidget::UpdateBossAndResultUI(UNaiwaP4ArenaSubsystem* Arena)
{
	if (!Arena)
	{
		return;
	}
	const FNaiwaP4ArenaState& State = Arena->GetState();
	const FNaiwaP4EntityState* Boss = State.Entities.FindByPredicate([](const FNaiwaP4EntityState& Entity)
	{
		return Entity.bWaveBoss;
	});
	if (BossHudPanel && BossHealthBar && BossStatusText && BossHealthText)
	{
		if (Boss && Boss->bAlive)
		{
			BossHudPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
			const int32 MaxHP = FMath::Max(Boss->MaxHP, 1);
			BossHealthBar->SetPercent(FMath::Clamp(static_cast<float>(Boss->HP) / MaxHP, 0.0f, 1.0f));
			FString BossStatus = FString::Printf(TEXT("奶桶队长  ·  阶段 %d"), Boss->BossPhase);
			if (Boss->EnemyTelegraphTicksRemaining > 0)
			{
				BossStatus = Boss->BossAttack == ENaiwaP4BossAttack::Charge
					? TEXT("奶桶冲锋预警！向侧面躲")
					: TEXT("桶盖砸地预警！远离队长");
			}
			else if (Boss->BossPhaseTransitionTicksRemaining > 0)
			{
				BossStatus = TEXT("奶桶队长进入强化阶段！");
			}
			BossStatusText->SetText(FText::FromString(BossStatus));
			BossHealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Boss->HP, MaxHP)));
		}
		else
		{
			BossHudPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (!RunResultPanel || State.RunResult == ENaiwaP4RunResult::InProgress
		|| RunResultPanel->GetVisibility() == ESlateVisibility::Visible)
	{
		return;
	}
	if (UpgradeOfferPanel && UpgradeOfferPanel->GetVisibility() == ESlateVisibility::Visible)
	{
		UpgradeOfferPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	RunResultTitle->SetText(FText::FromString(State.RunResult == ENaiwaP4RunResult::Victory
		? TEXT("胜利！奶桶队长被击败") : TEXT("本章挑战失败")));
	RunResultPanel->SetVisibility(ESlateVisibility::Visible);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
	}
}

void UNaiwaP4ArtIntegrationWidget::CycleTelegraphPreview()
{
	if (!TelegraphPreviewText)
	{
		return;
	}

	static const TCHAR* Labels[] = {
		TEXT("◎  攻击范围 · 红色圆环"),
		TEXT("➜  位移预警 · 黄色方向箭"),
		TEXT("✦  不可挡 · 白色星芒"),
	};
	static const FLinearColor Colors[] = {
		FLinearColor(0.95f, 0.27f, 0.21f, 1.0f),
		FLinearColor(0.98f, 0.78f, 0.25f, 1.0f),
		FLinearColor(0.96f, 0.94f, 0.86f, 1.0f),
	};

	TelegraphPreviewIndex = (TelegraphPreviewIndex + 1) % UE_ARRAY_COUNT(Labels);
	TelegraphPreviewText->SetText(FText::FromString(Labels[TelegraphPreviewIndex]));
	TelegraphPreviewText->SetColorAndOpacity(Colors[TelegraphPreviewIndex]);
}

void UNaiwaP4ArtIntegrationWidget::UpdateAttackWarnings(UNaiwaP4ArenaSubsystem* Arena)
{
	APlayerController* PlayerController = GetOwningPlayer();
	const UNaiwaP4Settings* Settings = GetDefault<UNaiwaP4Settings>();
	if (!Arena || !PlayerController || !Settings || !RootCanvas)
	{
		return;
	}

	for (const TPair<int32, TObjectPtr<UTextBlock>>& Entry : AttackWarningMarkers)
	{
		if (Entry.Value)
		{
			Entry.Value->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	for (const FNaiwaP4EntityState& Enemy : Arena->GetState().Entities)
	{
		if ((!Enemy.bTrainingEnemy && !Enemy.bWaveEnemy && !Enemy.bWaveBoss)
			|| !Enemy.bAlive || Enemy.EnemyTelegraphTicksRemaining <= 0)
		{
			continue;
		}

		UTextBlock* Marker = AttackWarningMarkers.FindRef(Enemy.EntityId);
		UCanvasPanelSlot* MarkerSlot = AttackWarningMarkerSlots.FindRef(Enemy.EntityId);
		if (!Marker || !MarkerSlot)
		{
			Marker = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
				*FString::Printf(TEXT("P4EnemyAttackWarning_%d"), Enemy.EntityId));
			if (!Marker)
			{
				continue;
			}
			Marker->SetText(FText::FromString(TEXT("!")));
			Marker->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 52));
			Marker->SetColorAndOpacity(EnemyWarningColor);
			Marker->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
			Marker->SetShadowOffset(FVector2D(2.0f, 3.0f));
			Marker->SetJustification(ETextJustify::Center);
			Marker->SetAutoWrapText(false);
			Marker->SetVisibility(ESlateVisibility::Collapsed);
			MarkerSlot = RootCanvas->AddChildToCanvas(Marker);
			MarkerSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			MarkerSlot->SetSize(FVector2D(56.0f, 64.0f));
			AttackWarningMarkers.Add(Enemy.EntityId, Marker);
			AttackWarningMarkerSlots.Add(Enemy.EntityId, MarkerSlot);
		}

		const FVector2D RenderPosition = Arena->GetSmoothedPresentationPosition(Enemy.EntityId, Enemy.Position);
		const float WorldX = Settings->LogicalArenaWidth - static_cast<float>(RenderPosition.X);
		// Static W3 art extends well above its collision radius. Place the warning
		// over the visible head; keep the collider-based estimate for graybox actors.
		float VisibleEnemyHeightUU = Enemy.CollisionRadius * CharacterPresentationScale;
		if (Enemy.bWaveEnemy && Enemy.EnemyTypeId == TEXT("naiding"))
		{
		VisibleEnemyHeightUU = StaticEnemyFrameHeightPx / StaticEnemyFrameSizePx
				* NaiDingPreviewCanvasHeightUU * CharacterPresentationScale;
		}
		else if (Enemy.bWaveBoss)
		{
			VisibleEnemyHeightUU = StaticEnemyFrameHeightPx / StaticEnemyFrameSizePx
				* 414.0f * CharacterPresentationScale;
		}
		else if (Enemy.bWaveEnemy && Enemy.EnemyTypeId == TEXT("naidun"))
		{
			VisibleEnemyHeightUU = StaticEnemyFrameHeightPx / StaticEnemyFrameSizePx
				* NaiDunPreviewCanvasHeightUU * CharacterPresentationScale;
		}
		const float EnemyTopZ = Enemy.CollisionRadius + VisibleEnemyHeightUU;
		const FVector MarkerWorldPosition(WorldX, RenderPosition.Y,
			EnemyTopZ + EnemyWarningHeadroomUU * CharacterPresentationScale);
		FVector2D MarkerPosition;
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			PlayerController, MarkerWorldPosition, MarkerPosition, true))
		{
			MarkerSlot->SetPosition(MarkerPosition);
			Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void UNaiwaP4ArtIntegrationWidget::UpdateEnemyHealthBars(UNaiwaP4ArenaSubsystem* Arena)
{
	APlayerController* PlayerController = GetOwningPlayer();
	const UNaiwaP4Settings* Settings = GetDefault<UNaiwaP4Settings>();
	if (!Arena || !PlayerController || !Settings || !RootCanvas)
	{
		return;
	}

	for (const TPair<int32, TObjectPtr<UProgressBar>>& Entry : EnemyHealthBars)
	{
		if (Entry.Value)
		{
			Entry.Value->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	for (const FNaiwaP4EntityState& Enemy : Arena->GetState().Entities)
	{
		if ((!Enemy.bTrainingEnemy && !Enemy.bWaveEnemy && !Enemy.bWaveBoss) || !Enemy.bAlive)
		{
			continue;
		}

		UProgressBar* EnemyBar = EnemyHealthBars.FindRef(Enemy.EntityId);
		UCanvasPanelSlot* HealthBarSlot = EnemyHealthBarSlots.FindRef(Enemy.EntityId);
		if (!EnemyBar || !HealthBarSlot)
		{
			EnemyBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),
				*FString::Printf(TEXT("P4EnemyHealthBar_%d"), Enemy.EntityId));
			if (!EnemyBar)
			{
				continue;
			}

			FProgressBarStyle BarStyle = EnemyBar->GetWidgetStyle();
			BarStyle.BackgroundImage = FSlateRoundedBoxBrush(FLinearColor(0.07f, 0.045f, 0.035f, 0.92f), 3.0f);
			BarStyle.FillImage = FSlateRoundedBoxBrush(FLinearColor(0.84f, 0.14f, 0.10f, 1.0f), 3.0f);
			EnemyBar->SetWidgetStyle(BarStyle);
			EnemyBar->SetFillColorAndOpacity(FLinearColor(0.92f, 0.18f, 0.11f, 1.0f));
			EnemyBar->SetVisibility(ESlateVisibility::Collapsed);
			HealthBarSlot = RootCanvas->AddChildToCanvas(EnemyBar);
			HealthBarSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			HealthBarSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			HealthBarSlot->SetSize(FVector2D(88.0f, 10.0f));
			EnemyHealthBars.Add(Enemy.EntityId, EnemyBar);
			EnemyHealthBarSlots.Add(Enemy.EntityId, HealthBarSlot);
		}

		const float MaxHP = static_cast<float>(FMath::Max(Enemy.MaxHP, 1));
		EnemyBar->SetPercent(FMath::Clamp(static_cast<float>(Enemy.HP) / MaxHP, 0.0f, 1.0f));

		const FVector2D RenderPosition = Arena->GetSmoothedPresentationPosition(Enemy.EntityId, Enemy.Position);
		const float WorldX = Settings->LogicalArenaWidth - static_cast<float>(RenderPosition.X);
		float VisibleEnemyHeightUU = Enemy.CollisionRadius * CharacterPresentationScale;
		if (Enemy.bWaveEnemy && Enemy.EnemyTypeId == TEXT("naiding"))
		{
			VisibleEnemyHeightUU = StaticEnemyFrameHeightPx / StaticEnemyFrameSizePx
				* NaiDingPreviewCanvasHeightUU * CharacterPresentationScale;
		}
		else if (Enemy.bWaveBoss)
		{
			VisibleEnemyHeightUU = StaticEnemyFrameHeightPx / StaticEnemyFrameSizePx
				* 414.0f * CharacterPresentationScale;
		}
		else if (Enemy.bWaveEnemy && Enemy.EnemyTypeId == TEXT("naidun"))
		{
			VisibleEnemyHeightUU = StaticEnemyFrameHeightPx / StaticEnemyFrameSizePx
				* NaiDunPreviewCanvasHeightUU * CharacterPresentationScale;
		}
		const float EnemyTopZ = Enemy.CollisionRadius + VisibleEnemyHeightUU;
		const FVector BarWorldPosition(WorldX, RenderPosition.Y,
			EnemyTopZ + 38.0f * CharacterPresentationScale);
		FVector2D BarPosition;
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			PlayerController, BarWorldPosition, BarPosition, true))
		{
			HealthBarSlot->SetPosition(BarPosition);
			EnemyBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void UNaiwaP4ArtIntegrationWidget::BuildUpgradeOffer()
{
	UpgradeOfferPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("P4UpgradeOfferOverlay"));
	UpgradeOfferPanel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.985f, 0.955f, 0.885f, 0.99f), 22.0f));
	UpgradeOfferPanel->SetPadding(FMargin(26.0f, 22.0f, 26.0f, 20.0f));
	UpgradeOfferPanel->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* OverlaySlot = RootCanvas->AddChildToCanvas(UpgradeOfferPanel);
	int32 ViewportWidth = 1280;
	int32 ViewportHeight = 720;
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	}
	const float PanelWidth = FMath::Clamp(static_cast<float>(ViewportWidth) * 0.84f, 1000.0f, 1560.0f);
	const float PanelHeight = FMath::Clamp(static_cast<float>(ViewportHeight) * 0.82f, 620.0f, 820.0f);
	UpgradeCardWidth = (PanelWidth - 52.0f - 54.0f) / 3.0f;
	UpgradeCardHeight = FMath::Clamp(PanelHeight - 304.0f, 390.0f, 516.0f);
	OverlaySlot->SetAnchors(FAnchors(0.5f, 0.0f));
	OverlaySlot->SetAlignment(FVector2D(0.5f, 0.0f));
	OverlaySlot->SetPosition(FVector2D(0.0f, 22.0f));
	OverlaySlot->SetSize(FVector2D(PanelWidth, PanelHeight));

	UVerticalBox* OfferStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("P4UpgradeOfferStack"));
	UpgradeOfferPanel->SetContent(OfferStack);
	OfferStack->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("等级强化 · 选择一张"), 27, InkColor, true));
	OfferStack->AddChildToVerticalBox(MakeText(WidgetTree,
		TEXT("击杀敌人获得经验 · 每级随机抽三张 · 可重抽一次 · 重复卡会进阶"),
		14, MutedColor));

	UpgradeCardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("P4UpgradeCardRow"));
	if (UVerticalBoxSlot* CardRowSlot = OfferStack->AddChildToVerticalBox(UpgradeCardRow))
	{
		CardRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		CardRowSlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 12.0f));
	}

	TArray<FName> OrderedCards;
	if (UWorld* World = GetWorld())
	{
		if (UNaiwaP4ArenaSubsystem* Arena = World->GetSubsystem<UNaiwaP4ArenaSubsystem>())
		{
			OrderedCards = Arena->GetUpgradeCardIds();
		}
	}
	if (OrderedCards.IsEmpty())
	{
		OrderedCards = { FName(TEXT("bounce_ball")), FName(TEXT("rounder_belly")), FName(TEXT("cant_hold")) };
	}
	for (int32 Index = 0; Index < FMath::Min(3, OrderedCards.Num()); ++Index)
	{
		AddCard(OrderedCards[Index], Index);
	}
	RefreshUpgradeCardStates();

	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("P4UpgradeOfferFooter"));
	OfferStack->AddChildToVerticalBox(Footer);
	UpgradeRerollButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("P4UpgradeRerollButton"));
	StyleButton(UpgradeRerollButton);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UpgradeRerollButton->IsFocusable = false;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	UpgradeRerollButton->OnClicked.AddDynamic(this, &ThisClass::OnRerollUpgradeClicked);
	UBorder* RerollFace = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("P4UpgradeRerollFace"));
	RerollFace->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.35f, 0.48f, 0.34f, 1.0f), 10.0f));
	RerollFace->SetPadding(FMargin(15.0f, 10.0f));
	UpgradeRerollText = MakeText(WidgetTree, TEXT("重抽（升级后可用）"), 14,
		FLinearColor(1.0f, 0.973f, 0.914f, 1.0f), true);
	UpgradeRerollText->SetAutoWrapText(false);
	RerollFace->SetContent(UpgradeRerollText);
	UpgradeRerollButton->AddChild(RerollFace);
	Footer->AddChildToHorizontalBox(UpgradeRerollButton);
	UButton* ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("P4ConfirmPreviewButton"));
	StyleButton(ConfirmButton);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	ConfirmButton->IsFocusable = false;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	ConfirmButton->OnClicked.AddDynamic(this, &ThisClass::OnConfirmPreviewClicked);
	UBorder* ConfirmFace = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("P4ConfirmPreviewFace"));
	ConfirmFace->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.36f, 0.28f, 0.19f, 1.0f), 10.0f));
	ConfirmFace->SetPadding(FMargin(16.0f, 10.0f));
	UTextBlock* ConfirmLabel = MakeText(WidgetTree, TEXT("确认所选强化"), 15, FLinearColor(1.0f, 0.953f, 0.843f, 1.0f), true);
	ConfirmLabel->SetAutoWrapText(false);
	ConfirmFace->SetContent(ConfirmLabel);
	ConfirmButton->AddChild(ConfirmFace);
	Footer->AddChildToHorizontalBox(ConfirmButton);
	ConfirmPreviewText = MakeText(WidgetTree, TEXT("升级时选卡领取；重抽仅限本级一次"), 13, MutedColor);
	if (UHorizontalBoxSlot* StatusSlot = Footer->AddChildToHorizontalBox(ConfirmPreviewText))
	{
		StatusSlot->SetPadding(FMargin(18.0f, 9.0f));
	}
}

void UNaiwaP4ArtIntegrationWidget::AddCard(const FName CardId, const int32 CardIndex)
{
	UWorld* World = GetWorld();
	UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	const FNaiwaP4UpgradeRow* Row = Arena ? Arena->FindUpgradeRow(CardId) : nullptr;
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("P4 art preview card row '%s' is unavailable."), *CardId.ToString());
		return;
	}

	UpgradeCardIds.Add(CardId);
	const FLinearColor Accent = CategoryColor(Row->CategoryId);
	const float FaceWidth = UpgradeCardWidth - 4.0f;
	const float FaceHeight = UpgradeCardHeight - 4.0f;
	const bool bCompact = FaceHeight < 440.0f;
	UButton* CardButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("P4UpgradeCard_%s"), *CardId.ToString()));
	StyleButton(CardButton);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	CardButton->IsFocusable = false;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	if (CardIndex == 0) CardButton->OnClicked.AddDynamic(this, &ThisClass::OnCard0Clicked);
	else if (CardIndex == 1) CardButton->OnClicked.AddDynamic(this, &ThisClass::OnCard1Clicked);
	else CardButton->OnClicked.AddDynamic(this, &ThisClass::OnCard2Clicked);
	if (UHorizontalBoxSlot* CardSlot = UpgradeCardRow ? UpgradeCardRow->AddChildToHorizontalBox(CardButton) : nullptr)
	{
		CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CardSlot->SetPadding(FMargin(9.0f, 0.0f));
	}

	UBorder* SelectionFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	SelectionFrame->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.79f, 0.73f, 0.62f, 1.0f), 19.0f));
	SelectionFrame->SetPadding(FMargin(2.0f));
	UBorder* Surface = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Surface->SetBrush(FSlateRoundedBoxBrush(FLinearColor(1.0f, 0.992f, 0.967f, 1.0f), 17.0f));
	Surface->SetPadding(FMargin(0.0f));
	SelectionFrame->SetContent(Surface);
	USizeBox* CardFrame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CardFrame->SetWidthOverride(UpgradeCardWidth);
	CardFrame->SetHeightOverride(UpgradeCardHeight);
	CardFrame->SetContent(SelectionFrame);
	CardButton->AddChild(CardFrame);
	UpgradeButtons.Add(CardButton);
	UpgradeCardSurfaces.Add(SelectionFrame);
	UpgradeCardFaces.Add(Surface);

	UCanvasPanel* CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Surface->SetContent(CardCanvas);
	auto Place = [CardCanvas](UWidget* Widget, const FVector2D& Position, const FVector2D& Size) -> UCanvasPanelSlot*
	{
		UCanvasPanelSlot* Slot = CardCanvas->AddChildToCanvas(Widget);
		Slot->SetAnchors(FAnchors(0.0f, 0.0f));
		Slot->SetAlignment(FVector2D::ZeroVector);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		return Slot;
	};

	UBorder* CategoryStripe = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	CategoryStripe->SetBrush(FSlateRoundedBoxBrush(Accent, 4.0f));
	Place(CategoryStripe, FVector2D(0.0f, 0.0f), FVector2D(FaceWidth, 8.0f));

	UTexture2D* CategoryMark = Row->CategoryMarkSoftRef.LoadSynchronous();
	if (!CategoryMark)
	{
		CategoryMark = LoadObject<UTexture2D>(nullptr, *CategoryTexturePath(Row->CategoryId));
	}
	if (CategoryMark)
	{
		UImage* MarkImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		MarkImage->SetBrushFromTexture(CategoryMark, true);
		Place(MarkImage, FVector2D(18.0f, 17.0f), FVector2D(20.0f, 20.0f));
	}
	UTextBlock* CategoryText = MakeText(WidgetTree, Row->CategoryLabel.ToString(), 14, Accent, true);
	CategoryText->SetAutoWrapText(false);
	Place(CategoryText, FVector2D(44.0f, 16.0f), FVector2D(FaceWidth - 150.0f, 24.0f));

	UBorder* AlphaBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	AlphaBadge->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.286f, 0.227f, 0.188f, 1.0f), 10.0f));
	Place(AlphaBadge, FVector2D(FaceWidth - 88.0f, 14.0f), FVector2D(70.0f, 25.0f));
	UTextBlock* AlphaText = MakeText(WidgetTree, TEXT("ALPHA"), 10, FLinearColor(1.0f, 0.953f, 0.843f, 1.0f), true);
	AlphaText->SetAutoWrapText(false);
	AlphaText->SetJustification(ETextJustify::Center);
	AlphaBadge->SetContent(AlphaText);

	const float ArtTop = 50.0f;
	const float ArtHeight = FMath::Clamp(FaceHeight * 0.30f, 96.0f, 154.0f);
	UBorder* ArtPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ArtPanel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.953f, 0.914f, 0.829f, 1.0f), 15.0f));
	Place(ArtPanel, FVector2D(17.0f, ArtTop), FVector2D(FaceWidth - 34.0f, ArtHeight));

	UTexture2D* Icon = Row->IconSoftRef.LoadSynchronous();
	if (!Icon)
	{
		Icon = LoadObject<UTexture2D>(nullptr, *FString::Printf(
			TEXT("/Game/Naiwa/P4/UI/Alpha/RuntimeReady/T_P4_A3_CardIcon_%s.T_P4_A3_CardIcon_%s"),
			*CardId.ToString(), *CardId.ToString()));
	}
	if (!Icon)
	{
		Icon = CategoryMark;
	}
	if (Icon)
	{
		UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		IconImage->SetBrushFromTexture(Icon, true);
		const float IconSize = FMath::Min(bCompact ? 104.0f : 132.0f, ArtHeight - 14.0f);
		Place(IconImage, FVector2D((FaceWidth - IconSize) * 0.5f, ArtTop + (ArtHeight - IconSize) * 0.5f),
			FVector2D(IconSize, IconSize));
	}

	const float TitleTop = ArtTop + ArtHeight + 8.0f;
	const float TitleHeight = bCompact ? 26.0f : 31.0f;
	UTextBlock* TitleText = MakeText(WidgetTree, Row->Title.ToString(), bCompact ? 19 : 23, InkColor, true);
	TitleText->SetAutoWrapText(false);
	Place(TitleText, FVector2D(19.0f, TitleTop), FVector2D(FaceWidth - 38.0f, TitleHeight));

	const float DescriptionTop = TitleTop + TitleHeight + 1.0f;
	const float ActionHeight = bCompact ? 38.0f : 44.0f;
	const float ActionTop = FaceHeight - ActionHeight - 15.0f;
	const float DescriptionHeight = bCompact ? 46.0f : 66.0f;
	UTextBlock* DescriptionText = MakeText(WidgetTree, Row->Description.ToString(), bCompact ? 12 : 14, InkColor);
	Place(DescriptionText, FVector2D(19.0f, DescriptionTop), FVector2D(FaceWidth - 38.0f, DescriptionHeight));
	UpgradeCardDescriptions.Add(DescriptionText);

	const float RuleTop = DescriptionTop + DescriptionHeight + 8.0f;
	const float RuleHeight = FMath::Min(bCompact ? 56.0f : 68.0f, ActionTop - RuleTop - 10.0f);
	UBorder* RulePanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	RulePanel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.936f, 0.894f, 0.814f, 1.0f), 9.0f));
	Place(RulePanel, FVector2D(18.0f, RuleTop), FVector2D(FaceWidth - 36.0f, RuleHeight));
	UBorder* RuleAccent = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	RuleAccent->SetBrush(FSlateRoundedBoxBrush(Accent, 3.0f));
	Place(RuleAccent, FVector2D(18.0f, RuleTop), FVector2D(4.0f, RuleHeight));
	const FText RuleBody = !Row->ComboRule.IsEmpty() ? Row->ComboRule : Row->TriggerRule;
	const FString RuleLabel = !Row->ComboRule.IsEmpty() ? TEXT("组合提示") : TEXT("触发限制");
	UTextBlock* RuleLabelText = MakeText(WidgetTree, RuleLabel, 10, FLinearColor(0.55f, 0.42f, 0.22f, 1.0f), true);
	RuleLabelText->SetAutoWrapText(false);
	Place(RuleLabelText, FVector2D(30.0f, RuleTop + 4.0f), FVector2D(FaceWidth - 64.0f, 14.0f));
	UTextBlock* RuleBodyText = MakeText(WidgetTree, RuleBody.ToString(), bCompact ? 10 : 11, MutedColor);
	Place(RuleBodyText, FVector2D(30.0f, RuleTop + 18.0f), FVector2D(FaceWidth - 64.0f, FMath::Max(18.0f, RuleHeight - 21.0f)));

	UBorder* ChoiceAction = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ChoiceAction->SetBrush(FSlateRoundedBoxBrush(FLinearColor(1.0f, 0.973f, 0.914f, 1.0f), 11.0f));
	Place(ChoiceAction, FVector2D(18.0f, ActionTop), FVector2D(FaceWidth - 36.0f, ActionHeight));
	UpgradeCardChoiceActions.Add(ChoiceAction);
	UTextBlock* ChoiceLabel = MakeText(WidgetTree, TEXT("选择此卡"), bCompact ? 12 : 13, InkColor, true);
	ChoiceLabel->SetAutoWrapText(false);
	Place(ChoiceLabel, FVector2D(31.0f, ActionTop + (ActionHeight - 22.0f) * 0.5f), FVector2D(FaceWidth - 115.0f, 22.0f));
	UTextBlock* ChoiceState = MakeText(WidgetTree, TEXT("未选"), bCompact ? 11 : 12, MutedColor, true);
	ChoiceState->SetAutoWrapText(false);
	ChoiceState->SetJustification(ETextJustify::Right);
	Place(ChoiceState, FVector2D(FaceWidth - 91.0f, ActionTop + (ActionHeight - 22.0f) * 0.5f), FVector2D(69.0f, 22.0f));
	UpgradeCardChoiceLabels.Add(ChoiceLabel);
	UpgradeCardChoiceStates.Add(ChoiceState);
}

void UNaiwaP4ArtIntegrationWidget::RebuildUpgradeCardChoices(const TArray<FName>& CardIds)
{
	if (!UpgradeCardRow)
	{
		return;
	}
	UpgradeCardRow->ClearChildren();
	UpgradeButtons.Reset();
	UpgradeCardSurfaces.Reset();
	UpgradeCardFaces.Reset();
	UpgradeCardDescriptions.Reset();
	UpgradeCardChoiceActions.Reset();
	UpgradeCardChoiceLabels.Reset();
	UpgradeCardChoiceStates.Reset();
	UpgradeCardIds.Reset();
	SelectedCardIndex = INDEX_NONE;
	for (int32 Index = 0; Index < FMath::Min(3, CardIds.Num()); ++Index)
	{
		AddCard(CardIds[Index], Index);
	}
	RefreshUpgradeCardStates();
	if (!UpgradeCardIds.IsEmpty())
	{
		SelectCard(0);
	}
	RefreshUpgradeRerollState();
}

void UNaiwaP4ArtIntegrationWidget::SelectCard(const int32 CardIndex)
{
	if (!UpgradeCardSurfaces.IsValidIndex(CardIndex) || !UpgradeCardIds.IsValidIndex(CardIndex))
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UNaiwaP4ArenaSubsystem* Arena = World->GetSubsystem<UNaiwaP4ArenaSubsystem>())
		{
			if (!Arena->GetAvailableUpgradeCardIds().Contains(UpgradeCardIds[CardIndex]))
			{
				return;
			}
		}
	}
	SelectedCardIndex = CardIndex;
	for (int32 Index = 0; Index < UpgradeCardSurfaces.Num(); ++Index)
	{
		const bool bSelected = Index == SelectedCardIndex;
		UpgradeCardSurfaces[Index]->SetBrush(FSlateRoundedBoxBrush(
			bSelected ? FLinearColor(0.78f, 0.58f, 0.20f, 1.0f) : FLinearColor(0.79f, 0.73f, 0.62f, 1.0f), 19.0f));
		if (UpgradeCardFaces.IsValidIndex(Index))
		{
			UpgradeCardFaces[Index]->SetBrush(FSlateRoundedBoxBrush(
				bSelected ? FLinearColor(1.0f, 0.985f, 0.92f, 1.0f) : FLinearColor(1.0f, 0.992f, 0.967f, 1.0f), 17.0f));
		}
		if (UpgradeCardChoiceLabels.IsValidIndex(Index))
		{
			UpgradeCardChoiceLabels[Index]->SetText(FText::FromString(bSelected ? TEXT("当前选择") : TEXT("选择此卡")));
		}
		if (UpgradeCardChoiceStates.IsValidIndex(Index))
		{
			UpgradeCardChoiceStates[Index]->SetText(FText::FromString(bSelected ? TEXT("✓ 已选") : TEXT("未选")));
			UpgradeCardChoiceStates[Index]->SetColorAndOpacity(bSelected ? InkColor : MutedColor);
		}
		if (UpgradeCardChoiceActions.IsValidIndex(Index))
		{
			UpgradeCardChoiceActions[Index]->SetBrush(FSlateRoundedBoxBrush(
				bSelected ? FLinearColor(0.93f, 0.76f, 0.40f, 1.0f) : FLinearColor(1.0f, 0.973f, 0.914f, 1.0f), 11.0f));
		}
	}
	if (ConfirmPreviewText)
	{
		const UNaiwaP4ArenaSubsystem* Arena = GetWorld()
			? GetWorld()->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
		ConfirmPreviewText->SetText(FText::FromString(Arena && Arena->GetState().PendingUpgradeChoices > 0
			? FString::Printf(TEXT("已选：%s · 确认领取并继续战斗"), *UpgradeCardIds[CardIndex].ToString())
			: FString::Printf(TEXT("已选：%s · 升级后确认领取"), *UpgradeCardIds[CardIndex].ToString())));
	}
}

void UNaiwaP4ArtIntegrationWidget::RefreshUpgradeCardStates()
{
	const UNaiwaP4ArenaSubsystem* Arena = GetWorld()
		? GetWorld()->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	const TArray<FName> AvailableIds = Arena ? Arena->GetAvailableUpgradeCardIds() : UpgradeCardIds;
	const FNaiwaP4EntityState* Player = Arena
		? Arena->GetState().Entities.FindByPredicate([](const FNaiwaP4EntityState& Entity)
			{ return Entity.Kind == ENaiwaP4EntityKind::Player; })
		: nullptr;
	for (int32 Index = 0; Index < UpgradeCardIds.Num(); ++Index)
	{
		const bool bAvailable = AvailableIds.Contains(UpgradeCardIds[Index]);
		const int32 Rank = Arena ? Arena->GetUpgradeCardRank(UpgradeCardIds[Index]) : 0;
		const FNaiwaP4UpgradeRow* Row = Arena ? Arena->FindUpgradeRow(UpgradeCardIds[Index]) : nullptr;
		if (UpgradeCardDescriptions.IsValidIndex(Index) && UpgradeCardDescriptions[Index] && Row)
		{
			UpgradeCardDescriptions[Index]->SetText(FText::FromString(
				BuildUpgradeCardDescription(*Row, Rank, Player)));
		}
		if (UpgradeButtons.IsValidIndex(Index) && UpgradeButtons[Index])
		{
			UpgradeButtons[Index]->SetIsEnabled(bAvailable);
		}
		if (UpgradeCardChoiceLabels.IsValidIndex(Index) && UpgradeCardChoiceLabels[Index])
		{
			UpgradeCardChoiceLabels[Index]->SetText(FText::FromString(!bAvailable
				? TEXT("暂不可选") : (Index == SelectedCardIndex ? TEXT("当前选择") : TEXT("选择并进阶"))));
		}
		if (UpgradeCardChoiceStates.IsValidIndex(Index) && UpgradeCardChoiceStates[Index])
		{
			const bool bSelected = bAvailable && Index == SelectedCardIndex;
			const FString RankLabel = Rank > 0
				? FString::Printf(TEXT("%d 阶 → %d 阶"), Rank, Rank + 1)
				: TEXT("新强化");
			const FString StateLabel = !bAvailable
				? TEXT("前置未满足")
				: (bSelected ? FString::Printf(TEXT("✓ %s"), *RankLabel) : RankLabel);
			UpgradeCardChoiceStates[Index]->SetText(FText::FromString(StateLabel));
			UpgradeCardChoiceStates[Index]->SetColorAndOpacity(bSelected ? InkColor : MutedColor);
		}
	}
	if (SelectedCardIndex != INDEX_NONE
		&& (!AvailableIds.Contains(UpgradeCardIds.IsValidIndex(SelectedCardIndex)
			? UpgradeCardIds[SelectedCardIndex] : NAME_None)))
	{
		SelectedCardIndex = INDEX_NONE;
	}
	RefreshUpgradeRerollState();
}

void UNaiwaP4ArtIntegrationWidget::RefreshUpgradeRerollState()
{
	if (!UpgradeRerollButton || !UpgradeRerollText)
	{
		return;
	}
	const UNaiwaP4ArenaSubsystem* Arena = GetWorld()
		? GetWorld()->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	const int32 PendingChoices = Arena ? Arena->GetState().PendingUpgradeChoices : 0;
	const int32 Remaining = Arena ? Arena->GetState().UpgradeRerollsRemaining : 0;
	UpgradeRerollButton->SetIsEnabled(PendingChoices > 0 && Remaining > 0);
	UpgradeRerollText->SetText(FText::FromString(PendingChoices <= 0
		? TEXT("重抽（升级后可用）")
		: (Remaining > 0 ? TEXT("重抽本级卡牌（剩余 1 次）") : TEXT("本级重抽已用"))));
}

void UNaiwaP4ArtIntegrationWidget::OnCard0Clicked() { SelectCard(0); }
void UNaiwaP4ArtIntegrationWidget::OnCard1Clicked() { SelectCard(1); }
void UNaiwaP4ArtIntegrationWidget::OnCard2Clicked() { SelectCard(2); }

void UNaiwaP4ArtIntegrationWidget::OnConfirmPreviewClicked()
{
	if (!ConfirmPreviewText || !UpgradeCardIds.IsValidIndex(SelectedCardIndex))
	{
		return;
	}
	UWorld* World = GetWorld();
	UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	if (!Arena || Arena->GetState().PendingUpgradeChoices <= 0)
	{
		ConfirmPreviewText->SetText(FText::FromString(TEXT("击杀敌人获得经验；升级后随机抽三张卡，可重抽一次。")));
		return;
	}

	FString Error;
	if (!Arena->ConfirmUpgradeChoice(UpgradeCardIds[SelectedCardIndex], Error))
	{
		ConfirmPreviewText->SetText(FText::FromString(Error));
		RefreshUpgradeCardStates();
		return;
	}

	if (Arena->GetState().PendingUpgradeChoices > 0)
	{
		RebuildUpgradeCardChoices(Arena->GetState().CurrentUpgradeOfferCardIds);
		DisplayedUpgradeOfferRevision = Arena->GetState().UpgradeOfferRevision;
		return;
	}
	SelectedCardIndex = INDEX_NONE;
	DisplayedUpgradeOfferRevision = INDEX_NONE;
	ToggleUpgradeOffer();
}

void UNaiwaP4ArtIntegrationWidget::OnRerollUpgradeClicked()
{
	UWorld* World = GetWorld();
	UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	if (!Arena)
	{
		return;
	}
	FString Error;
	if (!Arena->RerollUpgradeChoice(Error))
	{
		if (ConfirmPreviewText)
		{
			ConfirmPreviewText->SetText(FText::FromString(Error));
		}
		RefreshUpgradeRerollState();
		return;
	}
	RebuildUpgradeCardChoices(Arena->GetState().CurrentUpgradeOfferCardIds);
	DisplayedUpgradeOfferRevision = Arena->GetState().UpgradeOfferRevision;
	if (ConfirmPreviewText)
	{
		ConfirmPreviewText->SetText(FText::FromString(TEXT("本级卡牌已重抽；本级不再有重抽机会。")));
	}
}

void UNaiwaP4ArtIntegrationWidget::ToggleUpgradeOffer()
{
	if (!UpgradeOfferPanel)
	{
		return;
	}
	const UNaiwaP4ArenaSubsystem* Arena = GetWorld()
		? GetWorld()->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	const bool bShow = UpgradeOfferPanel->GetVisibility() != ESlateVisibility::Visible;
	if (bShow && (!Arena || Arena->GetState().PendingUpgradeChoices <= 0))
	{
		return;
	}
	if (!bShow && Arena && Arena->GetState().PendingUpgradeChoices > 0)
	{
		return;
	}
	UpgradeOfferPanel->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bShow)
	{
		if (Arena && Arena->GetState().PendingUpgradeChoices > 0)
		{
			RebuildUpgradeCardChoices(Arena->GetState().CurrentUpgradeOfferCardIds);
			DisplayedUpgradeOfferRevision = Arena->GetState().UpgradeOfferRevision;
		}
		else
		{
			RefreshUpgradeCardStates();
		}
		UpgradeOfferSlideElapsed = 0.0f;
		bUpgradeOfferSlidingIn = true;
		UpgradeOfferPanel->SetRenderTranslation(FVector2D(0.0f, -56.0f));
		if (SelectedCardIndex == INDEX_NONE && !UpgradeCardIds.IsEmpty())
		{
			SelectCard(0);
		}
	}
	else
	{
		bUpgradeOfferSlidingIn = false;
		UpgradeOfferPanel->SetRenderTranslation(FVector2D::ZeroVector);
		if (!Arena || Arena->GetState().PendingUpgradeChoices <= 0)
		{
			DisplayedUpgradeOfferRevision = INDEX_NONE;
		}
	}
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (bShow)
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);
			PlayerController->SetInputMode(InputMode);
			PlayerController->bShowMouseCursor = true;
		}
		else
		{
			PlayerController->SetInputMode(FInputModeGameOnly());
			PlayerController->bShowMouseCursor = false;
		}
	}
}

void UNaiwaP4ArtIntegrationWidget::RestartRun()
{
	UWorld* World = GetWorld();
	UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	if (!Arena || Arena->GetState().RunResult == ENaiwaP4RunResult::InProgress
		|| !Arena->RestartWaveCombatPreview())
	{
		return;
	}
	if (RunResultPanel)
	{
		RunResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (BossHudPanel)
	{
		BossHudPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (UpgradeOfferPanel)
	{
		UpgradeOfferPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	DisplayedUpgradeOfferRevision = INDEX_NONE;
	SelectedCardIndex = INDEX_NONE;
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->bShowMouseCursor = false;
	}
}

void UNaiwaP4ArtIntegrationWidget::OnRetryRunClicked()
{
	RestartRun();
}

bool UNaiwaP4ArtIntegrationWidget::IsUpgradeOfferVisible() const
{
	return UpgradeOfferPanel && UpgradeOfferPanel->GetVisibility() == ESlateVisibility::Visible;
}
