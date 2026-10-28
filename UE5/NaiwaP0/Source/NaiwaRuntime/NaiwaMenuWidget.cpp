#include "NaiwaMenuWidget.h"

#include "NaiwaMenuProgress.h"
#include "P5/Data/NaiwaP5SkinCatalog.h"
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
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"

namespace
{
    const FLinearColor BackgroundColor = FLinearColor::FromSRGBColor(FColor(247, 244, 238));
    const FLinearColor InkColor = FLinearColor::FromSRGBColor(FColor(30, 29, 27));
    const FLinearColor MutedColor = FLinearColor::FromSRGBColor(FColor(135, 126, 112));
    const FLinearColor BorderColor = FLinearColor::FromSRGBColor(FColor(226, 219, 207));
    const FLinearColor PaleColor = FLinearColor::FromSRGBColor(FColor(239, 233, 221));

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
    const FString MenuImageDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceAssets/MenuUI"));
    const auto LoadMenuImage = [&MenuImageDirectory](const TCHAR* Filename) -> UTexture2D*
    {
        return FImageUtils::ImportFileAsTexture2D(FPaths::Combine(MenuImageDirectory, Filename));
    };
    SelectCharacterTexture = LoadMenuImage(TEXT("T_UI_LevelNaiwa_RightCharacter.png"));
    if (!SelectCharacterTexture)
    {
        SelectCharacterTexture = LoadObject<UTexture2D>(nullptr,
            TEXT("/Game/Naiwa/UI/T_UI_LevelNaiwa_LeftFist.T_UI_LevelNaiwa_LeftFist"));
    }
    LevelSelectBackgroundTexture = LoadMenuImage(TEXT("T_UI_LevelSelect_Background.png"));
    Level11Thumbnail = LoadMenuImage(TEXT("T_UI_LevelSelect_11.png"));
    Level21Thumbnail = LoadMenuImage(TEXT("T_UI_LevelSelect_21.png"));
    Level22Thumbnail = LoadMenuImage(TEXT("T_UI_LevelSelect_22.png"));
    Level23Thumbnail = LoadMenuImage(TEXT("T_UI_LevelSelect_23.png"));
    Level31Thumbnail = LoadMenuImage(TEXT("T_UI_LevelSelect_31.png"));
    Level32Thumbnail = LoadMenuImage(TEXT("T_UI_LevelSelect_32.png"));

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

void UNaiwaMenuWidget::BuildWardrobePage()
{
    RootCanvas->ClearChildren();
    AddBackground();
    AddText(TEXT("衣柜"), FVector2D(0.12f, 0.08f), FVector2D(500.0f, 72.0f), 48, InkColor, true);
    AddText(TEXT("能力和外观由同一份 P5/P6 皮肤配置驱动；永久解锁只使用奶票。"),
        FVector2D(0.12f, 0.16f), FVector2D(900.0f, 38.0f), 18, MutedColor);

    UNaiwaProgressSubsystem* Progress = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UNaiwaProgressSubsystem>() : nullptr;
    AddText(FString::Printf(TEXT("奶票余额  %d"), Progress ? Progress->GetMilkTickets() : 0),
        FVector2D(0.72f, 0.105f), FVector2D(320.0f, 44.0f), 24, InkColor, true, ETextJustify::Right);

    const TArray<FNaiwaP5SkinRow>& Rows = FNaiwaP5SkinCatalog::GetRows();
    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        const FNaiwaP5SkinRow& Row = Rows[Index];
        const float Y = 0.235f + static_cast<float>(Index) * 0.12f;
        UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Card->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 8.0f));
        Card->SetBrushColor(FLinearColor(0.97f, 0.95f, 0.91f, 0.94f));
        UCanvasPanelSlot* CardSlot = RootCanvas->AddChildToCanvas(Card);
        CardSlot->SetAnchors(FAnchors(0.12f, Y - 0.01f, 0.88f, Y + 0.105f));
        CardSlot->SetOffsets(FMargin(0.0f));

        AddText(Row.DisplayName, FVector2D(0.15f, Y), FVector2D(480.0f, 34.0f), 24, InkColor, true);
        AddText(FString::Printf(TEXT("%s   ·   %d 奶票"), *Row.AbilityDescription, Row.TicketPrice),
            FVector2D(0.15f, Y + 0.045f), FVector2D(900.0f, 32.0f), 16, MutedColor);

        const bool bOwned = Progress && Progress->IsP5SkinOwned(Row.SkinId);
        const bool bSelected = Progress && Progress->GetSelectedP5SkinId() == Row.SkinId;
        const FString ActionLabel = bOwned
            ? (bSelected ? TEXT("已装备") : TEXT("装备"))
            : FString::Printf(TEXT("购买 %d 票"), Row.TicketPrice);
        UButton* ActionButton = AddButton(ActionLabel, bSelected ? InkColor : PaleColor,
            FVector2D(0.735f, Y + 0.013f), FVector2D(190.0f, 48.0f), 18);
        if (Row.SkinId == FName(TEXT("NaiwaSkin_Base"))) ActionButton->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnWardrobeBaseClicked);
        else if (Row.SkinId == FName(TEXT("NaiwaSkin_NaiYu"))) ActionButton->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnWardrobeNaiYuClicked);
        else if (Row.SkinId == FName(TEXT("NaiwaSkin_NaiCha"))) ActionButton->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnWardrobeNaiChaClicked);
        else if (Row.SkinId == FName(TEXT("NaiwaSkin_NaiHao"))) ActionButton->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnWardrobeNaiHaoClicked);
        else if (Row.SkinId == FName(TEXT("NaiwaSkin_NaiSha"))) ActionButton->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnWardrobeNaiShaClicked);
    }

    AddText(WardrobeFeedback, FVector2D(0.14f, 0.845f), FVector2D(800.0f, 34.0f), 17, MutedColor);
    UButton* ReturnButton = AddButton(TEXT("返回主菜单"), BackgroundColor,
        FVector2D(0.025f, 0.91f), FVector2D(190.0f, 54.0f), 18);
    ReturnButton->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnWardrobeBackClicked);
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
    AddText(TEXT("七关主线：1-1 → 2-1 → 2-2 → 2-3 → 3-1 → 3-2 → 4-1"),
        FVector2D(0.078f, 0.435f), FVector2D(790.0f, 48.0f), 22, MutedColor);

    UNaiwaProgressSubsystem* Progress = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UNaiwaProgressSubsystem>() : nullptr;
    AddText(FString::Printf(TEXT("奶票  %d"), Progress ? Progress->GetMilkTickets() : 0),
        FVector2D(0.075f, 0.525f), FVector2D(180.0f, 40.0f), 20, MutedColor, true);

    UButton* StartButton = AddButton(TEXT("开    始"), InkColor,
        FVector2D(0.075f, 0.585f), FVector2D(300.0f, 70.0f), 22);
    StartButton->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnStartClicked);
    UButton* WardrobeButton = AddButton(TEXT("衣    柜"), PaleColor,
        FVector2D(0.42f, 0.585f), FVector2D(180.0f, 70.0f), 20);
    WardrobeButton->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnWardrobeClicked);
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

