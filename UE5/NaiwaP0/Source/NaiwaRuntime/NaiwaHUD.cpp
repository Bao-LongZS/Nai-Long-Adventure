#include "NaiwaHUD.h"
#include "NaiwaCharacter.h"
#include "NaiwaP1GameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"

void ANaiwaHUD::BeginPlay()
{
    Super::BeginPlay();
    LoadDeathLaughFrames();
}

void ANaiwaHUD::LoadDeathLaughFrames()
{
    DeathLaughFrameTextures.Reset();
    DeathLaughFrameTextures.Reserve(ANaiwaP1GameMode::DeathLaughFrameCount);
    int32 LoadedFrames = 0;
    for (int32 Frame = 0; Frame < ANaiwaP1GameMode::DeathLaughFrameCount; ++Frame)
    {
        const FString FramePath = FString::Printf(
            TEXT("/Game/Naiwa/P1/Feedback/DeathLaughFrames/T_P1_DeathLaugh_%02d.T_P1_DeathLaugh_%02d"),
            Frame, Frame);
        UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *FramePath);
        DeathLaughFrameTextures.Add(Texture);
        LoadedFrames += Texture != nullptr ? 1 : 0;
    }
    UE_LOG(LogTemp, Display, TEXT("NAIWA_P1_DEATH_VIDEO preload frames=%d/%d legacy_fallback=disabled"),
        LoadedFrames, ANaiwaP1GameMode::DeathLaughFrameCount);
}

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

    const ANaiwaP1GameMode* P1Mode = Cast<ANaiwaP1GameMode>(UGameplayStatics::GetGameMode(this));

    const float Scale = FMath::Clamp(FMath::Min(Canvas->SizeX / 1280.0f, Canvas->SizeY / 720.0f), 0.65f, 2.0f);
    const float Margin = 20.0f * Scale;
    if (P1Mode)
    {
        const bool bExitHint = P1Mode->IsExitLockedHintVisible();
        const float PanelWidth = FMath::Min(640.0f * Scale, Canvas->SizeX - 2.0f * Margin);
        DrawRect(FLinearColor(0.02f, 0.035f, 0.045f, 0.82f), Margin, Margin,
            PanelWidth, (bExitHint ? 104.0f : 82.0f) * Scale);
        const FString Controls = FString::Printf(TEXT("A / D or Left / Right  Move    Space  Jump    Shift  Walk / Run: %s"),
            Character && !Character->IsRunningGait() ? TEXT("WALK") : TEXT("RUN"));
        DrawText(Controls, FLinearColor::White, Margin + 12.0f * Scale,
            Margin + 7.0f * Scale, GEngine->GetMediumFont(), Scale);
        const FString Progress = FString::Printf(TEXT("Pickup %d / %d    Checkpoint %s    Exit %s"),
            P1Mode->GetCollectedPickupCount(), P1Mode->GetTotalPickupCount(),
            P1Mode->HasCheckpoint() ? TEXT("ACTIVE") : TEXT("NONE"),
            P1Mode->IsGoalUnlocked() ? TEXT("READY") : TEXT("LOCKED"));
        DrawText(Progress, FLinearColor(0.95f, 0.95f, 0.76f), Margin + 12.0f * Scale,
            Margin + 31.0f * Scale, GEngine->GetMediumFont(), Scale);
        DrawText(TEXT("P / Esc  Pause / Continue    R  Restart level"), FLinearColor(0.78f, 0.90f, 0.94f),
            Margin + 12.0f * Scale, Margin + 55.0f * Scale, GEngine->GetMediumFont(), Scale);
        if (bExitHint)
        {
            DrawText(TEXT("Collect the pickup before entering the exit."), FLinearColor(1.0f, 0.72f, 0.40f),
                Margin + 12.0f * Scale, Margin + 79.0f * Scale, GEngine->GetMediumFont(), Scale * 0.85f);
        }
    }
    else
    {
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
    }

    const bool bPaused = UGameplayStatics::IsGamePaused(this);
    if (!bPaused && (!P1Mode || P1Mode->GetP1State() == ENaiwaP1State::Playing)) return;

    const bool bP1Dying = P1Mode && P1Mode->GetP1State() == ENaiwaP1State::Dying;
    if (!bP1Dying)
    {
        DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, P1Mode ? 0.30f : 0.38f),
            0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);
    }
    const float PanelWidth = (P1Mode ? 420.0f : 520.0f) * Scale;
    const bool bLaughSpriteActive = P1Mode && P1Mode->GetP1State() == ENaiwaP1State::Dying
        && P1Mode->IsDeathLaughSpriteActive();
    const float PanelHeight = (P1Mode
        ? (P1Mode->GetP1State() == ENaiwaP1State::Dying ? 580.0f : 170.0f)
        : 238.0f) * Scale;
    const float CenterX = Canvas->SizeX * 0.5f;
    const float PanelY = (Canvas->SizeY - PanelHeight) * 0.5f;
    if (!bLaughSpriteActive)
    {
        DrawRect(FLinearColor(0.025f, 0.075f, 0.09f, 0.96f),
            CenterX - PanelWidth * 0.5f, PanelY, PanelWidth, PanelHeight);
    }
    DrawRect(FLinearColor(0.97f, 0.77f, 0.21f), CenterX - PanelWidth * 0.5f, PanelY, PanelWidth, 4.0f * Scale);
    if (bPaused)
    {
        DrawCenteredLabel(TEXT("PAUSED"), CenterX, PanelY + (P1Mode ? 24.0f : 32.0f) * Scale,
            Scale * 1.5f, FLinearColor(1.0f, 0.84f, 0.30f));
        DrawCenteredLabel(TEXT("P / Esc   Continue"), CenterX,
            PanelY + (P1Mode ? 82.0f : 106.0f) * Scale, Scale, FLinearColor::White);
        DrawCenteredLabel(P1Mode ? TEXT("R   Restart level") : TEXT("R   Reset to spawn"),
            CenterX, PanelY + (P1Mode ? 119.0f : 151.0f) * Scale, Scale, FLinearColor::White);
    }
    else if (P1Mode)
    {
        const ENaiwaP1State State = P1Mode->GetP1State();
        DrawCenteredLabel(State == ENaiwaP1State::Dying ? TEXT("YOU DIED")
            : State == ENaiwaP1State::Respawning ? TEXT("RESPAWNING") : TEXT("LEVEL COMPLETE"),
            CenterX, PanelY + 36.0f * Scale, Scale * 1.4f,
            State == ENaiwaP1State::Won ? FLinearColor(0.45f, 1.0f, 0.60f) : FLinearColor(1.0f, 0.84f, 0.30f));
        if (State == ENaiwaP1State::Dying && bLaughSpriteActive)
        {
            const int32 Frame = P1Mode->GetDeathLaughSpriteFrame();
            UTexture2D* Texture = DeathLaughFrameTextures.IsValidIndex(Frame)
                ? DeathLaughFrameTextures[Frame].Get() : nullptr;
            if (Texture)
            {
                const float TextureWidth = FMath::Max(1.0f, static_cast<float>(Texture->GetSizeX()));
                const float TextureHeight = FMath::Max(1.0f, static_cast<float>(Texture->GetSizeY()));
                const float AspectRatio = TextureHeight / TextureWidth;
                const float MaxSpriteWidth = PanelWidth - 40.0f * Scale;
                const float MaxSpriteHeight = 445.0f * Scale;
                const float SpriteHeight = FMath::Min(MaxSpriteHeight, MaxSpriteWidth * AspectRatio);
                const float SpriteWidth = SpriteHeight / AspectRatio;
                // UCanvas::DrawTile uses the canvas's persistent DrawColor. Earlier HUD
                // elements can leave a translucent color there, which dims the sprite.
                Canvas->DrawColor = FColor::White;
                Canvas->DrawTile(Texture, CenterX - SpriteWidth * 0.5f, PanelY + 62.0f * Scale,
                    SpriteWidth, SpriteHeight, 0.0f, 0.0f,
                    static_cast<float>(Texture->GetSizeX()), static_cast<float>(Texture->GetSizeY()),
                    BLEND_Translucent);
            }
            DrawCenteredLabel(TEXT("Auto respawn after the laugh"), CenterX,
                PanelY + 520.0f * Scale, Scale, FLinearColor::White);
            DrawCenteredLabel(TEXT("R   Restart level"), CenterX,
                PanelY + 550.0f * Scale, Scale, FLinearColor::White);
        }
        else
        {
            DrawCenteredLabel(State == ENaiwaP1State::Dying ? TEXT("Auto respawn...")
                : State == ENaiwaP1State::Respawning
                    ? (P1Mode->HasCheckpoint() ? TEXT("Returning to checkpoint") : TEXT("Returning to start"))
                    : TEXT("Exit reached"),
                CenterX, PanelY + 91.0f * Scale, Scale, FLinearColor::White);
            DrawCenteredLabel(State == ENaiwaP1State::Won ? TEXT("R   Play again") : TEXT("R   Restart level"),
                CenterX, PanelY + 124.0f * Scale, Scale, FLinearColor::White);
        }
    }
}
