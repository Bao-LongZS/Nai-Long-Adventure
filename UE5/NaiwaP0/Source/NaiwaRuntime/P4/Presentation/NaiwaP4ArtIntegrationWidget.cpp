#include "NaiwaP4ArtIntegrationWidget.h"

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
}

void UNaiwaP4ArtIntegrationWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("P4ArtIntegrationRoot"));
	WidgetTree->RootWidget = RootCanvas;
	BuildBattleHud();
	BuildTelegraphPreview();
	BuildUpgradeOffer();
}

void UNaiwaP4ArtIntegrationWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UWorld* World = GetWorld();
	UNaiwaP4ArenaSubsystem* Arena = World ? World->GetSubsystem<UNaiwaP4ArenaSubsystem>() : nullptr;
	const UNaiwaP4Settings* Settings = GetDefault<UNaiwaP4Settings>();
	if (!Arena || !Settings || !HealthBar || !HealthText)
	{
		return;
	}

	const FNaiwaP4EntityState* Player = Arena->GetState().Entities.FindByPredicate([](const FNaiwaP4EntityState& Entity)
	{
		return Entity.Kind == ENaiwaP4EntityKind::Player;
	});
	if (!Player)
	{
		return;
	}

	const int32 MaxHP = FMath::Max(Settings->PlayerMaxHP, 1);
	HealthBar->SetPercent(FMath::Clamp(static_cast<float>(Player->HP) / MaxHP, 0.0f, 1.0f));
	HealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Player->HP, MaxHP)));
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
	HudSlot->SetSize(FVector2D(360.0f, 164.0f));

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

	HudStack->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("能量 / 波次：待战斗模拟接入"), 13, MutedColor));
	HudStack->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("Tab 卡面预览  ·  T 预警样式预览"), 13, MutedColor));
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
	PanelSlot->SetSize(FVector2D(330.0f, 92.0f));

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("P4TelegraphStylePreviewStack"));
	Panel->SetContent(Stack);
	Stack->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("预警样式预览 · T 切换"), 14, PaperColor, true));
	TelegraphPreviewText = MakeText(WidgetTree, TEXT("攻击范围：红色圆环"), 22,
		FLinearColor(0.95f, 0.27f, 0.21f, 1.0f), true);
	Stack->AddChildToVerticalBox(TelegraphPreviewText);
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

