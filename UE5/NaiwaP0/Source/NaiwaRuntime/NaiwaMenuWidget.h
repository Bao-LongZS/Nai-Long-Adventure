#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "NaiwaMenuWidget.generated.h"

class UButton;
class UCanvasPanel;
class UTexture2D;

UCLASS()
class NAIWARUNTIME_API UNaiwaMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;

    void ShowLevelSelect();

private:
    void BuildHomePage();
    void BuildLevelSelectPage();
    void AddBackground();
    UButton* AddButton(const FString& Label, const FLinearColor& Color,
        const FVector2D& Position, const FVector2D& Size, int32 FontSize = 22);
    void AddText(const FString& Label, const FVector2D& Position, const FVector2D& Size,
        int32 FontSize, const FLinearColor& Color, bool bBold = false,
        ETextJustify::Type Justification = ETextJustify::Left);
    void AddLevelCard(const FString& LevelId, const FString& Title, const FString& Subtitle,
        bool bUnlocked, float Left, float Right);

    UFUNCTION() void OnStartClicked();
    UFUNCTION() void OnExitClicked();
    UFUNCTION() void OnStartSelectedLevel();
    UFUNCTION() void OnSelect11();
    UFUNCTION() void OnSelect21();
    UFUNCTION() void OnSelect22();
    UFUNCTION() void OnSelect23();
    void SelectLevel(const FString& LevelId);

    UPROPERTY(Transient)
    TObjectPtr<UCanvasPanel> RootCanvas;

    UPROPERTY(Transient)
    TObjectPtr<UButton> StartLevelButton;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> StartCharacterTexture;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> SelectCharacterTexture;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> Level11Thumbnail;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> Level21Thumbnail;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> Level22Thumbnail;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> Level23Thumbnail;

    FString SelectedLevel = TEXT("1-1");
};
