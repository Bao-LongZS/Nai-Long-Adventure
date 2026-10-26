#include "NaiwaMenuWidget.h"

#include "NaiwaMenuProgress.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Parse.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"

namespace
{
    const FLinearColor BackgroundColor = FLinearColor::FromSRGBColor(FColor(247, 244, 238));
    const FLinearColor InkColor = FLinearColor::FromSRGBColor(FColor(30, 29, 27));
    const FLinearColor MutedColor = FLinearColor::FromSRGBColor(FColor(135, 126, 112));
    const FLinearColor BorderColor = FLinearColor::FromSRGBColor(FColor(226, 219, 207));
    const FLinearColor PaleColor = FLinearColor::FromSRGBColor(FColor(239, 233, 221));
    const FLinearColor AccentColor = FLinearColor::FromSRGBColor(FColor(220, 145, 48));

    UCanvasPanelSlot* PlaceAt(UCanvasPanel* Canvas, UWidget* Widget,
        const FVector2D& Position, const FVector2D& Size)
    {
        UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
        Slot->SetAnchors(FAnchors(Position.X, Position.Y));
        Slot->SetPosition(FVector2D::ZeroVector);
        Slot->SetSize(Size);
        return Slot;
    }

    UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, int32 Size,
        const FLinearColor& Color, bool bBold = false)
    {
        UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Block->SetText(FText::FromString(Text));
        Block->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size));
        Block->SetColorAndOpacity(Color);
        Block->SetAutoWrapText(true);
        return Block;
    }

    void StyleButton(UButton* Button, const FLinearColor& Normal)
    {
        FButtonStyle Style;
        Style.SetNormal(FSlateRoundedBoxBrush(Normal, 10.0f));
        Style.SetHovered(FSlateRoundedBoxBrush(FLinearColor::LerpUsingHSV(Normal, FLinearColor::White, 0.08f), 10.0f));
        Style.SetPressed(FSlateRoundedBoxBrush(FLinearColor::LerpUsingHSV(Normal, FLinearColor::Black, 0.08f), 10.0f));
        Style.SetDisabled(FSlateRoundedBoxBrush(Normal.CopyWithNewOpacity(0.65f), 10.0f));
        Style.SetNormalPadding(FMargin(0));
        Style.SetPressedPadding(FMargin(0));
        Button->SetStyle(Style);
    }
}

void UNaiwaMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    StartCharacterTexture = LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/Naiwa/UI/T_UI_StartNaiwa.T_UI_StartNaiwa"));
    SelectCharacterTexture = LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/Naiwa/UI/T_UI_LevelNaiwa_LeftFist.T_UI_LevelNaiwa_LeftFist"));
    Level11Thumbnail = LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/Naiwa/UI/T_UI_Thumbnail_11.T_UI_Thumbnail_11"));
    Level21Thumbnail = LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/Naiwa/UI/T_UI_Thumbnail_21.T_UI_Thumbnail_21"));
    Level22Thumbnail = LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/Naiwa/UI/T_UI_Thumbnail_22.T_UI_Thumbnail_22"));
    Level23Thumbnail = LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/Naiwa/UI/T_UI_Thumbnail_23.T_UI_Thumbnail_23"));

    RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MenuRoot"));
    WidgetTree->RootWidget = RootCanvas;

    FString MenuPage;
    if (GetWorld())
    {
        FParse::Value(*GetWorld()->URL.ToString(), TEXT("NaiwaMenuPage="), MenuPage);
    }
    if (MenuPage.Equals(TEXT("levels"), ESearchCase::IgnoreCase))
    {
        BuildLevelSelectPage();
    }
    else
    {
        BuildHomePage();
    }
}

void UNaiwaMenuWidget::ShowLevelSelect()
{
    BuildLevelSelectPage();
}

void UNaiwaMenuWidget::AddBackground()
{
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Background->SetBrushColor(BackgroundColor);
    UCanvasPanelSlot* BackgroundSlot = RootCanvas->AddChildToCanvas(Background);
    BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    BackgroundSlot->SetOffsets(FMargin(0.0f));
}

void UNaiwaMenuWidget::AddText(const FString& Label, const FVector2D& Position,
    const FVector2D& Size, int32 FontSize, const FLinearColor& Color,
    bool bBold, ETextJustify::Type Justification)
{
    UTextBlock* Block = MakeText(WidgetTree, Label, FontSize, Color, bBold);
    Block->SetJustification(Justification);
    PlaceAt(RootCanvas, Block, Position, Size);
}