void UNaiwaMenuWidget::AddLevelCard(const FString& LevelId, bool bUnlocked,
    const FVector2D& Position, const FVector2D& Size)
{
    const bool bSelected = SelectedLevel == LevelId;
    UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Frame->SetBrush(FSlateRoundedBoxBrush(bSelected ? InkColor : FLinearColor::White,
        bSelected ? 14.0f : 12.0f));
    Frame->SetBrushColor(bSelected ? InkColor : FLinearColor(0.97f, 0.95f, 0.91f, 0.86f));
    Frame->SetPadding(FMargin(bSelected ? 2.0f : 0.0f));
    UCanvasPanelSlot* FrameSlot = RootCanvas->AddChildToCanvas(Frame);
    FrameSlot->SetAnchors(FAnchors(Position.X, Position.Y,
        Position.X + Size.X, Position.Y + Size.Y));
    FrameSlot->SetOffsets(FMargin(0.0f));

    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    StyleButton(Button, bSelected ? FLinearColor::White : PaleColor);
    Button->SetIsEnabled(bUnlocked);
    Frame->SetContent(Button);

    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    UButtonSlot* ContentSlot = Cast<UButtonSlot>(Button->AddChild(Stack));
    if (ContentSlot)
    {
        ContentSlot->SetHorizontalAlignment(HAlign_Fill);
        ContentSlot->SetVerticalAlignment(VAlign_Top);
    }

    USizeBox* ArtSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    ArtSize->SetWidthOverride(CurrentLevelPage == 0 ? 420.0f
        : CurrentLevelPage == 1 ? 370.0f : 410.0f);
    ArtSize->SetHeightOverride(CurrentLevelPage == 0 ? 350.0f
        : CurrentLevelPage == 1 ? 330.0f : 370.0f);
    UImage* ArtImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
    UTexture2D* Thumbnail = nullptr;
    if (LevelId == TEXT("1-1")) Thumbnail = Level11Thumbnail;
    else if (LevelId == TEXT("2-1")) Thumbnail = Level21Thumbnail;
    else if (LevelId == TEXT("2-2")) Thumbnail = Level22Thumbnail;
    else if (LevelId == TEXT("2-3")) Thumbnail = Level23Thumbnail;
    else if (LevelId == TEXT("3-1")) Thumbnail = Level31Thumbnail;
    else if (LevelId == TEXT("3-2")) Thumbnail = Level32Thumbnail;
    if (Thumbnail)
    {
        ArtImage->SetBrushFromTexture(Thumbnail, false);
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
    ArtSlot->SetPadding(FMargin(8.0f, 12.0f, 8.0f, 5.0f));
    ArtSlot->SetHorizontalAlignment(HAlign_Center);

    UTextBlock* Number = MakeText(WidgetTree, LevelId, 32, InkColor, true);
    Number->SetAutoWrapText(false);
    Number->SetJustification(ETextJustify::Center);
    Stack->AddChildToVerticalBox(Number)->SetPadding(FMargin(8.0f, 0.0f, 8.0f, 0.0f));

    if (LevelId == TEXT("1-1")) Button->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnSelect11);
    if (LevelId == TEXT("2-1")) Button->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnSelect21);
    if (LevelId == TEXT("2-2")) Button->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnSelect22);
    if (LevelId == TEXT("2-3")) Button->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnSelect23);
    if (LevelId == TEXT("3-1")) Button->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnSelect31);
    if (LevelId == TEXT("3-2")) Button->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnSelect32);
    if (LevelId == TEXT("4-1")) Button->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnSelect41);
}

