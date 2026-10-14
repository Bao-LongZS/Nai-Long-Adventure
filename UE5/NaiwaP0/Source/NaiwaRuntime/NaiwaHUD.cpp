#include "NaiwaHUD.h"
#include "NaiwaCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

void ANaiwaHUD::DrawCenteredLabel(const FString& Text, float CenterX, float Y, float Scale,
    const FLinearColor& Color)
{
    UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
    float Width = 0.0f;
    float Height = 0.0f;
    Canvas->StrLen(Font, Text, Width, Height);
    DrawText(Text, Color, CenterX - Width * Scale * 0.5f, Y, Font, Scale);
}

void ANaiwaHUD::DrawHUD()
{
    Super::DrawHUD();
    ANaiwaCharacter* Character = Cast<ANaiwaCharacter>(GetOwningPawn());
    if (Character)
    {
        // DrawHUD continues while paused, when the character's gameplay tick stops.
        Character->SampleInputEvidence();
    }
    if (!Canvas || !GEngine) return;

    const float Scale = FMath::Clamp(FMath::Min(Canvas->SizeX / 1280.0f, Canvas->SizeY / 720.0f), 0.65f, 2.0f);
    const float Margin = 20.0f * Scale;
    const float HelpWidth = FMath::Min(720.0f * Scale, Canvas->SizeX - 2.0f * Margin);
    DrawRect(FLinearColor(0.02f, 0.035f, 0.045f, 0.82f), Margin, Margin, HelpWidth, 100.0f * Scale);
    DrawText(TEXT("A / D or Left / Right   Move       Space   Jump"), FLinearColor::White,
        Margin + 14.0f * Scale, Margin + 10.0f * Scale, GEngine->GetMediumFont(), Scale);
    DrawText(TEXT("P / Esc   Pause / Continue         R   Reset to spawn"), FLinearColor(0.78f, 0.90f, 0.94f),
        Margin + 14.0f * Scale, Margin + 38.0f * Scale, GEngine->GetMediumFont(), Scale);
    const FString GaitHelp = FString::Printf(TEXT("Shift   Walk / Run       Mode: %s"),
        Character && !Character->IsRunningGait() ? TEXT("WALK") : TEXT("RUN"));
    DrawText(GaitHelp, FLinearColor(1.0f, 0.84f, 0.30f),
        Margin + 14.0f * Scale, Margin + 66.0f * Scale, GEngine->GetMediumFont(), Scale);

    if (!UGameplayStatics::IsGamePaused(this)) return;

    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.38f), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);
    const float PanelWidth = 520.0f * Scale;
    const float PanelHeight = 238.0f * Scale;
    const float CenterX = Canvas->SizeX * 0.5f;
    const float PanelY = (Canvas->SizeY - PanelHeight) * 0.5f;
    DrawRect(FLinearColor(0.025f, 0.075f, 0.09f, 0.96f), CenterX - PanelWidth * 0.5f, PanelY, PanelWidth, PanelHeight);
    DrawRect(FLinearColor(0.97f, 0.77f, 0.21f), CenterX - PanelWidth * 0.5f, PanelY, PanelWidth, 4.0f * Scale);
    DrawCenteredLabel(TEXT("PAUSED"), CenterX, PanelY + 32.0f * Scale, Scale * 1.5f, FLinearColor(1.0f, 0.84f, 0.30f));
    DrawCenteredLabel(TEXT("P / Esc   Continue"), CenterX, PanelY + 106.0f * Scale, Scale, FLinearColor::White);
    DrawCenteredLabel(TEXT("R   Reset to spawn"), CenterX, PanelY + 151.0f * Scale, Scale, FLinearColor::White);
}