UButton* UNaiwaMenuWidget::AddButton(const FString& Label, const FLinearColor& Color,
    const FVector2D& Position, const FVector2D& Size, int32 FontSize)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    StyleButton(Button, Color);
    UTextBlock* Text = MakeText(WidgetTree, Label, FontSize,
        Color.Equals(InkColor) ? BackgroundColor : InkColor, true);
    Text->SetJustification(ETextJustify::Center);
    UButtonSlot* ContentSlot = Cast<UButtonSlot>(Button->AddChild(Text));
    if (ContentSlot)
    {
        ContentSlot->SetHorizontalAlignment(HAlign_Fill);
        ContentSlot->SetVerticalAlignment(VAlign_Center);
    }
    PlaceAt(RootCanvas, Button, Position, Size);
    return Button;
}

void UNaiwaMenuWidget::BuildHomePage()
{
    RootCanvas->ClearChildren();
    AddBackground();

    AddText(TEXT("N A I W A   &   T H E   S L I N G S H O T"),
        FVector2D(0.075f, 0.205f), FVector2D(700.0f, 34.0f), 15, MutedColor);
    AddText(TEXT("奶蛙闯关"), FVector2D(0.075f, 0.285f), FVector2D(740.0f, 118.0f), 76, InkColor, true);
    AddText(TEXT("1-1 平台关     →     2-1 / 2-2 / 2-3 弹弓关     ·     同一局连闯"),
        FVector2D(0.078f, 0.435f), FVector2D(790.0f, 48.0f), 22, MutedColor);

    UButton* StartButton = AddButton(TEXT("开    始"), InkColor,
        FVector2D(0.075f, 0.585f), FVector2D(300.0f, 70.0f), 22);
    StartButton->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnStartClicked);
    UButton* ExitButton = AddButton(TEXT("退    出"), BackgroundColor,
        FVector2D(0.255f, 0.585f), FVector2D(180.0f, 70.0f), 20);
    StyleButton(ExitButton, BackgroundColor);
    ExitButton->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnExitClicked);

    AddText(TEXT("naiwa build 2026.10     ·     按任意键继续"),
        FVector2D(0.075f, 0.91f), FVector2D(540.0f, 28.0f), 14, MutedColor);

    if (StartCharacterTexture)
    {
        UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
        Scale->SetStretch(EStretch::ScaleToFit);
        UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        Image->SetBrushFromTexture(StartCharacterTexture, false);
        Scale->SetContent(Image);
        UCanvasPanelSlot* ImageSlot = RootCanvas->AddChildToCanvas(Scale);
        ImageSlot->SetAnchors(FAnchors(0.69f, 0.17f, 0.96f, 0.90f));
        ImageSlot->SetOffsets(FMargin(0.0f));
    }
}

void UNaiwaMenuWidget::AddLevelCard(const FString& LevelId, const FString& Title,
    const FString& Subtitle, bool bUnlocked, float Left, float Right)
{
    const bool bSelected = SelectedLevel == LevelId;
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    StyleButton(Button, bSelected ? PaleColor : FLinearColor::White);
    Button->SetIsEnabled(bUnlocked);
    UCanvasPanelSlot* ButtonSlot = RootCanvas->AddChildToCanvas(Button);
    ButtonSlot->SetAnchors(FAnchors(Left, 0.365f, Right, 0.755f));
    ButtonSlot->SetOffsets(FMargin(0.0f));

    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Button->AddChild(Stack);

    USizeBox* ArtSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    ArtSize->SetWidthOverride(182.0f);
    ArtSize->SetHeightOverride(182.0f);
    UImage* ArtImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
    UTexture2D* Thumbnail = nullptr;
    if (LevelId == TEXT("1-1")) Thumbnail = Level11Thumbnail;
    else if (LevelId == TEXT("2-1")) Thumbnail = Level21Thumbnail;
    else if (LevelId == TEXT("2-2")) Thumbnail = Level22Thumbnail;
    else if (LevelId == TEXT("2-3")) Thumbnail = Level23Thumbnail;
    if (Thumbnail)
    {
        ArtImage->SetBrushFromTexture(Thumbnail, true);
        ArtImage->SetColorAndOpacity(bUnlocked
            ? FLinearColor::White
            : FLinearColor(0.82f, 0.82f, 0.82f, 0.62f));
    }
    else
    {
        ArtImage->SetColorAndOpacity(bUnlocked ? PaleColor : BorderColor.CopyWithNewOpacity(0.45f));
    }
    ArtSize->AddChild(ArtImage);
    UVerticalBoxSlot* ArtSlot = Stack->AddChildToVerticalBox(ArtSize);
    ArtSlot->SetPadding(FMargin(12.0f, 12.0f, 12.0f, 4.0f));
    ArtSlot->SetHorizontalAlignment(HAlign_Center);

    UVerticalBox* Details = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    UTextBlock* Number = MakeText(WidgetTree, LevelId, 15, MutedColor);
    UTextBlock* Name = MakeText(WidgetTree, Title, 25, bUnlocked ? InkColor : MutedColor, true);
    UTextBlock* State = MakeText(WidgetTree, Subtitle, 15,
        bUnlocked ? MutedColor : AccentColor);
    Number->SetAutoWrapText(false);
    Name->SetAutoWrapText(false);
    State->SetAutoWrapText(true);
    Details->AddChildToVerticalBox(Number)->SetPadding(FMargin(0, 8, 0, 1));
    Details->AddChildToVerticalBox(Name)->SetPadding(FMargin(0, 0, 0, 2));
    Details->AddChildToVerticalBox(State)->SetPadding(FMargin(0, 0, 0, 0));
    UVerticalBoxSlot* DetailsSlot = Stack->AddChildToVerticalBox(Details);
    DetailsSlot->SetPadding(FMargin(24.0f, 9.0f, 20.0f, 10.0f));

    if (LevelId == TEXT("1-1")) Button->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnSelect11);
    if (LevelId == TEXT("2-1")) Button->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnSelect21);
    if (LevelId == TEXT("2-2")) Button->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnSelect22);
    if (LevelId == TEXT("2-3")) Button->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnSelect23);
}