void UNaiwaMenuWidget::BuildLevelSelectPage()
{
    RootCanvas->ClearChildren();
    AddBackground();

    if (LevelSelectBackgroundTexture)
    {
        UImage* BackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        BackgroundImage->SetBrushFromTexture(LevelSelectBackgroundTexture, false);
        UCanvasPanelSlot* BackgroundSlot = RootCanvas->AddChildToCanvas(BackgroundImage);
        BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        BackgroundSlot->SetOffsets(FMargin(0.0f));
    }

    const TCHAR* StageTitle = CurrentLevelPage == 0 ? TEXT("第一大关")
        : CurrentLevelPage == 1 ? TEXT("第二大关")
        : CurrentLevelPage == 2 ? TEXT("第三大关") : TEXT("第四大关");
    AddText(TEXT("N A I W A   &   T H E   S L I N G S H O T"),
        FVector2D(0.05f, 0.055f), FVector2D(560.0f, 28.0f), 15, MutedColor);
    AddText(TEXT("选择关卡"), FVector2D(0.05f, 0.115f), FVector2D(620.0f, 82.0f), 58, InkColor, true);
    AddText(StageTitle, FVector2D(0.05f, 0.225f), FVector2D(420.0f, 48.0f), 34, MutedColor, true);

    AddText(FString::Printf(TEXT("%02d / 04"), CurrentLevelPage + 1),
        FVector2D(0.82f, 0.065f), FVector2D(240.0f, 38.0f), 29, InkColor, true, ETextJustify::Right);
    for (int32 DotIndex = 0; DotIndex < 4; ++DotIndex)
    {
        AddText(TEXT("●"), FVector2D(0.895f + DotIndex * 0.026f, 0.124f),
            FVector2D(24.0f, 24.0f), 14, DotIndex == CurrentLevelPage ? InkColor : BorderColor,
            false, ETextJustify::Center);
    }

    UNaiwaProgressSubsystem* Progress = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UNaiwaProgressSubsystem>() : nullptr;

    if (SelectCharacterTexture)
    {
        UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        Image->SetBrushFromTexture(SelectCharacterTexture, false);
        UCanvasPanelSlot* ImageSlot = RootCanvas->AddChildToCanvas(Image);
        ImageSlot->SetAnchors(FAnchors(0.83f, 0.565f, 1.0f, 0.94f));
        ImageSlot->SetOffsets(FMargin(0.0f));
    }

    const bool bUnlocked22 = Progress && Progress->IsLevelUnlocked(TEXT("2-2"));
    const bool bUnlocked23 = Progress && Progress->IsLevelUnlocked(TEXT("2-3"));
    const bool bUnlocked31 = Progress && Progress->IsLevelUnlocked(TEXT("3-1"));
    const bool bUnlocked32 = Progress && Progress->IsLevelUnlocked(TEXT("3-2"));
    const bool bUnlocked41 = Progress && Progress->IsLevelUnlocked(TEXT("4-1"));

    FVector2D SelectedPosition(0.38f, 0.267f);
    FVector2D CardSize(0.25f, 0.52f);
    float StartButtonY = 0.704f;
    if (CurrentLevelPage == 0)
    {
        AddLevelCard(TEXT("1-1"), true, SelectedPosition, CardSize);
    }
    else if (CurrentLevelPage == 1)
    {
        const FVector2D CardSizeP2(0.212f, 0.465f);
        const FVector2D SelectedCardSizeP2(0.212f, 0.525f);
        const FVector2D Pos21(0.165f, SelectedLevel == TEXT("2-1") ? 0.295f : 0.313f);
        const FVector2D Pos22(0.395f, SelectedLevel == TEXT("2-2") ? 0.295f : 0.313f);
        const FVector2D Pos23(0.625f, SelectedLevel == TEXT("2-3") ? 0.295f : 0.313f);
        const FVector2D Size21 = SelectedLevel == TEXT("2-1") ? SelectedCardSizeP2 : CardSizeP2;
        const FVector2D Size22 = SelectedLevel == TEXT("2-2") ? SelectedCardSizeP2 : CardSizeP2;
        const FVector2D Size23 = SelectedLevel == TEXT("2-3") ? SelectedCardSizeP2 : CardSizeP2;
        AddLevelCard(TEXT("2-1"), true, Pos21, Size21);
        AddLevelCard(TEXT("2-2"), bUnlocked22, Pos22, Size22);
        AddLevelCard(TEXT("2-3"), bUnlocked23, Pos23, Size23);
        SelectedPosition = SelectedLevel == TEXT("2-2") ? Pos22
            : SelectedLevel == TEXT("2-3") ? Pos23 : Pos21;
        CardSize = SelectedLevel == TEXT("2-2") ? Size22
            : SelectedLevel == TEXT("2-3") ? Size23 : Size21;
        StartButtonY = 0.738f;
    }
    else if (CurrentLevelPage == 2)
    {
        const FVector2D CardSizeP3(0.25f, 0.495f);
        const FVector2D SelectedCardSizeP3(0.25f, 0.54f);
        const FVector2D Pos31(0.245f, SelectedLevel == TEXT("3-1") ? 0.294f : 0.30f);
        const FVector2D Pos32(0.512f, SelectedLevel == TEXT("3-2") ? 0.294f : 0.30f);
        const FVector2D Size31 = SelectedLevel == TEXT("3-1") ? SelectedCardSizeP3 : CardSizeP3;
        const FVector2D Size32 = SelectedLevel == TEXT("3-2") ? SelectedCardSizeP3 : CardSizeP3;
        AddLevelCard(TEXT("3-1"), bUnlocked31, Pos31, Size31);
        AddLevelCard(TEXT("3-2"), bUnlocked32, Pos32, Size32);
        SelectedPosition = SelectedLevel == TEXT("3-2") ? Pos32 : Pos31;
        CardSize = SelectedLevel == TEXT("3-2") ? Size32 : Size31;
        StartButtonY = 0.755f;
    }
    else
    {
        SelectedPosition = FVector2D(0.38f, 0.29f);
        CardSize = FVector2D(0.25f, 0.52f);
        AddLevelCard(TEXT("4-1"), bUnlocked41, SelectedPosition, CardSize);
        StartButtonY = 0.704f;
    }

    const float ButtonWidthFraction = 0.117f;
    const FVector2D StartButtonPosition(
        SelectedPosition.X + (CardSize.X - ButtonWidthFraction) * 0.5f, StartButtonY);
    StartLevelButton = AddButton(TEXT("开    始"), InkColor,
        StartButtonPosition, FVector2D(225.0f, 58.0f), 20);
    StartLevelButton->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnStartSelectedLevel);
    StartLevelButton->SetIsEnabled(SelectedLevel == TEXT("1-1") || SelectedLevel == TEXT("2-1")
        || (Progress && Progress->IsLevelUnlocked(SelectedLevel)));

    UButton* ReturnButton = AddButton(TEXT("返回主菜单"), BackgroundColor,
        FVector2D(0.025f, 0.905f), FVector2D(180.0f, 54.0f), 18);
    ReturnButton->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnReturnHomeClicked);

    UButton* PreviousButton = AddButton(TEXT("←  上一页"), BackgroundColor,
        FVector2D(0.35f, 0.88f), FVector2D(230.0f, 64.0f), 20);
    PreviousButton->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnPreviousPageClicked);
    PreviousButton->SetIsEnabled(CurrentLevelPage > 0);

    UButton* NextButton = AddButton(TEXT("下一页  →"), BackgroundColor,
        FVector2D(0.51f, 0.88f), FVector2D(230.0f, 64.0f), 20);
    NextButton->OnClicked.AddDynamic(this, &UNaiwaMenuWidget::OnNextPageClicked);
    NextButton->SetIsEnabled(CurrentLevelPage < 3);
}

