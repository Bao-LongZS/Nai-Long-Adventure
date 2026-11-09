#include "Presentation/NaiwaP7GrayboxHUD.h"

#include "Core/NaiwaP7Board.h"
#include "Core/NaiwaP7Simulation.h"
#include "Core/NaiwaP7TowerDefenseSubsystem.h"
#include "Presentation/NaiwaP7GrayboxPlayerController.h"

namespace
{
    const FLinearColor BoardBackground(0.035f, 0.050f, 0.065f, 1.0f);
    const FLinearColor GridLineColor(0.22f, 0.27f, 0.31f, 0.8f);
    const FLinearColor TextColor(0.90f, 0.94f, 0.96f, 1.0f);
    const FLinearColor MutedTextColor(0.62f, 0.70f, 0.75f, 1.0f);
    const FLinearColor RouteAColor(0.18f, 0.78f, 0.92f, 1.0f);
    const FLinearColor RouteBColor(1.0f, 0.67f, 0.20f, 1.0f);
    const FLinearColor BuildSiteColor(0.35f, 0.92f, 0.62f, 1.0f);
    const FLinearColor TowerColor(0.23f, 0.52f, 0.95f, 1.0f);
    const FLinearColor EnemyColor(0.90f, 0.32f, 0.62f, 1.0f);
    const FLinearColor ObstacleColor(0.76f, 0.47f, 0.25f, 1.0f);
    const FLinearColor InterferenceColor(1.0f, 0.28f, 0.20f, 1.0f);

    FVector2D CellCenterOnScreen(
        const FIntPoint& Cell,
        const FVector2D& BoardOrigin,
        const float CellWidth,
        const float CellHeight)
    {
        return BoardOrigin + FVector2D(
            (static_cast<float>(Cell.X) + 0.5f) * CellWidth,
            (static_cast<float>(Cell.Y) + 0.5f) * CellHeight);
    }

    FVector2D GridPositionOnScreen(
        const FVector2D& GridPosition,
        const FVector2D& BoardOrigin,
        const float CellWidth,
        const float CellHeight)
    {
        return BoardOrigin + FVector2D(
            (GridPosition.X + 0.5f) * CellWidth,
            (GridPosition.Y + 0.5f) * CellHeight);
    }

    void DrawOutline(
        AHUD* Hud,
        const FVector2D& Origin,
        const FVector2D& Size,
        const FLinearColor& Color,
        const float Thickness)
    {
        Hud->DrawLine(Origin.X, Origin.Y, Origin.X + Size.X, Origin.Y, Color, Thickness);
        Hud->DrawLine(Origin.X + Size.X, Origin.Y, Origin.X + Size.X, Origin.Y + Size.Y, Color, Thickness);
        Hud->DrawLine(Origin.X + Size.X, Origin.Y + Size.Y, Origin.X, Origin.Y + Size.Y, Color, Thickness);
        Hud->DrawLine(Origin.X, Origin.Y + Size.Y, Origin.X, Origin.Y, Color, Thickness);
    }

    const TCHAR* GetPhaseLabel(const NaiwaP7::ESimulationPhase Phase)
    {
        using NaiwaP7::ESimulationPhase;
        switch (Phase)
        {
        case ESimulationPhase::NoLevel: return TEXT("No level");
        case ESimulationPhase::BetweenWaves: return TEXT("Prepare");
        case ESimulationPhase::WaveActive: return TEXT("Wave active");
        case ESimulationPhase::Won: return TEXT("Won");
        case ESimulationPhase::Lost: return TEXT("Lost");
        default: return TEXT("Unknown");
        }
    }
}

bool ANaiwaP7GrayboxHUD::TryCalculateBoardRect(
    const FVector2D& ViewportSize,
    FVector2D& OutOrigin,
    FVector2D& OutSize)
{
    constexpr double ReferenceViewportWidth = NaiwaP7::ReferenceCellWidth * NaiwaP7::BoardColumns;
    constexpr double ReferenceViewportHeight = 1080.0;
    constexpr double ReferenceBoardHeight = NaiwaP7::ReferenceCellHeight * NaiwaP7::BoardRows;

    if (!FMath::IsFinite(ViewportSize.X) || !FMath::IsFinite(ViewportSize.Y) ||
        ViewportSize.X <= 0.0 || ViewportSize.Y <= 0.0)
    {
        return false;
    }

    const double Scale = FMath::Min(
        ViewportSize.X / ReferenceViewportWidth,
        ViewportSize.Y / ReferenceViewportHeight);
    const FVector2D CandidateSize(
        ReferenceViewportWidth * Scale,
        ReferenceBoardHeight * Scale);
    const FVector2D CandidateOrigin(
        (ViewportSize.X - CandidateSize.X) * 0.5,
        (ViewportSize.Y - CandidateSize.Y) * 0.5);

    OutOrigin = CandidateOrigin;
    OutSize = CandidateSize;
    return true;
}