void UNaiwaMenuWidget::BuildLevelSelectPage()
{
    RootCanvas->ClearChildren();
    AddBackground();

    AddText(TEXT("奶蛙闯关"), FVector2D(0.05f, 0.065f), FVector2D(330.0f, 42.0f), 25, InkColor, true);
    AddText(TEXT("关卡进度"), FVector2D(0.79f, 0.075f), FVector2D(340.0f, 32.0f), 16, MutedColor, false, ETextJustify::Right);

    UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Divider->SetBrushColor(BorderColor);
    UCanvasPanelSlot* DividerSlot = RootCanvas->AddChildToCanvas(Divider);
    DividerSlot->SetAnchors(FAnchors(0.05f, 0.142f, 0.77f, 0.144f));
    DividerSlot->SetOffsets(FMargin(0.0f));

    AddText(TEXT("S E L E C T   L E V E L"), FVector2D(0.05f, 0.18f), FVector2D(360.0f, 26.0f), 14, MutedColor);
    AddText(TEXT("关卡选择"), FVector2D(0.05f, 0.215f), FVector2D(620.0f, 78.0f), 54, InkColor, true);
    AddText(TEXT("1-1 与 2-1 可直接进入；2-2 需通关 2-1，2-3 需通关 2-2。"),
        FVector2D(0.05f, 0.29f), FVector2D(770.0f, 34.0f), 18, MutedColor);

    UNaiwaProgressSubsystem* Progress = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UNaiwaProgressSubsystem>() : nullptr;
    const int32 ClearedCount = Progress ? Progress->GetClearedLevelCount() : 0;
    AddText(FString::Printf(TEXT("通关 %d / 4     ·     收集进度保存于本机"), ClearedCount),
        FVector2D(0.78f, 0.18f), FVector2D(0.17f, 0.035f), 15, MutedColor, false, ETextJustify::Right);

    if (SelectCharacterTexture)
    {
        UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
        Scale->SetStretch(EStretch::ScaleToFit);
        UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        Image->SetBrushFromTexture(SelectCharacterTexture, false);
        Scale->SetContent(Image);
        UCanvasPanelSlot* ImageSlot = RootCanvas->AddChildToCanvas(Scale);
        ImageSlot->SetAnchors(FAnchors(0.82f, 0.055f, 0.965f, 0.285f));
        ImageSlot->SetOffsets(FMargin(0.0f));
    }

    FNaiwaLevelRecord Record;
    const bool bCleared11 = Progress && Progress->GetLevelRecord(TEXT("1-1"), Record) && Record.bCleared;
    const FString Sub11 = bCleared11
        ? FString::Printf(TEXT("已通关    %.1f 秒    %d 死亡"), Record.BestTimeSeconds, Record.BestDeaths)
        : TEXT("平台关    ·    直接进入");
    const bool bCleared21 = Progress && Progress->GetLevelRecord(TEXT("2-1"), Record) && Record.bCleared;
    const FString Sub21 = bCleared21
        ? FString::Printf(TEXT("已通关    %d 发    最佳记录"), Record.BestShots)
        : TEXT("弹弓关    ·    直接进入");
    const bool bCleared22 = Progress && Progress->GetLevelRecord(TEXT("2-2"), Record) && Record.bCleared;
    const FString Sub22 = bCleared22
        ? FString::Printf(TEXT("已通关    %d 发    最佳记录"), Record.BestShots)
        : TEXT("未解锁    ·    通关 2-1");
    const bool bUnlocked22 = Progress && Progress->IsLevelUnlocked(TEXT("2-2"));
    const bool bCleared23 = Progress && Progress->GetLevelRecord(TEXT("2-3"), Record) && Record.bCleared;
    const FString Sub23 = bCleared23
        ? FString::Printf(TEXT("已通关    %d 发    最佳记录"), Record.BestShots)
        : TEXT("未解锁    ·    通关 2-2");
    const bool bUnlocked23 = Progress && Progress->IsLevelUnlocked(TEXT("2-3"));

    AddLevelCard(TEXT("1-1"), TEXT("斩首之前"), Sub11, true, 0.05f, 0.265f);
    AddLevelCard(TEXT("2-1"), TEXT("斩首"), Sub21, true, 0.29f, 0.505f);
    AddLevelCard(TEXT("2-2"), TEXT("选蛋"), Sub22, bUnlocked22, 0.53f, 0.745f);
    AddLevelCard(TEXT("2-3"), TEXT("综合"), Sub23, bUnlocked23, 0.77f, 0.985f);

    UBorder* DetailPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    DetailPanel->SetBrushColor(FLinearColor::White);
    UCanvasPanelSlot* DetailSlot = RootCanvas->AddChildToCanvas(DetailPanel);
    DetailSlot->SetAnchors(FAnchors(0.05f, 0.80f, 0.95f, 0.94f));
    DetailSlot->SetOffsets(FMargin(0.0f));
    AddText(SelectedLevel, FVector2D(0.065f, 0.818f), FVector2D(180.0f, 34.0f), 24, InkColor, true);

    FString Detail = TEXT("选择一关开始。1-1 通关后会自动进入 2-1；从 2-1 开始的流程不会返回 1-1。");
    if (SelectedLevel == TEXT("2-1")) Detail = TEXT("弹弓关    ·    可直接进入    ·    通关后解锁 2-2");
    if (SelectedLevel == TEXT("2-2")) Detail = TEXT("弹弓关    ·    通关后解锁 2-3");
    if (SelectedLevel == TEXT("2-3")) Detail = TEXT("综合关    ·    完成两阶段后返回关卡选择");
    AddText(Detail, FVector2D(0.065f, 0.858f), FVector2D(980.0f, 30.0f), 16, MutedColor);

    StartLevelButton = AddButton(TEXT("开始这一关"), InkColor,
        FVector2D(0.835f, 0.823f), FVector2D(220.0f, 60.0f), 20);
    StartLevelButton->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnStartSelectedLevel);
    StartLevelButton->SetIsEnabled(SelectedLevel == TEXT("1-1") || SelectedLevel == TEXT("2-1")
        || (Progress && Progress->IsLevelUnlocked(SelectedLevel)));
}