void UNaiwaMenuWidget::GoToLevelPage(int32 PageIndex)
{
    CurrentLevelPage = FMath::Clamp(PageIndex, 0, 3);
    if (CurrentLevelPage == 0)
    {
        SelectedLevel = TEXT("1-1");
    }
    else if (CurrentLevelPage == 1)
    {
        UNaiwaProgressSubsystem* Progress = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UNaiwaProgressSubsystem>() : nullptr;
        SelectedLevel = Progress && Progress->IsLevelUnlocked(TEXT("2-2"))
            ? TEXT("2-2") : TEXT("2-1");
    }
    else if (CurrentLevelPage == 2)
    {
        SelectedLevel = TEXT("3-1");
    }
    else
    {
        SelectedLevel = TEXT("4-1");
    }
    BuildLevelSelectPage();
}

void UNaiwaMenuWidget::OnPreviousPageClicked()
{
    GoToLevelPage(CurrentLevelPage - 1);
}

void UNaiwaMenuWidget::OnNextPageClicked()
{
    GoToLevelPage(CurrentLevelPage + 1);
}

void UNaiwaMenuWidget::OnReturnHomeClicked()
{
    BuildHomePage();
}

void UNaiwaMenuWidget::SelectLevel(const FString& LevelId)
{
    UNaiwaProgressSubsystem* Progress = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UNaiwaProgressSubsystem>() : nullptr;
    if (!Progress || !Progress->IsLevelUnlocked(LevelId)) return;
    SelectedLevel = LevelId;
    CurrentLevelPage = LevelId.StartsWith(TEXT("1-")) ? 0
        : LevelId.StartsWith(TEXT("2-")) ? 1
        : LevelId.StartsWith(TEXT("3-")) ? 2 : 3;
    BuildLevelSelectPage();
}

