#include "NaiwaP2HUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "NaiwaP2Ballistics.h"
#include "NaiwaP2GameMode.h"
#include "NaiwaP2PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    void DrawFeedbackParticles(UCanvas* Canvas, const float Scale, const double AgeSeconds,
        const int32 CueSerial)
    {
        constexpr float LifetimeSeconds = 1.35f;
        if (!Canvas || AgeSeconds < 0.0 || AgeSeconds >= LifetimeSeconds)
        {
            return;
        }

        static const FLinearColor Colors[] = {
            FLinearColor(1.0f, 0.84f, 0.32f),
            FLinearColor(1.0f, 0.48f, 0.25f),
            FLinearColor(0.48f, 0.90f, 0.82f),
            FLinearColor(0.98f, 0.98f, 0.88f),
            FLinearColor(0.78f, 0.62f, 1.0f)
        };

        const float Age = static_cast<float>(AgeSeconds);
        const float Fade = FMath::Square(1.0f - Age / LifetimeSeconds);
        const FVector2D Center(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f);
        constexpr int32 ParticleCount = 48;
        for (int32 Index = 0; Index < ParticleCount; ++Index)
        {
            FRandomStream Random(CueSerial * 104729 + Index * 8191 + 37);
            const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
            const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
            const FVector2D Perpendicular(-Direction.Y, Direction.X);
            const float StartRadius = Random.FRandRange(24.0f, 88.0f) * Scale;
            const float Speed = Random.FRandRange(155.0f, 390.0f) * Scale;
            const float Length = Random.FRandRange(4.0f, 11.0f) * Scale;
            const FVector2D Position = Center + Direction * (StartRadius + Speed * Age) +
                FVector2D(0.0f, 56.0f * Age * Age * Scale);
            FLinearColor Color = Colors[Index % UE_ARRAY_COUNT(Colors)];
            Color.A = Fade * Random.FRandRange(0.62f, 0.96f);
            Canvas->K2_DrawLine(Position - Direction * Length * 0.55f,
                Position + Direction * Length * 0.55f, FMath::Max(1.2f, 2.4f * Scale), Color);
            Canvas->K2_DrawLine(Position - Perpendicular * Length * 0.22f,
                Position + Perpendicular * Length * 0.22f, FMath::Max(1.0f, 1.5f * Scale), Color);
        }
    }

    FVector TrajectoryWorldPoint(const NaiwaP2::FTrajectoryState& State)
    {
        return FVector(State.X * 100.0, 0.0, State.Z * 100.0);
    }
}

ANaiwaP2HUD::ANaiwaP2HUD()
{
    static ConstructorHelpers::FObjectFinder<UTexture2D> OneMinuteImage(
        TEXT("/Game/Naiwa/P2/UI/T_P2_Failure_OneMinute.T_P2_Failure_OneMinute"));
    if (OneMinuteImage.Succeeded()) OneMinuteFeedbackImage = OneMinuteImage.Object;

    static ConstructorHelpers::FObjectFinder<UTexture2D> FifthResetImage(
        TEXT("/Game/Naiwa/P2/UI/T_P2_Failure_Resets.T_P2_Failure_Resets"));
    if (FifthResetImage.Succeeded()) FifthResetFeedbackImage = FifthResetImage.Object;

    static ConstructorHelpers::FObjectFinder<UTexture2D> OutOfEggsImage(
        TEXT("/Game/Naiwa/P2/UI/T_P2_Failure_Exhausted.T_P2_Failure_Exhausted"));
    if (OutOfEggsImage.Succeeded()) OutOfEggsFeedbackImage = OutOfEggsImage.Object;
}

void ANaiwaP2HUD::DrawCenteredLabel(const FString& Text, const float CenterX, const float Y,
    const float Scale, const FLinearColor& Color)
{
    UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
    float Width = 0.0f;
    float Height = 0.0f;
    Canvas->StrLen(Font, Text, Width, Height);
    DrawText(Text, Color, CenterX - Width * Scale * 0.5f, Y, Font, Scale);
}