void UNaiwaMenuWidget::SelectLevel(const FString& LevelId)
{
    UNaiwaProgressSubsystem* Progress = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UNaiwaProgressSubsystem>() : nullptr;
    if (!Progress || !Progress->IsLevelUnlocked(LevelId)) return;
    SelectedLevel = LevelId;
    BuildLevelSelectPage();
}

void UNaiwaMenuWidget::OnStartClicked()
{
    SelectedLevel = TEXT("1-1");
    BuildLevelSelectPage();
}

void UNaiwaMenuWidget::OnExitClicked()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UNaiwaMenuWidget::OnStartSelectedLevel()
{
    if (SelectedLevel == TEXT("1-1"))
    {
        UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Naiwa/P1/Maps/L_NaiwaP1_v7")),
            true, TEXT("NaiwaLinkRun=1"));
    }
    else if (SelectedLevel == TEXT("2-1") || SelectedLevel == TEXT("2-2") || SelectedLevel == TEXT("2-3"))
    {
        FString MapPath;
        FString Options(TEXT("NaiwaLinkRun=1"));
        if (SelectedLevel == TEXT("2-1")) MapPath = TEXT("/Game/Naiwa/P2/Maps/L_NaiwaP2_21");
        else if (SelectedLevel == TEXT("2-2")) MapPath = TEXT("/Game/Naiwa/P2/Maps/L_NaiwaP2_22");
        else MapPath = TEXT("/Game/Naiwa/P2/Maps/L_NaiwaP2_23");
        if (SelectedLevel == TEXT("2-3")) Options += TEXT("?NaiwaP2Stage=1");
        UGameplayStatics::OpenLevel(this, FName(*MapPath), true, Options);
    }
}

void UNaiwaMenuWidget::OnSelect11() { SelectLevel(TEXT("1-1")); }
void UNaiwaMenuWidget::OnSelect21() { SelectLevel(TEXT("2-1")); }
void UNaiwaMenuWidget::OnSelect22() { SelectLevel(TEXT("2-2")); }
void UNaiwaMenuWidget::OnSelect23() { SelectLevel(TEXT("2-3")); }