void UNaiwaMenuWidget::OnStartClicked()
{
    SelectedLevel = TEXT("1-1");
    CurrentLevelPage = 0;
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
    else if (SelectedLevel == TEXT("3-1") || SelectedLevel == TEXT("3-2"))
    {
        const FString MapPath = SelectedLevel == TEXT("3-1")
            ? TEXT("/Game/Naiwa/P3/Maps/L_NaiwaP3_31")
            : TEXT("/Game/Naiwa/P3/Maps/L_NaiwaP3_32");
        UGameplayStatics::OpenLevel(this, FName(*MapPath), true, TEXT("NaiwaLinkRun=1"));
    }
    else if (SelectedLevel == TEXT("4-1"))
    {
        UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Naiwa/P5/Maps/L_NaiwaP5_M2Prototype")), true);
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
void UNaiwaMenuWidget::OnSelect31() { SelectLevel(TEXT("3-1")); }
void UNaiwaMenuWidget::OnSelect32() { SelectLevel(TEXT("3-2")); }
void UNaiwaMenuWidget::OnSelect41() { SelectLevel(TEXT("4-1")); }

void UNaiwaMenuWidget::OnWardrobeClicked()
{
    WardrobeFeedback.Empty();
    BuildWardrobePage();
}

void UNaiwaMenuWidget::OnWardrobeBackClicked()
{
    WardrobeFeedback.Empty();
    BuildHomePage();
}

void UNaiwaMenuWidget::PurchaseOrEquipP5Skin(const FName SkinId)
{
    UNaiwaProgressSubsystem* Progress = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UNaiwaProgressSubsystem>() : nullptr;
    if (!Progress)
    {
        WardrobeFeedback = TEXT("存档系统暂时不可用。");
        BuildWardrobePage();
        return;
    }

    if (!Progress->IsP5SkinOwned(SkinId) && !Progress->TryPurchaseP5Skin(SkinId))
    {
        WardrobeFeedback = TEXT("奶票不足，或皮肤配置不存在。");
        BuildWardrobePage();
        return;
    }

    if (Progress->SetSelectedP5Skin(SkinId))
    {
        WardrobeFeedback = TEXT("已保存并装备；下次开始 P5 时生效。");
    }
    else
    {
        WardrobeFeedback = TEXT("皮肤已购买，但当前无法装备。");
    }
    BuildWardrobePage();
}

void UNaiwaMenuWidget::OnWardrobeBaseClicked() { PurchaseOrEquipP5Skin(FName(TEXT("NaiwaSkin_Base"))); }
void UNaiwaMenuWidget::OnWardrobeNaiYuClicked() { PurchaseOrEquipP5Skin(FName(TEXT("NaiwaSkin_NaiYu"))); }
void UNaiwaMenuWidget::OnWardrobeNaiChaClicked() { PurchaseOrEquipP5Skin(FName(TEXT("NaiwaSkin_NaiCha"))); }
void UNaiwaMenuWidget::OnWardrobeNaiHaoClicked() { PurchaseOrEquipP5Skin(FName(TEXT("NaiwaSkin_NaiHao"))); }
void UNaiwaMenuWidget::OnWardrobeNaiShaClicked() { PurchaseOrEquipP5Skin(FName(TEXT("NaiwaSkin_NaiSha"))); }