void ANaiwaP7GrayboxHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !PlayerOwner || !GetWorld())
    {
        return;
    }

    int32 ViewportWidth = 0;
    int32 ViewportHeight = 0;
    PlayerOwner->GetViewportSize(ViewportWidth, ViewportHeight);

    const FVector2D ViewportSize(static_cast<float>(ViewportWidth), static_cast<float>(ViewportHeight));
    FVector2D BoardOrigin;
    FVector2D BoardSize;
    if (!TryCalculateBoardRect(ViewportSize, BoardOrigin, BoardSize))
    {
        return;
    }

    const double LayoutScale = BoardSize.X /
        (NaiwaP7::ReferenceCellWidth * NaiwaP7::BoardColumns);
    const float CellWidth = BoardSize.X / NaiwaP7::BoardColumns;
    const float CellHeight = BoardSize.Y / NaiwaP7::BoardRows;
    const float TextScale = static_cast<float>(FMath::Clamp(LayoutScale, 0.55, 1.5));
    const float Margin = 24.0f * TextScale;
    const float TopPanelBottom = FMath::Max(0.0f, BoardOrigin.Y - 16.0f * TextScale);
    const float BottomPanelTop = BoardOrigin.Y + BoardSize.Y + 16.0f * TextScale;

    DrawRect(FLinearColor(0.025f, 0.035f, 0.047f, 0.98f), 0.0f, 0.0f, ViewportSize.X, TopPanelBottom);
    DrawRect(FLinearColor(0.025f, 0.035f, 0.047f, 0.98f), 0.0f, BottomPanelTop,
        ViewportSize.X, FMath::Max(0.0f, ViewportSize.Y - BottomPanelTop));

    UNaiwaP7TowerDefenseSubsystem* P7Subsystem = GetWorld()->GetSubsystem<UNaiwaP7TowerDefenseSubsystem>();
    if (!P7Subsystem || !P7Subsystem->GetSimulation().HasLevel())
    {
        DrawText(TEXT("P7 GRAYBOX"), TextColor, Margin, Margin, nullptr, TextScale);
        DrawText(TEXT("Waiting for a validated P7 level fixture."), MutedTextColor,
            Margin, Margin + 32.0f * TextScale, nullptr, TextScale);
        return;
    }

    const NaiwaP7::FSimulationSnapshot Snapshot = P7Subsystem->GetSnapshot();
    const NaiwaP7::FLevelDefinition& Level = P7Subsystem->GetSimulation().GetLevelDefinition();
    const ANaiwaP7GrayboxPlayerController* P7Controller =
        Cast<ANaiwaP7GrayboxPlayerController>(PlayerOwner);

    const FString RouteLabel = Snapshot.ActiveRoute == NaiwaP7::ERoute::A ? TEXT("Route A") : TEXT("Route B");
    const FString WaveLabel = FString::Printf(TEXT("Wave %d / %d"), Snapshot.CurrentWave, Snapshot.TotalWaves);
    const FString EconomyLabel = FString::Printf(TEXT("Coins %d"), Snapshot.Coins);
    const FString PhaseLabel = GetPhaseLabel(Snapshot.Phase);
    DrawText(TEXT("P7 GRAYBOX"), TextColor, Margin, Margin, nullptr, TextScale);
    DrawText(WaveLabel, TextColor, Margin + 205.0f * TextScale, Margin, nullptr, TextScale);
    DrawText(RouteLabel, Snapshot.ActiveRoute == NaiwaP7::ERoute::A ? RouteAColor : RouteBColor,
        Margin + 390.0f * TextScale, Margin, nullptr, TextScale);
    DrawText(EconomyLabel, TextColor, Margin + 535.0f * TextScale, Margin, nullptr, TextScale);
    DrawText(PhaseLabel, MutedTextColor, Margin + 700.0f * TextScale, Margin, nullptr, TextScale);
    if (P7Controller && !P7Controller->GetSelectedTowerId().IsNone())
    {
        DrawText(FString::Printf(TEXT("Build: %s"), *P7Controller->GetSelectedTowerId().ToString()),
            BuildSiteColor, Margin + 875.0f * TextScale, Margin, nullptr, TextScale * 0.85f);
    }

    int32 TowerSlot = 0;
    for (const FNaiwaP7TowerRow& TowerRow : P7Subsystem->GetSimulation().GetTowerRows())
    {
        if (TowerRow.Level != 1)
        {
            continue;
        }
        const FString TowerChoice = FString::Printf(TEXT("%d: %s  %d coins"),
            TowerSlot + 1, *TowerRow.TowerId.ToString(), TowerRow.CostCoins);
        DrawText(TowerChoice, MutedTextColor, Margin + TowerSlot * 260.0f * TextScale,
            Margin + 76.0f * TextScale, nullptr, TextScale * 0.78f);
        ++TowerSlot;
        if (TowerSlot == 3)
        {
            break;
        }
    }

    const float JarBarX = Margin;
    const float JarBarY = Margin + 42.0f * TextScale;
    const float JarBarWidth = 210.0f * TextScale;
    const float JarBarHeight = 12.0f * TextScale;
    DrawText(FString::Printf(TEXT("Milk jar %d / %d"), Snapshot.MilkJarHealth,
        NaiwaP7::FSimulation::StartingMilkJarHealth), MutedTextColor,
        JarBarX, JarBarY - 20.0f * TextScale, nullptr, TextScale * 0.8f);
    DrawRect(FLinearColor(0.16f, 0.19f, 0.21f, 1.0f), JarBarX, JarBarY, JarBarWidth, JarBarHeight);
    const float JarFraction = static_cast<float>(Snapshot.MilkJarHealth) /
        NaiwaP7::FSimulation::StartingMilkJarHealth;
    DrawRect(JarFraction > 0.3f ? BuildSiteColor : InterferenceColor,
        JarBarX, JarBarY, JarBarWidth * FMath::Clamp(JarFraction, 0.0f, 1.0f), JarBarHeight);

    DrawRect(BoardBackground, BoardOrigin.X, BoardOrigin.Y, BoardSize.X, BoardSize.Y);
    for (int32 Column = 0; Column <= NaiwaP7::BoardColumns; ++Column)
    {
        const float X = BoardOrigin.X + Column * CellWidth;
        DrawLine(X, BoardOrigin.Y, X, BoardOrigin.Y + BoardSize.Y, GridLineColor, 1.0f * TextScale);
    }
    for (int32 Row = 0; Row <= NaiwaP7::BoardRows; ++Row)
    {
        const float Y = BoardOrigin.Y + Row * CellHeight;
        DrawLine(BoardOrigin.X, Y, BoardOrigin.X + BoardSize.X, Y, GridLineColor, 1.0f * TextScale);
    }

    const auto DrawRoute = [this, &BoardOrigin, CellWidth, CellHeight](
        const NaiwaP7::FRoutePath& Path,
        const FLinearColor& Color,
        const float Thickness)
    {
        for (int32 Index = 1; Index < Path.Cells.Num(); ++Index)
        {
            const FVector2D Start = CellCenterOnScreen(Path.Cells[Index - 1], BoardOrigin, CellWidth, CellHeight);
            const FVector2D End = CellCenterOnScreen(Path.Cells[Index], BoardOrigin, CellWidth, CellHeight);
            DrawLine(Start.X, Start.Y, End.X, End.Y, Color, Thickness);
        }
    };

    const FLinearColor RouteAInactive(RouteAColor.R, RouteAColor.G, RouteAColor.B, 0.35f);
    const FLinearColor RouteBInactive(RouteBColor.R, RouteBColor.G, RouteBColor.B, 0.35f);
    DrawRoute(Level.RouteA, RouteAInactive, CellHeight * 0.15f);
    DrawRoute(Level.RouteB, RouteBInactive, CellHeight * 0.15f);
    if (Snapshot.ActiveRoute == NaiwaP7::ERoute::A)
    {
        DrawRoute(Level.RouteA, RouteAColor, CellHeight * 0.29f);
    }
    else
    {
        DrawRoute(Level.RouteB, RouteBColor, CellHeight * 0.29f);
    }

    const float SiteWidth = CellWidth * 0.58f;
    const float SiteHeight = CellHeight * 0.60f;
    const float OutlineThickness = FMath::Max(1.0f, 2.0f * TextScale);
    for (const FIntPoint& Cell : Level.VisibleBuildCells)
    {
        const FVector2D Center = CellCenterOnScreen(Cell, BoardOrigin, CellWidth, CellHeight);
        const FVector2D SiteOrigin(Center.X - SiteWidth * 0.5f, Center.Y - SiteHeight * 0.5f);
        DrawOutline(this, SiteOrigin, FVector2D(SiteWidth, SiteHeight), BuildSiteColor, OutlineThickness);
        DrawText(TEXT("+"), BuildSiteColor, Center.X - 5.0f * TextScale, Center.Y - 11.0f * TextScale,
            nullptr, TextScale);
    }

    const FVector2D TriggerCenter = CellCenterOnScreen(
        Level.InterferenceTriggerCell, BoardOrigin, CellWidth, CellHeight);
    DrawOutline(this, FVector2D(
        TriggerCenter.X - SiteWidth * 0.5f,
        TriggerCenter.Y - SiteHeight * 0.5f),
        FVector2D(SiteWidth, SiteHeight), InterferenceColor, OutlineThickness);
    DrawText(TEXT("!"), InterferenceColor,
        TriggerCenter.X - 4.0f * TextScale, TriggerCenter.Y - 11.0f * TextScale,
        nullptr, TextScale);

    for (const NaiwaP7::FObstacleSnapshot& Obstacle : Snapshot.Obstacles)
    {
        if (Obstacle.bCleared)
        {
            continue;
        }

        const FVector2D Center = CellCenterOnScreen(Obstacle.Cell, BoardOrigin, CellWidth, CellHeight);
        const float Width = CellWidth * 0.72f;
        const float Height = CellHeight * 0.72f;
        DrawRect(ObstacleColor, Center.X - Width * 0.5f, Center.Y - Height * 0.5f, Width, Height);
        DrawText(FString::Printf(TEXT("%s %d"), *Obstacle.ObstacleId.ToString(), Obstacle.Health),
            TextColor, Center.X - CellWidth * 0.47f, Center.Y - Height * 0.5f - 18.0f * TextScale,
            nullptr, TextScale * 0.65f);
    }

    for (const NaiwaP7::FTowerSnapshot& Tower : Snapshot.Towers)
    {
        const FVector2D Center = CellCenterOnScreen(Tower.Cell, BoardOrigin, CellWidth, CellHeight);
        const FVector2D TowerSize(CellWidth * 0.56f, CellHeight * 0.60f);
        const FVector2D TowerOrigin(Center.X - TowerSize.X * 0.5f, Center.Y - TowerSize.Y * 0.5f);
        DrawRect(TowerColor, TowerOrigin.X, TowerOrigin.Y, TowerSize.X, TowerSize.Y);
        const bool bTowerSelected = P7Controller && P7Controller->HasSelectedTowerCell() &&
            P7Controller->GetSelectedTowerCell() == Tower.Cell;
        DrawOutline(this, TowerOrigin, TowerSize,
            Tower.InterferenceStepsRemaining > 0 ? InterferenceColor :
                (bTowerSelected ? BuildSiteColor : TextColor),
            bTowerSelected ? OutlineThickness * 1.6f : OutlineThickness);
        DrawText(FString::Printf(TEXT("%s L%d"), *Tower.TowerId.ToString(), Tower.Level),
            TextColor, Center.X - TowerSize.X * 0.45f, Center.Y - 7.0f * TextScale,
            nullptr, TextScale * 0.62f);
        if (!Tower.TargetObstacleId.IsNone())
        {
            DrawText(FString::Printf(TEXT("Target: %s"), *Tower.TargetObstacleId.ToString()), MutedTextColor,
                Center.X - CellWidth * 0.5f, Center.Y + TowerSize.Y * 0.5f + 2.0f * TextScale,
                nullptr, TextScale * 0.55f);
        }
        if (Tower.InterferenceStepsRemaining > 0)
        {
            DrawText(FString::Printf(TEXT("%.1fs"), Tower.InterferenceStepsRemaining / 60.0f),
                InterferenceColor, Center.X - 16.0f * TextScale, Center.Y - TowerSize.Y * 0.5f - 18.0f * TextScale,
                nullptr, TextScale * 0.72f);
        }
    }

    for (const NaiwaP7::FObstacleSnapshot& Obstacle : Snapshot.Obstacles)
    {
        if (!Obstacle.bHiddenBuildSiteUnlocked)
        {
            continue;
        }

        const NaiwaP7::FHiddenBuildSite* Site = Level.HiddenBuildSites.FindByPredicate(
            [&Obstacle](const NaiwaP7::FHiddenBuildSite& Candidate)
            {
                return Candidate.UnlockObstacleId == Obstacle.ObstacleId;
            });
        if (!Site)
        {
            continue;
        }

        const FVector2D Center = CellCenterOnScreen(Site->Cell, BoardOrigin, CellWidth, CellHeight);
        const FVector2D SiteOrigin(Center.X - SiteWidth * 0.5f, Center.Y - SiteHeight * 0.5f);
        DrawOutline(this, SiteOrigin, FVector2D(SiteWidth, SiteHeight), BuildSiteColor, OutlineThickness);
        DrawText(TEXT("+"), BuildSiteColor, Center.X - 5.0f * TextScale, Center.Y - 11.0f * TextScale,
            nullptr, TextScale);
    }

    for (const NaiwaP7::FEnemySnapshot& Enemy : Snapshot.Enemies)
    {
        const FVector2D Center = GridPositionOnScreen(Enemy.GridPosition, BoardOrigin, CellWidth, CellHeight);
        const float Width = CellWidth * 0.32f;
        const float Height = CellHeight * 0.46f;
        DrawRect(EnemyColor, Center.X - Width * 0.5f, Center.Y - Height * 0.5f, Width, Height);
        DrawOutline(this, FVector2D(Center.X - Width * 0.5f, Center.Y - Height * 0.5f),
            FVector2D(Width, Height), TextColor, FMath::Max(1.0f, TextScale));
        DrawText(FString::Printf(TEXT("%d"), Enemy.Health), TextColor,
            Center.X - 8.0f * TextScale, Center.Y - 8.0f * TextScale, nullptr, TextScale * 0.55f);
    }

    FIntPoint HoveredCell;
    if (P7Controller && P7Controller->TryGetHoveredCell(HoveredCell))
    {
        const FVector2D Center = CellCenterOnScreen(HoveredCell, BoardOrigin, CellWidth, CellHeight);
        DrawOutline(this,
            FVector2D(Center.X - CellWidth * 0.49f, Center.Y - CellHeight * 0.49f),
            FVector2D(CellWidth * 0.98f, CellHeight * 0.98f),
            FLinearColor(0.96f, 0.96f, 0.88f, 0.95f),
            FMath::Max(1.0f, 2.0f * TextScale));
    }

    const float FooterY = BottomPanelTop + Margin * 0.5f;
    DrawText(TEXT("A route: cyan   B route: amber   + build site   ! interference trigger"),
        MutedTextColor, Margin, FooterY, nullptr, TextScale * 0.8f);
    DrawText(TEXT("1-3 tower  |  Click place/select/target  |  Right-click cancel  |  U upgrade  |  C clear target"),
        MutedTextColor, Margin, FooterY + 22.0f * TextScale, nullptr, TextScale * 0.72f);
    DrawText(TEXT("N next wave  |  A/B route in preparation  |  Space pause"),
        MutedTextColor, Margin, FooterY + 42.0f * TextScale, nullptr, TextScale * 0.72f);
    if (P7Controller && !P7Controller->GetInteractionMessage().IsEmpty())
    {
        DrawText(P7Controller->GetInteractionMessage(), TextColor,
            Margin, FooterY + 64.0f * TextScale, nullptr, TextScale * 0.72f);
    }
    DrawText(FString::Printf(TEXT("Graybox snapshot | %lld pending spawns | State %016llX"),
        Snapshot.PendingEnemySpawns, Snapshot.StateHash), MutedTextColor,
        Margin, FooterY + 86.0f * TextScale, nullptr, TextScale * 0.65f);
}