void UNaiwaP4ArtIntegrationWidget::BuildUpgradeOffer()
{
	UpgradeOfferPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("P4UpgradeOfferOverlay"));
	UpgradeOfferPanel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.08f, 0.075f, 0.065f, 0.93f), 0.0f));
	UpgradeOfferPanel->SetPadding(FMargin(28.0f));
	UpgradeOfferPanel->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* OverlaySlot = RootCanvas->AddChildToCanvas(UpgradeOfferPanel);
	OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	OverlaySlot->SetOffsets(FMargin(0.0f));

	UVerticalBox* OfferStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("P4UpgradeOfferStack"));
	UpgradeOfferPanel->SetContent(OfferStack);
	OfferStack->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("本章强化"), 32, PaperColor, true));
	OfferStack->AddChildToVerticalBox(MakeText(WidgetTree,
		TEXT("卡名、数值和说明来自 P4 Alpha DataTable。此面板只预览选择，不发放战斗效果。"),
		15, FLinearColor(0.92f, 0.88f, 0.80f, 1.0f)));

	UpgradeCardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("P4UpgradeCardRow"));
	if (UVerticalBoxSlot* CardRowSlot = OfferStack->AddChildToVerticalBox(UpgradeCardRow))
	{
		CardRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CardRowSlot->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 12.0f));
	}

	const TArray<FName> OrderedCards = { FName(TEXT("bounce_ball")), FName(TEXT("rounder_belly")), FName(TEXT("cant_hold")) };
	for (int32 Index = 0; Index < OrderedCards.Num(); ++Index)
	{
		AddCard(OrderedCards[Index], Index);
	}

	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("P4UpgradeOfferFooter"));
	OfferStack->AddChildToVerticalBox(Footer);
	UButton* ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("P4ConfirmPreviewButton"));
	StyleButton(ConfirmButton);
	ConfirmButton->SetBackgroundColor(FLinearColor(0.96f, 0.78f, 0.38f, 1.0f));
	ConfirmButton->OnClicked.AddDynamic(this, &ThisClass::OnConfirmPreviewClicked);
	ConfirmButton->AddChild(MakeText(WidgetTree, TEXT("确认预览选择"), 17, InkColor, true));
	Footer->AddChildToHorizontalBox(ConfirmButton);
	ConfirmPreviewText = MakeText(WidgetTree, TEXT("按 Tab 关闭"), 14, FLinearColor(0.92f, 0.88f, 0.80f, 1.0f));
	if (UHorizontalBoxSlot* StatusSlot = Footer->AddChildToHorizontalBox(ConfirmPreviewText))
	{
		StatusSlot->SetPadding(FMargin(18.0f, 8.0f));
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
	UButton* CardButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("P4UpgradeCard_%s"), *CardId.ToString()));
	StyleButton(CardButton);
	if (CardIndex == 0) CardButton->OnClicked.AddDynamic(this, &ThisClass::OnCard0Clicked);
	else if (CardIndex == 1) CardButton->OnClicked.AddDynamic(this, &ThisClass::OnCard1Clicked);
	else CardButton->OnClicked.AddDynamic(this, &ThisClass::OnCard2Clicked);
	if (UHorizontalBoxSlot* CardSlot = UpgradeCardRow ? UpgradeCardRow->AddChildToHorizontalBox(CardButton) : nullptr)
	{
		CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CardSlot->SetPadding(FMargin(9.0f, 0.0f));
	}

	UBorder* Surface = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Surface->SetBrush(FSlateRoundedBoxBrush(PaperColor, 12.0f));
	Surface->SetPadding(FMargin(18.0f));
	CardButton->AddChild(Surface);
	UpgradeButtons.Add(CardButton);
	UpgradeCardSurfaces.Add(Surface);

	UVerticalBox* CardStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Surface->SetContent(CardStack);
	UHorizontalBox* CategoryRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	CardStack->AddChildToVerticalBox(CategoryRow);

	UTexture2D* CategoryMark = Row->CategoryMarkSoftRef.LoadSynchronous();
	if (!CategoryMark)
	{
		CategoryMark = LoadObject<UTexture2D>(nullptr, *CategoryTexturePath(Row->CategoryId));
	}
	if (CategoryMark)
	{
		UImage* MarkImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		MarkImage->SetBrushFromTexture(CategoryMark, true);
		MarkImage->SetDesiredSizeOverride(FVector2D(24.0f, 24.0f));
		CategoryRow->AddChildToHorizontalBox(MarkImage);
	}
	UTextBlock* CategoryText = MakeText(WidgetTree, Row->CategoryLabel.ToString(), 14, Accent, true);
	if (UHorizontalBoxSlot* CategoryTextSlot = CategoryRow->AddChildToHorizontalBox(CategoryText))
	{
		CategoryTextSlot->SetPadding(FMargin(8.0f, 2.0f));
	}

	UTexture2D* Icon = Row->IconSoftRef.LoadSynchronous();
	if (Icon)
	{
		UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		IconImage->SetBrushFromTexture(Icon, true);
		IconImage->SetDesiredSizeOverride(FVector2D(128.0f, 128.0f));
		if (UVerticalBoxSlot* IconSlot = CardStack->AddChildToVerticalBox(IconImage))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 8.0f));
		}
	}

	CardStack->AddChildToVerticalBox(MakeText(WidgetTree, Row->Title.ToString(), 24, InkColor, true));
	CardStack->AddChildToVerticalBox(MakeText(WidgetTree, Row->Description.ToString(), 16, InkColor));
	if (!Row->TriggerRule.IsEmpty())
	{
		CardStack->AddChildToVerticalBox(MakeText(WidgetTree, Row->TriggerRule.ToString(), 13, MutedColor));
	}
	if (!Row->ComboRule.IsEmpty())
	{
		CardStack->AddChildToVerticalBox(MakeText(WidgetTree, Row->ComboRule.ToString(), 13, MutedColor));
	}
}

void UNaiwaP4ArtIntegrationWidget::SelectCard(const int32 CardIndex)
{
	if (!UpgradeCardSurfaces.IsValidIndex(CardIndex) || !UpgradeCardIds.IsValidIndex(CardIndex))
	{
		return;
	}
	SelectedCardIndex = CardIndex;
	for (int32 Index = 0; Index < UpgradeCardSurfaces.Num(); ++Index)
	{
		UBorder* Surface = UpgradeCardSurfaces[Index];
		Surface->SetBrushColor(Index == SelectedCardIndex
			? FLinearColor(1.0f, 0.88f, 0.58f, 1.0f)
			: FLinearColor(1.0f, 0.973f, 0.914f, 0.96f));
	}
	if (ConfirmPreviewText)
	{
		ConfirmPreviewText->SetText(FText::FromString(FString::Printf(
			TEXT("已选：%s · 点击确认仅展示回执"), *UpgradeCardIds[CardIndex].ToString())));
	}
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
	ConfirmPreviewText->SetText(FText::FromString(FString::Printf(
		TEXT("预览已确认：%s。战斗奖励尚未接入。"), *UpgradeCardIds[SelectedCardIndex].ToString())));
}

void UNaiwaP4ArtIntegrationWidget::ToggleUpgradeOffer()
{
	if (!UpgradeOfferPanel)
	{
		return;
	}
	const bool bShow = UpgradeOfferPanel->GetVisibility() != ESlateVisibility::Visible;
	UpgradeOfferPanel->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bShow && SelectedCardIndex == INDEX_NONE && !UpgradeCardSurfaces.IsEmpty())
	{
		SelectCard(0);
	}
}

bool UNaiwaP4ArtIntegrationWidget::IsUpgradeOfferVisible() const
{
	return UpgradeOfferPanel && UpgradeOfferPanel->GetVisibility() == ESlateVisibility::Visible;
}