void ANaiwaP2HUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !GEngine)
    {
        return;
    }

    const ANaiwaP2GameMode* Mode = Cast<ANaiwaP2GameMode>(UGameplayStatics::GetGameMode(this));
    if (!Mode)
    {
        return;
    }

    const float Scale = FMath::Clamp(FMath::Min(Canvas->SizeX / 1280.0f, Canvas->SizeY / 720.0f), 0.7f, 1.6f);
    const float Margin = 18.0f * Scale;
    const float PanelWidth = FMath::Min(760.0f * Scale, Canvas->SizeX - 2.0f * Margin);
    const bool bLevel22 = Mode->GetLevelId() == TEXT("2-2");
    DrawRect(FLinearColor(0.015f, 0.035f, 0.045f, 0.86f), Margin, Margin,
        PanelWidth, (bLevel22 ? 190.0f : 158.0f) * Scale);

    DrawText(bLevel22 ? TEXT("2-2  EGG TRIAL") : TEXT("2-1  SLINGSHOT TEST"), FLinearColor(1.0f, 0.82f, 0.30f),
        Margin + 12.0f * Scale, Margin + 8.0f * Scale, GEngine->GetMediumFont(), Scale * 1.1f);
    const FString Status = FString::Printf(TEXT("%s    SHOTS %d / %d    TOKENS %d    TARGETS %d"),
        *Mode->GetPlayStateLabel(), Mode->GetShotsRemaining(), Mode->GetTotalShots(),
        Mode->GetEggTokensRemaining(), Mode->GetRemainingGoalCount());
    DrawText(Status, FLinearColor::White, Margin + 12.0f * Scale,
        Margin + 34.0f * Scale, GEngine->GetMediumFont(), Scale);

    APlayerController* PC = GetOwningPlayerController();
    const ANaiwaP2PlayerController* P2PC = Cast<ANaiwaP2PlayerController>(PC);
    FString ControlHint = bLevel22
        ? TEXT("1-5 SELECT AN UNUSED EGG  |  HOLD LEFT ON EGG, PULL, RELEASE TO FIRE")
        : TEXT("HOLD LEFT BUTTON ON THE EGG  >  PULL LEFT + DOWN  >  RELEASE TO FIRE");
    FLinearColor ControlColor(0.74f, 0.89f, 0.93f);
    if (bLevel22 && Mode->GetPlayState() == ENaiwaP2PlayState::Flying)
    {
        if (Mode->GetCurrentShotEgg() == NaiwaP2::EEggKind::Chick)
        {
            ControlHint = TEXT("CHICK: CLICK LEFT BUTTON ONCE WHILE FLYING TO FLAP UP");
        }
        else if (Mode->GetCurrentShotEgg() == NaiwaP2::EEggKind::Mouse)
        {
            ControlHint = TEXT("MOUSE: CLICK LEFT BUTTON ONCE WHILE FLYING TO SPLIT");
        }
        else
        {
            ControlHint = FString::Printf(TEXT("IN FLIGHT  |  %s"),
                NaiwaP2::GetEggKindLabel(Mode->GetCurrentShotEgg()));
        }
    }
    if (P2PC && P2PC->IsAimDragging())
    {
        if (P2PC->HasValidAimDrag())
        {
            ControlHint = FString::Printf(TEXT("KEEP HOLDING  |  POWER %.0f%%  |  AIM %.0f DEG  |  RELEASE TO FIRE"),
                P2PC->GetDragPower() * 100.0, P2PC->GetDragAngleDegrees());
            ControlColor = FLinearColor(1.0f, 0.88f, 0.35f);
        }
        else
        {
            ControlHint = TEXT("KEEP HOLDING  |  PULL THE EGG LEFT + DOWN TO BUILD POWER");
            ControlColor = FLinearColor(1.0f, 0.58f, 0.34f);
        }
    }
    DrawText(ControlHint, ControlColor, Margin + 12.0f * Scale,
        Margin + 62.0f * Scale, GEngine->GetMediumFont(), Scale * 0.84f);
    DrawText(TEXT("R  RESETS THE LEVEL    ESC  LEVEL SELECT"), FLinearColor(0.62f, 0.78f, 0.82f),
        Margin + 12.0f * Scale, Margin + 94.0f * Scale,
        GEngine->GetMediumFont(), Scale * 0.78f);
    if (bLevel22)
    {
        const FString EggSlots = FString::Printf(
            TEXT("1 BASE   2 CHICK   3 MOUSE   4 SOY   5 TEA    SELECTED: %s"),
            NaiwaP2::GetEggKindLabel(Mode->GetSelectedEgg()));
        DrawText(EggSlots, FLinearColor(1.0f, 0.84f, 0.42f), Margin + 12.0f * Scale,
            Margin + 120.0f * Scale, GEngine->GetMediumFont(), Scale * 0.70f);
    }
    DrawText(TEXT("SOLID: PATH TO IMPACT    DASHED: PREDICTED TO LANDING"),
        FLinearColor(0.62f, 0.82f, 0.84f), Margin + 12.0f * Scale,
        Margin + (bLevel22 ? 152.0f : 120.0f) * Scale, GEngine->GetMediumFont(), Scale * 0.70f);

    const NaiwaP2::FShotResult* Preview = Mode->GetAimPreview();
    if (Preview && PC && Preview->States.Num() > 1 && Mode->GetPlayState() == ENaiwaP2PlayState::Aiming)
    {
        FVector2D PreviousScreen;
        bool bPreviousVisible = PC->ProjectWorldLocationToScreen(TrajectoryWorldPoint(Preview->States[0]), PreviousScreen);
        for (int32 Index = 12; Index < Preview->States.Num(); Index += 12)
        {
            FVector2D Screen;
            const bool bVisible = PC->ProjectWorldLocationToScreen(TrajectoryWorldPoint(Preview->States[Index]), Screen);
            if (bPreviousVisible && bVisible)
            {
                Canvas->K2_DrawLine(PreviousScreen, Screen, 3.0f, FLinearColor(0.88f, 0.94f, 0.52f, 0.92f));
            }
            PreviousScreen = Screen;
            bPreviousVisible = bVisible;
        }

        const NaiwaP2::FShotState Launch = NaiwaP2::FBallistics::MakeLaunchState(
            Mode->GetAimAngleDegrees(), Mode->GetAimPower());
        const NaiwaP2::FTrajectoryResult FullTrace = NaiwaP2::FBallistics::TraceEmpty(Launch);
        if (FullTrace.bCompleted && FullTrace.States.Num() > 1)
        {
            const int32 ImpactTick = FMath::Clamp(Preview->States.Last().Tick,
                0, FullTrace.States.Num() - 1);
            const FLinearColor PredictedColor(0.48f, 0.86f, 0.94f, 0.58f);
            constexpr int32 DashLength = 8;
            constexpr int32 DashPeriod = 13;
            for (int32 StartIndex = ImpactTick; StartIndex < FullTrace.States.Num() - 1;
                 StartIndex += DashPeriod)
            {
                const int32 EndIndex = FMath::Min(StartIndex + DashLength, FullTrace.States.Num() - 1);
                FVector2D StartScreen;
                FVector2D EndScreen;
                if (PC->ProjectWorldLocationToScreen(TrajectoryWorldPoint(FullTrace.States[StartIndex]), StartScreen) &&
                    PC->ProjectWorldLocationToScreen(TrajectoryWorldPoint(FullTrace.States[EndIndex]), EndScreen))
                {
                    Canvas->K2_DrawLine(StartScreen, EndScreen, 2.2f * Scale, PredictedColor);
                }
            }

            FVector2D LandingScreen;
            if (PC->ProjectWorldLocationToScreen(TrajectoryWorldPoint(FullTrace.States.Last()), LandingScreen))
            {
                constexpr float LandingMarkRadius = 8.0f;
                Canvas->K2_DrawLine(LandingScreen + FVector2D(-LandingMarkRadius, 0.0f),
                    LandingScreen + FVector2D(LandingMarkRadius, 0.0f), 2.0f * Scale, PredictedColor);
                Canvas->K2_DrawLine(LandingScreen + FVector2D(0.0f, -LandingMarkRadius),
                    LandingScreen + FVector2D(0.0f, LandingMarkRadius), 2.0f * Scale, PredictedColor);
            }
        }
    }

    if (PC && P2PC)
    {
        const FVector2D PouchWorld2D(P2PC->IsAimDragging()
                ? P2PC->GetDraggedPouchWorldLocation().X : 0.0,
            P2PC->IsAimDragging()
                ? P2PC->GetDraggedPouchWorldLocation().Z : 180.0);
        const FVector PouchWorld(PouchWorld2D.X, 0.0, PouchWorld2D.Y);
        FVector2D UpperForkScreen;
        FVector2D LowerForkScreen;
        FVector2D PouchScreen;
        const bool bUpperVisible = PC->ProjectWorldLocationToScreen(FVector(145.0, 0.0, 238.0), UpperForkScreen);
        const bool bLowerVisible = PC->ProjectWorldLocationToScreen(FVector(145.0, 0.0, 88.0), LowerForkScreen);
        const bool bPouchVisible = PC->ProjectWorldLocationToScreen(PouchWorld, PouchScreen);
        const FLinearColor BandColor = P2PC->HasValidAimDrag()
            ? FLinearColor(1.0f, 0.66f, 0.18f, 0.98f)
            : FLinearColor(0.72f, 0.90f, 0.32f, 0.92f);
        if (bPouchVisible && bUpperVisible)
        {
            Canvas->K2_DrawLine(UpperForkScreen, PouchScreen, P2PC->IsAimDragging() ? 5.0f : 3.5f, BandColor);
        }
        if (bPouchVisible && bLowerVisible)
        {
            Canvas->K2_DrawLine(LowerForkScreen, PouchScreen, P2PC->IsAimDragging() ? 5.0f : 3.5f, BandColor);
        }
        if (bPouchVisible)
        {
            const float RingRadius = P2PC->IsAimDragging() ? 34.0f * Scale : 72.0f * Scale;
            const FLinearColor RingColor = P2PC->IsAimDragging()
                ? FLinearColor(1.0f, 0.75f, 0.18f, 0.90f)
                : FLinearColor(0.75f, 1.0f, 0.35f, 0.78f);
            constexpr int32 RingSegments = 40;
            FVector2D Previous = PouchScreen + FVector2D(RingRadius, 0.0f);
            for (int32 Index = 1; Index <= RingSegments; ++Index)
            {
                const float Angle = 2.0f * PI * static_cast<float>(Index) / RingSegments;
                const FVector2D Current = PouchScreen + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RingRadius;
                Canvas->K2_DrawLine(Previous, Current, 2.5f * Scale, RingColor);
                Previous = Current;
            }
            if (!P2PC->IsAimDragging() && Mode->GetPlayState() == ENaiwaP2PlayState::Aiming)
            {
                DrawCenteredLabel(TEXT("HOLD + DRAG EGG"), PouchScreen.X,
                    PouchScreen.Y - RingRadius - 18.0f * Scale, Scale * 0.86f, FLinearColor::White);
                const FVector2D ArrowTip = PouchScreen + FVector2D(-48.0f * Scale, 42.0f * Scale);
                Canvas->K2_DrawLine(PouchScreen, ArrowTip, 3.0f * Scale, RingColor);
                Canvas->K2_DrawLine(ArrowTip, ArrowTip + FVector2D(16.0f * Scale, -1.0f * Scale),
                    3.0f * Scale, RingColor);
                Canvas->K2_DrawLine(ArrowTip, ArrowTip + FVector2D(1.0f * Scale, -16.0f * Scale),
                    3.0f * Scale, RingColor);
            }
        }
    }

    if (const NaiwaP2::FShotResult* Shot = Mode->GetCurrentShot();
        Shot && PC && Shot->States.Num() > 1 &&
        (Mode->GetPlayState() == ENaiwaP2PlayState::Flying || Mode->GetPlayState() == ENaiwaP2PlayState::Resolving))
    {
        FVector2D PreviousScreen;
        bool bPreviousVisible = PC->ProjectWorldLocationToScreen(TrajectoryWorldPoint(Shot->States[0]), PreviousScreen);
        for (int32 Index = 12; Index < Shot->States.Num(); Index += 12)
        {
            FVector2D Screen;
            const bool bVisible = PC->ProjectWorldLocationToScreen(TrajectoryWorldPoint(Shot->States[Index]), Screen);
            if (bPreviousVisible && bVisible)
            {
                Canvas->K2_DrawLine(PreviousScreen, Screen, 4.5f, FLinearColor(1.0f, 0.78f, 0.18f, 0.94f));
            }
            PreviousScreen = Screen;
            bPreviousVisible = bVisible;
        }

        const FLinearColor SplitColors[] = {
            FLinearColor(0.36f, 0.88f, 1.0f, 0.94f),
            FLinearColor(1.0f, 0.72f, 0.28f, 0.94f),
            FLinearColor(0.86f, 0.62f, 1.0f, 0.94f)
        };
        for (int32 PathIndex = 0; PathIndex < Shot->SplitPaths.Num(); ++PathIndex)
        {
            const NaiwaP2::FProjectilePath& Path = Shot->SplitPaths[PathIndex];
            if (Path.States.Num() < 2)
            {
                continue;
            }
            FVector2D PathPreviousScreen;
            bool bPathPreviousVisible = PC->ProjectWorldLocationToScreen(
                TrajectoryWorldPoint(Path.States[0]), PathPreviousScreen);
            const FLinearColor& PathColor = SplitColors[PathIndex % UE_ARRAY_COUNT(SplitColors)];
            for (int32 Index = 8; Index < Path.States.Num(); Index += 8)
            {
                FVector2D PathScreen;
                const bool bPathVisible = PC->ProjectWorldLocationToScreen(
                    TrajectoryWorldPoint(Path.States[Index]), PathScreen);
                if (bPathPreviousVisible && bPathVisible)
                {
                    Canvas->K2_DrawLine(PathPreviousScreen, PathScreen, 3.4f, PathColor);
                }
                PathPreviousScreen = PathScreen;
                bPathPreviousVisible = bPathVisible;
            }
        }
    }

    if (!Mode->GetActionNotice().IsEmpty() && Mode->GetActionNoticeRemaining() > 0.0)
    {
        const float CenterX = Canvas->SizeX * 0.5f;
        const float NoticeY = Canvas->SizeY * 0.22f;
        const float NoticeWidth = FMath::Min(700.0f * Scale, Canvas->SizeX - 2.0f * Margin);
        DrawRect(FLinearColor(0.015f, 0.035f, 0.045f, 0.90f),
            CenterX - NoticeWidth * 0.5f, NoticeY - 26.0f * Scale,
            NoticeWidth, 58.0f * Scale);
        DrawCenteredLabel(Mode->GetActionNotice(), CenterX, NoticeY - 10.0f * Scale,
            Scale * 1.02f, FLinearColor(1.0f, 0.86f, 0.32f));
    }

    if (Mode->GetPlayState() == ENaiwaP2PlayState::Failed || Mode->GetPlayState() == ENaiwaP2PlayState::Won)
    {
        const float CenterX = Canvas->SizeX * 0.5f;
        const float CenterY = Canvas->SizeY * 0.5f;
        DrawRect(FLinearColor(0.01f, 0.04f, 0.05f, 0.92f),
            CenterX - 250.0f * Scale, CenterY - 75.0f * Scale,
            500.0f * Scale, 150.0f * Scale);
        DrawCenteredLabel(Mode->GetPlayState() == ENaiwaP2PlayState::Failed ? TEXT("OUT OF EGGS") : TEXT("ALL TARGETS CLEARED"),
            CenterX, CenterY - 45.0f * Scale, Scale * 1.25f,
            Mode->GetPlayState() == ENaiwaP2PlayState::Won ? FLinearColor(0.4f, 1.0f, 0.55f) : FLinearColor(1.0f, 0.65f, 0.35f));
        DrawCenteredLabel(TEXT("Press R to reset level"), CenterX, CenterY + 8.0f * Scale,
            Scale, FLinearColor::White);
    }

    if (Mode->ShouldShowFeedbackImage())
    {
        UTexture2D* FeedbackTexture = nullptr;
        switch (Mode->GetFeedbackImage())
        {
        case ENaiwaP2FeedbackImage::OneMinute:
            FeedbackTexture = OneMinuteFeedbackImage;
            break;
        case ENaiwaP2FeedbackImage::FifthReset:
            FeedbackTexture = FifthResetFeedbackImage;
            break;
        case ENaiwaP2FeedbackImage::OutOfEggs:
            FeedbackTexture = OutOfEggsFeedbackImage;
            break;
        default:
            break;
        }

        if (FeedbackTexture)
        {
            DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.30f), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);
            const float TextureWidth = static_cast<float>(FeedbackTexture->GetSizeX());
            const float TextureHeight = static_cast<float>(FeedbackTexture->GetSizeY());
            if (TextureWidth > 0.0f && TextureHeight > 0.0f)
            {
                const float MaxWidth = Canvas->SizeX * 0.66f;
                const float MaxHeight = Canvas->SizeY * 0.80f;
                const float FitScale = FMath::Min(MaxWidth / TextureWidth, MaxHeight / TextureHeight);
                const FVector2D DrawSize(TextureWidth * FitScale, TextureHeight * FitScale);
                const FVector2D DrawPosition((Canvas->SizeX - DrawSize.X) * 0.5f,
                    (Canvas->SizeY - DrawSize.Y) * 0.5f);
                Canvas->K2_DrawTexture(FeedbackTexture, DrawPosition, DrawSize,
                    FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::White,
                    BLEND_Translucent, 0.0f, FVector2D::ZeroVector);
                DrawFeedbackParticles(Canvas, Scale, Mode->GetFeedbackCueAgeSeconds(),
                    Mode->GetFeedbackCueSerial());
            }
        }
    }
}
