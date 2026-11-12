#include "Presentation/NaiwaP7GrayboxHUD.h"

#include "Core/NaiwaP7Board.h"
#include "Core/NaiwaP7Simulation.h"
#include "Core/NaiwaP7TowerDefenseSubsystem.h"
#include "Presentation/NaiwaP7GrayboxPlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    const FLinearColor BoardBackground(0.77f, 0.74f, 0.65f, 1.0f);
    const FLinearColor TextColor(0.96f, 0.97f, 0.96f, 1.0f);
    const FLinearColor MutedTextColor(0.72f, 0.78f, 0.80f, 1.0f);
    const FLinearColor RouteAColor(0.08f, 0.69f, 0.77f, 1.0f);
    const FLinearColor RouteBColor(0.95f, 0.61f, 0.14f, 1.0f);
    const FLinearColor BuildSiteColor(0.10f, 0.62f, 0.36f, 1.0f);
    const FLinearColor TowerColor(0.23f, 0.52f, 0.95f, 1.0f);
    const FLinearColor EnemyColor(0.78f, 0.20f, 0.36f, 1.0f);
    const FLinearColor ObstacleColor(0.45f, 0.28f, 0.17f, 1.0f);
    const FLinearColor InterferenceColor(0.94f, 0.18f, 0.16f, 1.0f);
    const FLinearColor HudPanelColor(0.035f, 0.055f, 0.075f, 0.98f);
    const FLinearColor HudButtonColor(0.12f, 0.18f, 0.22f, 1.0f);
    constexpr int32 VFXFrameSize = 192;
    constexpr int32 VFXAtlasStride = 208;
    constexpr int32 VFXAtlasInset = 16;
    constexpr int32 MilliCellsPerCell = 1000;
    constexpr int32 HUDIconAtlasSize = 1144;
    constexpr int32 HUDIconCellSize = 256;
    constexpr int32 HUDIconCellStride = 280;
    constexpr int32 HUDIconAtlasInset = 24;
    constexpr int32 HUDIconCellCoin = 3;
    constexpr int32 HUDIconCellGuardianEgg = 4;
    constexpr int32 HUDIconCellWave = 5;
    constexpr int32 HUDIconCellRouteA = 12;
    constexpr int32 HUDIconCellRouteB = 13;
    constexpr int32 ResultBadgeAtlasWidth = 832;
    constexpr int32 ResultBadgeAtlasHeight = 416;
    constexpr int32 ResultBadgeFrameSize = 384;
    constexpr int32 ResultBadgeFrameStride = 416;
    constexpr int32 ResultBadgeAtlasInset = 16;
    constexpr int32 ForkGateAtlasWidth = 2928;
    constexpr int32 ForkGateCellWidth = 960;
    constexpr int32 ForkGateCellStride = 984;

    constexpr float FirstVisibleBoardRow = static_cast<float>(NaiwaP7::PresentationFirstVisibleRow);

    FVector2D CellCenterOnScreen(
        const FIntPoint& Cell,
        const FVector2D& BoardOrigin,
        const float CellWidth,
        const float CellHeight)
    {
        return BoardOrigin + FVector2D(
            (static_cast<float>(Cell.X) + 0.5f) * CellWidth,
            (static_cast<float>(Cell.Y) - FirstVisibleBoardRow + 0.5f) * CellHeight);
    }

    FVector2D GridPositionOnScreen(
        const FVector2D& GridPosition,
        const FVector2D& BoardOrigin,
        const float CellWidth,
        const float CellHeight)
    {
        return BoardOrigin + FVector2D(
            (GridPosition.X + 0.5f) * CellWidth,
            (GridPosition.Y - FirstVisibleBoardRow + 0.5f) * CellHeight);
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
        case ESimulationPhase::NoLevel: return TEXT("等待关卡");
        case ESimulationPhase::BetweenWaves: return TEXT("布防阶段");
        case ESimulationPhase::WaveActive: return TEXT("防守进行中");
        case ESimulationPhase::Won: return TEXT("胜利");
        case ESimulationPhase::Lost: return TEXT("失败");
        default: return TEXT("状态未知");
        }
    }

    const TCHAR* GetTowerDisplayName(const FName TowerId)
    {
        if (TowerId == FName(TEXT("P7_Tower_MilkChicken"))) return TEXT("奶鸡炮塔");
        if (TowerId == FName(TEXT("P7_Tower_MilkDog"))) return TEXT("奶狗炮塔");
        if (TowerId == FName(TEXT("P7_Tower_MilkMouse"))) return TEXT("奶鼠炮塔");
        return TEXT("炮塔");
    }

    const TCHAR* GetTowerRoleLabel(const FName TowerId)
    {
        if (TowerId == FName(TEXT("P7_Tower_MilkChicken"))) return TEXT("单体强攻");
        if (TowerId == FName(TEXT("P7_Tower_MilkDog"))) return TEXT("范围攻击");
        if (TowerId == FName(TEXT("P7_Tower_MilkMouse"))) return TEXT("减速支援");
        return TEXT("防守炮塔");
    }

    const TCHAR* GetObstacleDisplayName(const FName ObstacleId)
    {
        return ObstacleId.ToString().EndsWith(TEXT("B")) ? TEXT("燕麦饼叠垛 B") : TEXT("奶油饼干箱 A");
    }

    const TCHAR* GetEnemyMarker(const FName EnemyId)
    {
        if (EnemyId == FName(TEXT("P7_Enemy_Fast"))) return TEXT("快");
        if (EnemyId == FName(TEXT("P7_Enemy_Jammer"))) return TEXT("扰");
        return TEXT("普");
    }

    const NaiwaP7::FTowerSnapshot* FindTowerSnapshot(
        const TArray<NaiwaP7::FTowerSnapshot>& Towers,
        const NaiwaP7::FTowerSnapshot& Match)
    {
        return Towers.FindByPredicate([&Match](const NaiwaP7::FTowerSnapshot& Candidate)
        {
            return Candidate.Cell == Match.Cell && Candidate.TowerId == Match.TowerId;
        });
    }

    const NaiwaP7::FObstacleSnapshot* FindObstacleSnapshot(
        const TArray<NaiwaP7::FObstacleSnapshot>& Obstacles,
        const FName ObstacleId)
    {
        return Obstacles.FindByPredicate([ObstacleId](const NaiwaP7::FObstacleSnapshot& Candidate)
        {
            return Candidate.ObstacleId == ObstacleId;
        });
    }
    constexpr float HorizontalBoardOccupancy = 0.92f;

    float GetHudScale(const FVector2D& ViewportSize)
    {
        return FMath::Clamp(ViewportSize.Y / 720.0f, 1.0f, 1.5f);
    }

    struct FHudRect
    {
        FVector2D Origin = FVector2D::ZeroVector;
        FVector2D Size = FVector2D::ZeroVector;
    };

    bool ContainsPoint(const FHudRect& Rect, const FVector2D& Point)
    {
        return Point.X >= Rect.Origin.X && Point.Y >= Rect.Origin.Y &&
            Point.X <= Rect.Origin.X + Rect.Size.X && Point.Y <= Rect.Origin.Y + Rect.Size.Y;
    }

    FHudRect GetTowerCardRect(const FVector2D& ViewportSize, const int32 Slot)
    {
        const float Scale = GetHudScale(ViewportSize);
        const float Margin = 24.0f * Scale;
        return { FVector2D(Margin + Slot * 224.0f * Scale, Margin + 64.0f * Scale),
            FVector2D(214.0f * Scale, 56.0f * Scale) };
    }

    FHudRect GetTopButtonRect(const FVector2D& ViewportSize, const ENaiwaP7HudAction Action)
    {
        const float Scale = GetHudScale(ViewportSize);
        const float Margin = 24.0f * Scale;
        const float StartWidth = 152.0f * Scale;
        const float RouteWidth = 82.0f * Scale;
        const float PauseWidth = 88.0f * Scale;
        const float Gap = 8.0f * Scale;
        const float TotalWidth = StartWidth + RouteWidth * 2.0f + PauseWidth + Gap * 3.0f;
        const float StartX = ViewportSize.X - Margin - TotalWidth;
        const float Y = Margin + 64.0f * Scale;
        switch (Action)
        {
        case ENaiwaP7HudAction::BeginWave:
            return { FVector2D(StartX, Y), FVector2D(StartWidth, 56.0f * Scale) };
        case ENaiwaP7HudAction::SelectRouteA:
            return { FVector2D(StartX + StartWidth + Gap, Y), FVector2D(RouteWidth, 56.0f * Scale) };
        case ENaiwaP7HudAction::SelectRouteB:
            return { FVector2D(StartX + StartWidth + Gap + RouteWidth + Gap, Y),
                FVector2D(RouteWidth, 56.0f * Scale) };
        case ENaiwaP7HudAction::TogglePause:
            return { FVector2D(StartX + StartWidth + Gap + RouteWidth * 2.0f + Gap * 2.0f, Y),
                FVector2D(PauseWidth, 56.0f * Scale) };
        default:
            return {};
        }
    }

    FHudRect GetSelectionButtonRect(
        const FVector2D& ViewportSize,
        const ENaiwaP7HudAction Action)
    {
        FVector2D BoardOrigin;
        FVector2D BoardSize;
        if (!ANaiwaP7GrayboxHUD::TryCalculateBoardRect(ViewportSize, BoardOrigin, BoardSize))
        {
            return {};
        }
        const float Scale = GetHudScale(ViewportSize);
        const float Margin = 24.0f * Scale;
        const float FooterY = BoardOrigin.Y + BoardSize.Y + 16.0f * Scale + Margin * 0.5f;
        const float UpgradeWidth = 116.0f * Scale;
        const float ClearWidth = 142.0f * Scale;
        const float Gap = 8.0f * Scale;
        const float RightX = ViewportSize.X - Margin - UpgradeWidth - ClearWidth - Gap;
        if (Action == ENaiwaP7HudAction::UpgradeSelectedTower)
        {
            return { FVector2D(RightX, FooterY + 46.0f * Scale),
                FVector2D(UpgradeWidth, 34.0f * Scale) };
        }
        if (Action == ENaiwaP7HudAction::ClearSelectedTowerTarget)
        {
            return { FVector2D(RightX + UpgradeWidth + Gap, FooterY + 46.0f * Scale),
                FVector2D(ClearWidth, 34.0f * Scale) };
        }
        return {};
    }

    FHudRect GetRestartButtonRect(const FVector2D& ViewportSize)
    {
        const float Scale = GetHudScale(ViewportSize);
        return { FVector2D(ViewportSize.X * 0.5f - 96.0f * Scale,
                ViewportSize.Y * 0.5f + 48.0f * Scale),
            FVector2D(192.0f * Scale, 44.0f * Scale) };
    }

    void DrawButton(
        AHUD* Hud,
        UFont* Font,
        const FHudRect& Rect,
        const FString& Label,
        const float TextScale,
        const FLinearColor& Accent,
        const bool bEnabled = true)
    {
        if (!Hud)
        {
            return;
        }
        const FLinearColor Fill = bEnabled ? HudButtonColor : FLinearColor(0.08f, 0.10f, 0.12f, 0.9f);
        Hud->DrawRect(Fill, Rect.Origin.X, Rect.Origin.Y, Rect.Size.X, Rect.Size.Y);
        DrawOutline(Hud, Rect.Origin, Rect.Size,
            bEnabled ? Accent : FLinearColor(0.28f, 0.32f, 0.34f, 0.75f),
            FMath::Max(1.0f, TextScale));

        float TextWidth = 0.0f;
        float TextHeight = 0.0f;
        if (Font)
        {
            Hud->GetTextSize(Label, TextWidth, TextHeight, Font, 1.0f);
        }
        Hud->DrawText(Label, bEnabled ? TextColor : MutedTextColor,
            Rect.Origin.X + (Rect.Size.X - TextWidth * TextScale) * 0.5f,
            Rect.Origin.Y + (Rect.Size.Y - TextHeight * TextScale) * 0.5f,
            Font, TextScale);
    }

    void DrawCenteredText(
        AHUD* Hud,
        UFont* Font,
        const FString& Text,
        const FVector2D& Center,
        const FLinearColor& Color,
        const float Scale)
    {
        float TextWidth = 0.0f;
        float TextHeight = 0.0f;
        if (Hud && Font)
        {
            Hud->GetTextSize(Text, TextWidth, TextHeight, Font, 1.0f);
        }
        if (Hud)
        {
            Hud->DrawText(Text, Color, Center.X - TextWidth * Scale * 0.5f,
                Center.Y - TextHeight * Scale * 0.5f, Font, Scale);
        }
    }

    void DrawMapBadge(
        AHUD* Hud,
        UFont* Font,
        const FVector2D& Center,
        const FVector2D& Size,
        const FString& Label,
        const float Scale,
        const FLinearColor& Accent)
    {
        if (!Hud)
        {
            return;
        }
        const FVector2D Origin(Center.X - Size.X * 0.5f, Center.Y - Size.Y * 0.5f);
        Hud->DrawRect(HudPanelColor, Origin.X, Origin.Y, Size.X, Size.Y);
        DrawOutline(Hud, Origin, Size, Accent, FMath::Max(1.0f, Scale));
        DrawCenteredText(Hud, Font, Label, Center, TextColor, Scale * 0.70f);
    }

    void DrawDirectionArrow(
        AHUD* Hud,
        const FVector2D& Center,
        const FVector2D& Direction,
        const float Size,
        const FLinearColor& Color,
        const float Thickness)
    {
        if (!Hud || Direction.IsNearlyZero())
        {
            return;
        }
        const FVector2D Unit = Direction.GetSafeNormal();
        const FVector2D Perpendicular(-Unit.Y, Unit.X);
        const FVector2D Tail = Center - Unit * Size * 0.38f;
        const FVector2D Tip = Center + Unit * Size * 0.38f;
        const FVector2D ArrowBase = Tip - Unit * Size * 0.30f;
        Hud->DrawLine(Tail.X, Tail.Y, Tip.X, Tip.Y, Color, Thickness);
        Hud->DrawLine(Tip.X, Tip.Y, ArrowBase.X + Perpendicular.X * Size * 0.20f,
            ArrowBase.Y + Perpendicular.Y * Size * 0.20f, Color, Thickness);
        Hud->DrawLine(Tip.X, Tip.Y, ArrowBase.X - Perpendicular.X * Size * 0.20f,
            ArrowBase.Y - Perpendicular.Y * Size * 0.20f, Color, Thickness);
    }

}

ANaiwaP7GrayboxHUD::ANaiwaP7GrayboxHUD()
{
    static ConstructorHelpers::FObjectFinder<UTexture2D> GroundBoardFinder(
        TEXT("/Game/Naiwa/P7/Environment/A0/T_P7_A0_GroundBoard_Field_v01.T_P7_A0_GroundBoard_Field_v01"));
    if (GroundBoardFinder.Succeeded())
    {
        GroundBoardTexture = GroundBoardFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UTexture2D> GuardianEggFinder(
        TEXT("/Game/Naiwa/P7/Environment/A1/T_P7_A1_GuardianMilkEgg_v04.T_P7_A1_GuardianMilkEgg_v04"));
    if (GuardianEggFinder.Succeeded())
    {
        GuardianEggTexture = GuardianEggFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UTexture2D> CatalogIdlePortraitFinder(
        TEXT("/Game/Naiwa/P7/Characters/A1/CatalogDirect/T_P7_A1_CharacterHUD_Idle_Figure139_v02.T_P7_A1_CharacterHUD_Idle_Figure139_v02"));
    if (CatalogIdlePortraitFinder.Succeeded())
    {
        CatalogIdlePortrait = CatalogIdlePortraitFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UTexture2D> CatalogLaughPortraitFinder(
        TEXT("/Game/Naiwa/P7/Characters/A1/CatalogDirect/T_P7_A1_CharacterHUD_Laugh_Figure003_v03.T_P7_A1_CharacterHUD_Laugh_Figure003_v03"));
    if (CatalogLaughPortraitFinder.Succeeded())
    {
        CatalogLaughPortrait = CatalogLaughPortraitFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UTexture2D> CatalogVictoryPortraitFinder(
        TEXT("/Game/Naiwa/P7/Characters/A1/CatalogDirect/T_P7_A1_CharacterHUD_Victory_Figure121_v02.T_P7_A1_CharacterHUD_Victory_Figure121_v02"));
    if (CatalogVictoryPortraitFinder.Succeeded())
    {
        CatalogVictoryPortrait = CatalogVictoryPortraitFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UTexture2D> CatalogDistressPortraitFinder(
        TEXT("/Game/Naiwa/P7/Characters/A1/CatalogDirect/T_P7_A1_CharacterHUD_Distress_Figure106_v02.T_P7_A1_CharacterHUD_Distress_Figure106_v02"));
    if (CatalogDistressPortraitFinder.Succeeded())
    {
        CatalogDistressPortrait = CatalogDistressPortraitFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UTexture2D> HUDIconAtlasFinder(
        TEXT("/Game/Naiwa/P7/UI/A1/T_P7_A1_HUDIconAtlas_v02.T_P7_A1_HUDIconAtlas_v02"));
    if (HUDIconAtlasFinder.Succeeded())
    {
        HUDIconAtlas = HUDIconAtlasFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UTexture2D> ResultBadgeAtlasFinder(
        TEXT("/Game/Naiwa/P7/UI/A1/T_P7_A1_ResultBadgeAtlas_v02.T_P7_A1_ResultBadgeAtlas_v02"));
    if (ResultBadgeAtlasFinder.Succeeded())
    {
        ResultBadgeAtlas = ResultBadgeAtlasFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UTexture2D> ForkGateAtlasFinder(
        TEXT("/Game/Naiwa/P7/Environment/A1/T_P7_A1_ForkGateStates_v03.T_P7_A1_ForkGateStates_v03"));
    if (ForkGateAtlasFinder.Succeeded())
    {
        ForkGateAtlas = ForkGateAtlasFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UTexture2D> VFXAtlasFinder(
        TEXT("/Game/Naiwa/P7/VFX/A1/T_P7_A1_VFXAtlas_v07.T_P7_A1_VFXAtlas_v07"));
    if (VFXAtlasFinder.Succeeded())
    {
        VFXAtlas = VFXAtlasFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UTexture2D> TowerProjectileVFXAtlasFinder(
        TEXT("/Game/Naiwa/P7/VFX/A1/T_P7_A1_TowerProjectileVFX_v01.T_P7_A1_TowerProjectileVFX_v01"));
    if (TowerProjectileVFXAtlasFinder.Succeeded())
    {
        TowerProjectileVFXAtlas = TowerProjectileVFXAtlasFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UTexture2D> ChickenL1Finder(
        TEXT("/Game/Naiwa/P7/Towers/A1/T_P7_A1_SingleTarget_MilkChicken_L1_v01.T_P7_A1_SingleTarget_MilkChicken_L1_v01"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> ChickenL2Finder(
        TEXT("/Game/Naiwa/P7/Towers/A1/T_P7_A1_SingleTarget_MilkChicken_L2_v03.T_P7_A1_SingleTarget_MilkChicken_L2_v03"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> ChickenL3Finder(
        TEXT("/Game/Naiwa/P7/Towers/A1/T_P7_A1_SingleTarget_MilkChicken_L3_v03.T_P7_A1_SingleTarget_MilkChicken_L3_v03"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> DogL1Finder(
        TEXT("/Game/Naiwa/P7/Towers/A1/T_P7_A1_Area_MilkDog_L1_v01.T_P7_A1_Area_MilkDog_L1_v01"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> DogL2Finder(
        TEXT("/Game/Naiwa/P7/Towers/A1/T_P7_A1_Area_MilkDog_L2_v03.T_P7_A1_Area_MilkDog_L2_v03"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> DogL3Finder(
        TEXT("/Game/Naiwa/P7/Towers/A1/T_P7_A1_Area_MilkDog_L3_v03.T_P7_A1_Area_MilkDog_L3_v03"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> MouseL1Finder(
        TEXT("/Game/Naiwa/P7/Towers/A1/T_P7_A1_Slow_MilkMouse_L1_v02.T_P7_A1_Slow_MilkMouse_L1_v02"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> MouseL2Finder(
        TEXT("/Game/Naiwa/P7/Towers/A1/T_P7_A1_Slow_MilkMouse_L2_v03.T_P7_A1_Slow_MilkMouse_L2_v03"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> MouseL3Finder(
        TEXT("/Game/Naiwa/P7/Towers/A1/T_P7_A1_Slow_MilkMouse_L3_v04.T_P7_A1_Slow_MilkMouse_L3_v04"));

    A1TowerTextures = {
        ChickenL1Finder.Object,
        ChickenL2Finder.Object,
        ChickenL3Finder.Object,
        DogL1Finder.Object,
        DogL2Finder.Object,
        DogL3Finder.Object,
        MouseL1Finder.Object,
        MouseL2Finder.Object,
        MouseL3Finder.Object
    };
}

UTexture2D* ANaiwaP7GrayboxHUD::FindTowerTexture(const FName TowerId, const int32 Level) const
{
    int32 TowerTypeIndex = INDEX_NONE;
    if (TowerId == FName(TEXT("P7_Tower_MilkChicken")))
    {
        TowerTypeIndex = 0;
    }
    else if (TowerId == FName(TEXT("P7_Tower_MilkDog")))
    {
        TowerTypeIndex = 1;
    }
    else if (TowerId == FName(TEXT("P7_Tower_MilkMouse")))
    {
        TowerTypeIndex = 2;
    }

    if (TowerTypeIndex == INDEX_NONE || Level < 1 || Level > 3)
    {
        return nullptr;
    }

    const int32 TextureIndex = TowerTypeIndex * 3 + (Level - 1);
    return A1TowerTextures.IsValidIndex(TextureIndex) ? A1TowerTextures[TextureIndex].Get() : nullptr;
}

void ANaiwaP7GrayboxHUD::DrawTowerTexture(
    UTexture2D* Texture,
    const FVector2D& Origin,
    const FVector2D& Size)
{
    if (!Canvas || !Texture)
    {
        return;
    }

    Canvas->K2_DrawTexture(Texture, Origin, Size, FVector2D::ZeroVector, FVector2D(1.0f, 1.0f),
        FLinearColor::White, BLEND_Translucent, 0.0f, FVector2D::ZeroVector);
}

void ANaiwaP7GrayboxHUD::DrawHudIcon(
    const int32 AtlasCellIndex,
    const FVector2D& Origin,
    const FVector2D& Size,
    const FLinearColor& Tint)
{
    if (!Canvas || !HUDIconAtlas || AtlasCellIndex < 0 || AtlasCellIndex >= 16)
    {
        return;
    }

    const int32 Column = AtlasCellIndex % 4;
    const int32 Row = AtlasCellIndex / 4;
    const FVector2D UVPosition(
        static_cast<float>(HUDIconAtlasInset + Column * HUDIconCellStride) / HUDIconAtlasSize,
        static_cast<float>(HUDIconAtlasInset + Row * HUDIconCellStride) / HUDIconAtlasSize);
    const FVector2D UVSize(
        static_cast<float>(HUDIconCellSize) / HUDIconAtlasSize,
        static_cast<float>(HUDIconCellSize) / HUDIconAtlasSize);
    Canvas->K2_DrawTexture(HUDIconAtlas, Origin, Size, UVPosition, UVSize, Tint,
        BLEND_Translucent, 0.0f, FVector2D::ZeroVector);
}

void ANaiwaP7GrayboxHUD::DrawResultBadge(
    const int32 AtlasCellIndex,
    const FVector2D& Origin,
    const FVector2D& Size)
{
    if (!Canvas || !ResultBadgeAtlas || AtlasCellIndex < 0 || AtlasCellIndex > 1)
    {
        return;
    }

    const FVector2D UVPosition(
        static_cast<float>(ResultBadgeAtlasInset + AtlasCellIndex * ResultBadgeFrameStride) /
            ResultBadgeAtlasWidth,
        static_cast<float>(ResultBadgeAtlasInset) / ResultBadgeAtlasHeight);
    const FVector2D UVSize(
        static_cast<float>(ResultBadgeFrameSize) / ResultBadgeAtlasWidth,
        static_cast<float>(ResultBadgeFrameSize) / ResultBadgeAtlasHeight);
    Canvas->K2_DrawTexture(ResultBadgeAtlas, Origin, Size, UVPosition, UVSize,
        FLinearColor::White, BLEND_Translucent, 0.0f, FVector2D::ZeroVector);
}

void ANaiwaP7GrayboxHUD::DrawForkGateTile(
    const int32 AtlasCellIndex,
    const FVector2D& Origin,
    const FVector2D& Size)
{
    if (!Canvas || !ForkGateAtlas || AtlasCellIndex < 0 || AtlasCellIndex > 2)
    {
        return;
    }

    const FVector2D UVPosition(
        static_cast<float>(AtlasCellIndex * ForkGateCellStride) / ForkGateAtlasWidth,
        0.0f);
    const FVector2D UVSize(
        static_cast<float>(ForkGateCellWidth) / ForkGateAtlasWidth,
        1.0f);
    Canvas->K2_DrawTexture(ForkGateAtlas, Origin, Size, UVPosition, UVSize,
        FLinearColor::White, BLEND_Translucent, 0.0f, FVector2D::ZeroVector);
}

void ANaiwaP7GrayboxHUD::DrawGroundBoardTexture(
    const FVector2D& Origin,
    const FVector2D& Size)
{
    if (!Canvas || !GroundBoardTexture || Size.X <= 0.0f || Size.Y <= 0.0f)
    {
        return;
    }

    const float SourceAspect = static_cast<float>(GroundBoardTexture->GetSizeX()) /
        FMath::Max(1.0f, static_cast<float>(GroundBoardTexture->GetSizeY()));
    const float DestinationAspect = Size.X / Size.Y;
    FVector2D UVPosition = FVector2D::ZeroVector;
    FVector2D UVSize(1.0f, 1.0f);
    if (SourceAspect < DestinationAspect)
    {
        UVSize.Y = SourceAspect / DestinationAspect;
        UVPosition.Y = (1.0f - UVSize.Y) * 0.5f;
    }
    else if (SourceAspect > DestinationAspect)
    {
        UVSize.X = DestinationAspect / SourceAspect;
        UVPosition.X = (1.0f - UVSize.X) * 0.5f;
    }

    Canvas->K2_DrawTexture(GroundBoardTexture, Origin, Size, UVPosition, UVSize,
        FLinearColor::White, BLEND_Translucent, 0.0f, FVector2D::ZeroVector);
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
        ReferenceViewportWidth * Scale * HorizontalBoardOccupancy,
        ReferenceBoardHeight * Scale);
    const FVector2D CandidateOrigin(
        (ViewportSize.X - CandidateSize.X) * 0.5,
        (ViewportSize.Y - CandidateSize.Y) * 0.5);

    OutOrigin = CandidateOrigin;
    OutSize = CandidateSize;
    return true;
}

bool ANaiwaP7GrayboxHUD::TryHitTestInteractiveAction(
    const FVector2D& ViewportSize,
    const FVector2D& ScreenPosition,
    ENaiwaP7HudAction& OutAction,
    const bool bHasTowerSelection,
    const bool bIsTerminalPhase)
{
    OutAction = ENaiwaP7HudAction::None;
    if (!FMath::IsFinite(ViewportSize.X) || !FMath::IsFinite(ViewportSize.Y) ||
        !FMath::IsFinite(ScreenPosition.X) || !FMath::IsFinite(ScreenPosition.Y))
    {
        return false;
    }

    if (bIsTerminalPhase)
    {
        if (ContainsPoint(GetRestartButtonRect(ViewportSize), ScreenPosition))
        {
            OutAction = ENaiwaP7HudAction::RestartRun;
            return true;
        }
        return false;
    }

    for (int32 Slot = 0; Slot < 3; ++Slot)
    {
        if (ContainsPoint(GetTowerCardRect(ViewportSize, Slot), ScreenPosition))
        {
            OutAction = static_cast<ENaiwaP7HudAction>(
                static_cast<uint8>(ENaiwaP7HudAction::TowerSlot1) + Slot);
            return true;
        }
    }

    const ENaiwaP7HudAction TopActions[] = {
        ENaiwaP7HudAction::BeginWave,
        ENaiwaP7HudAction::SelectRouteA,
        ENaiwaP7HudAction::SelectRouteB,
        ENaiwaP7HudAction::TogglePause
    };
    for (const ENaiwaP7HudAction Action : TopActions)
    {
        if (ContainsPoint(GetTopButtonRect(ViewportSize, Action), ScreenPosition))
        {
            OutAction = Action;
            return true;
        }
    }

    if (bHasTowerSelection)
    {
        const ENaiwaP7HudAction SelectionActions[] = {
            ENaiwaP7HudAction::UpgradeSelectedTower,
            ENaiwaP7HudAction::ClearSelectedTowerTarget
        };
        for (const ENaiwaP7HudAction Action : SelectionActions)
        {
            if (ContainsPoint(GetSelectionButtonRect(ViewportSize, Action), ScreenPosition))
            {
                OutAction = Action;
                return true;
            }
        }
    }

    return false;
}

void ANaiwaP7GrayboxHUD::CaptureVFXEvents(
    const NaiwaP7::FSimulationSnapshot& Snapshot,
    const NaiwaP7::FLevelDefinition& Level,
    const TArray<FNaiwaP7TowerRow>& TowerRows,
    const double CurrentTime)
{
    if (!bHasPreviousSnapshot || Snapshot.SimulationStep < PreviousSnapshot.SimulationStep)
    {
        PreviousSnapshot = Snapshot;
        bHasPreviousSnapshot = true;
        ActiveVFX.Reset();
        TowersAwaitingRecoveryShot.Reset();
        RecentCoinGain = 0;
        RecentCoinGainExpiry = 0.0;
        return;
    }

    if (Snapshot.Coins > PreviousSnapshot.Coins)
    {
        RecentCoinGain = CurrentTime < RecentCoinGainExpiry
            ? RecentCoinGain + Snapshot.Coins - PreviousSnapshot.Coins
            : Snapshot.Coins - PreviousSnapshot.Coins;
        RecentCoinGainExpiry = CurrentTime + 0.85;
    }

    const uint64 StepDelta = Snapshot.SimulationStep - PreviousSnapshot.SimulationStep;
    const auto AddEffect = [this, CurrentTime](
        const EVFXPlaybackKind Kind,
        const FVector2D& StartGridPosition,
        const FVector2D& EndGridPosition,
        const float Duration,
        const bool bObstacleCleared = false)
    {
        constexpr int32 MaxConcurrentEffects = 64;
        if (ActiveVFX.Num() >= MaxConcurrentEffects)
        {
            ActiveVFX.RemoveAt(0, 1, EAllowShrinking::No);
        }

        FActiveVFX& Effect = ActiveVFX.AddDefaulted_GetRef();
        Effect.Kind = Kind;
        Effect.StartTime = CurrentTime;
        Effect.StartGridPosition = StartGridPosition;
        Effect.EndGridPosition = EndGridPosition;
        Effect.Duration = Duration;
        Effect.bObstacleCleared = bObstacleCleared;
    };

    for (const NaiwaP7::FTowerSnapshot& Tower : Snapshot.Towers)
    {
        const NaiwaP7::FTowerSnapshot* PreviousTower = FindTowerSnapshot(PreviousSnapshot.Towers, Tower);
        if (PreviousTower && PreviousTower->InterferenceStepsRemaining > 0 &&
            Tower.InterferenceStepsRemaining == 0)
        {
            TowersAwaitingRecoveryShot.Add(Tower.Cell);
        }
    }

    for (const NaiwaP7::FTowerSnapshot& Tower : Snapshot.Towers)
    {
        const NaiwaP7::FTowerSnapshot* PreviousTower = FindTowerSnapshot(PreviousSnapshot.Towers, Tower);
        if (!PreviousTower)
        {
            continue;
        }

        const FVector2D TowerPosition(static_cast<float>(Tower.Cell.X), static_cast<float>(Tower.Cell.Y));
        if (Tower.Level > PreviousTower->Level)
        {
            AddEffect(EVFXPlaybackKind::Upgrade, TowerPosition, TowerPosition, 0.60f);
        }

        if (StepDelta == 0 || Tower.Level != PreviousTower->Level || Tower.InterferenceStepsRemaining > 0)
        {
            continue;
        }

        const FNaiwaP7TowerRow* TowerRow = TowerRows.FindByPredicate([&Tower](const FNaiwaP7TowerRow& Candidate)
        {
            return Candidate.TowerId == Tower.TowerId && Candidate.Level == Tower.Level;
        });
        if (!TowerRow)
        {
            continue;
        }

        const int32 ExpectedCooldown = PreviousTower->AttackCooldownSteps > StepDelta
            ? PreviousTower->AttackCooldownSteps - static_cast<int32>(StepDelta)
            : 0;
        const bool bCooldownReset = Tower.AttackCooldownSteps > ExpectedCooldown;
        if (!bCooldownReset || !Tower.TargetObstacleId.IsNone() || !PreviousTower->TargetObstacleId.IsNone())
        {
            continue;
        }

        const int64 RangeSquared = static_cast<int64>(TowerRow->RangeMilliCells) * TowerRow->RangeMilliCells;
        const int32 RouteACells = FMath::Max(1, Level.RouteA.Cells.Num());
        const int32 RouteBCells = FMath::Max(1, Level.RouteB.Cells.Num());
        const NaiwaP7::FEnemySnapshot* Target = nullptr;
        int64 BestDistanceToEndpoint = MAX_int64;
        int64 BestDistanceSquared = MAX_int64;
        const auto ConsiderTarget = [&](const NaiwaP7::FEnemySnapshot& Enemy)
        {
            const int64 EnemyX = FMath::RoundToInt64(Enemy.GridPosition.X * MilliCellsPerCell);
            const int64 EnemyY = FMath::RoundToInt64(Enemy.GridPosition.Y * MilliCellsPerCell);
            const int64 Dx = EnemyX - static_cast<int64>(Tower.Cell.X) * MilliCellsPerCell;
            const int64 Dy = EnemyY - static_cast<int64>(Tower.Cell.Y) * MilliCellsPerCell;
            const int64 DistanceSquared = Dx * Dx + Dy * Dy;
            if (DistanceSquared > RangeSquared)
            {
                return;
            }

            const int64 MaxProgress = static_cast<int64>(
                (Enemy.Route == NaiwaP7::ERoute::A ? RouteACells : RouteBCells) - 1) * MilliCellsPerCell;
            const int64 DistanceToEndpoint = MaxProgress - Enemy.ProgressMilliCells;
            if (DistanceToEndpoint < BestDistanceToEndpoint ||
                (DistanceToEndpoint == BestDistanceToEndpoint &&
                    (DistanceSquared < BestDistanceSquared ||
                        (DistanceSquared == BestDistanceSquared &&
                            (!Target || Enemy.EntityId < Target->EntityId)))))
            {
                Target = &Enemy;
                BestDistanceToEndpoint = DistanceToEndpoint;
                BestDistanceSquared = DistanceSquared;
            }
        };
        for (const NaiwaP7::FEnemySnapshot& Enemy : Snapshot.Enemies)
        {
            ConsiderTarget(Enemy);
        }
        for (const NaiwaP7::FEnemySnapshot& Enemy : PreviousSnapshot.Enemies)
        {
            if (!Snapshot.Enemies.ContainsByPredicate([&Enemy](const NaiwaP7::FEnemySnapshot& CurrentEnemy)
                { return CurrentEnemy.EntityId == Enemy.EntityId; }))
            {
                ConsiderTarget(Enemy);
            }
        }
        if (Target)
        {
            const bool bRecoveringFromJam = TowersAwaitingRecoveryShot.Contains(Tower.Cell);
            if (bRecoveringFromJam)
            {
                TowersAwaitingRecoveryShot.Remove(Tower.Cell);
            }
            EVFXPlaybackKind ProjectileEffect = EVFXPlaybackKind::Shot;
            if (Tower.TowerId == FName(TEXT("P7_Tower_MilkDog")))
            {
                ProjectileEffect = EVFXPlaybackKind::DogShot;
            }
            else if (Tower.TowerId == FName(TEXT("P7_Tower_MilkMouse")))
            {
                ProjectileEffect = EVFXPlaybackKind::MouseShot;
            }

            const bool bUseChickenRecoveryAccent =
                bRecoveringFromJam && ProjectileEffect == EVFXPlaybackKind::Shot;
            AddEffect(bUseChickenRecoveryAccent ? EVFXPlaybackKind::JamRecovery : ProjectileEffect,
                TowerPosition, Target->GridPosition, bUseChickenRecoveryAccent ? 0.30f : 0.48f);
        }
    }

    for (const NaiwaP7::FObstacleSnapshot& Obstacle : Snapshot.Obstacles)
    {
        const NaiwaP7::FObstacleSnapshot* PreviousObstacle =
            FindObstacleSnapshot(PreviousSnapshot.Obstacles, Obstacle.ObstacleId);
        if (!PreviousObstacle || PreviousObstacle->Health <= Obstacle.Health)
        {
            continue;
        }

        FVector2D SourcePosition(static_cast<float>(Obstacle.Cell.X), static_cast<float>(Obstacle.Cell.Y));
        for (const NaiwaP7::FTowerSnapshot& PreviousTower : PreviousSnapshot.Towers)
        {
            if (PreviousTower.TargetObstacleId == Obstacle.ObstacleId)
            {
                SourcePosition = FVector2D(
                    static_cast<float>(PreviousTower.Cell.X), static_cast<float>(PreviousTower.Cell.Y));
                break;
            }
        }

        const FVector2D ObstaclePosition(static_cast<float>(Obstacle.Cell.X), static_cast<float>(Obstacle.Cell.Y));
        const float Duration = Obstacle.bCleared ? 0.56f : 0.28f;
        AddEffect(EVFXPlaybackKind::Clear, SourcePosition, ObstaclePosition, Duration, Obstacle.bCleared);
    }

    for (const NaiwaP7::FEnemySnapshot& Enemy : Snapshot.Enemies)
    {
        if (!Enemy.bInterferenceTriggered)
        {
            continue;
        }
        const NaiwaP7::FEnemySnapshot* PreviousEnemy = PreviousSnapshot.Enemies.FindByPredicate(
            [&Enemy](const NaiwaP7::FEnemySnapshot& Candidate)
            {
                return Candidate.EntityId == Enemy.EntityId;
            });
        if (PreviousEnemy && PreviousEnemy->bInterferenceTriggered)
        {
            continue;
        }

        for (const NaiwaP7::FTowerSnapshot& Tower : Snapshot.Towers)
        {
            const NaiwaP7::FTowerSnapshot* PreviousTower = FindTowerSnapshot(PreviousSnapshot.Towers, Tower);
            if (Tower.InterferenceStepsRemaining <= 0 ||
                (PreviousTower && PreviousTower->InterferenceStepsRemaining > 0))
            {
                continue;
            }

            const FVector2D TowerPosition(static_cast<float>(Tower.Cell.X), static_cast<float>(Tower.Cell.Y));
            AddEffect(EVFXPlaybackKind::Jam, Enemy.GridPosition, TowerPosition, 0.60f);
        }
    }

    PreviousSnapshot = Snapshot;
    bHasPreviousSnapshot = true;
}

void ANaiwaP7GrayboxHUD::DrawVFX(
    const FVector2D& BoardOrigin,
    const float CellWidth,
    const float CellHeight,
    const double CurrentTime)
{
    if (!Canvas || !VFXAtlas)
    {
        return;
    }

    for (int32 EffectIndex = ActiveVFX.Num() - 1; EffectIndex >= 0; --EffectIndex)
    {
        const FActiveVFX& Effect = ActiveVFX[EffectIndex];
        const float Age = static_cast<float>(CurrentTime - Effect.StartTime);
        if (Age >= Effect.Duration)
        {
            ActiveVFX.RemoveAt(EffectIndex, 1, EAllowShrinking::No);
            continue;
        }

        const float SafeAge = FMath::Max(0.0f, Age);
        const FVector2D StartScreen = GridPositionOnScreen(
            Effect.StartGridPosition, BoardOrigin, CellWidth, CellHeight);
        const FVector2D EndScreen = GridPositionOnScreen(
            Effect.EndGridPosition, BoardOrigin, CellWidth, CellHeight);
        UTexture2D* EffectAtlas = VFXAtlas;
        int32 AtlasRowOffset = 0;
        FVector2D DrawPosition = StartScreen;
        int32 FrameIndex = 0;
        float Alpha = 1.0f;
        bool bRotateAlongPath = false;

        switch (Effect.Kind)
        {
        case EVFXPlaybackKind::Shot:
        case EVFXPlaybackKind::DogShot:
        case EVFXPlaybackKind::MouseShot:
            if (Effect.Kind == EVFXPlaybackKind::DogShot)
            {
                EffectAtlas = TowerProjectileVFXAtlas;
            }
            else if (Effect.Kind == EVFXPlaybackKind::MouseShot)
            {
                EffectAtlas = TowerProjectileVFXAtlas;
                AtlasRowOffset = 1;
            }
            if (SafeAge < 0.08f)
            {
                FrameIndex = 0;
            }
            else if (SafeAge < 0.26f)
            {
                FrameIndex = 1;
                const float TravelAlpha = (SafeAge - 0.08f) / 0.18f;
                DrawPosition = FMath::Lerp(StartScreen, EndScreen, TravelAlpha);
                bRotateAlongPath = true;
            }
            else if (SafeAge < 0.36f)
            {
                FrameIndex = 2;
                DrawPosition = EndScreen;
            }
            else
            {
                FrameIndex = 3;
                DrawPosition = EndScreen;
                Alpha = FMath::Clamp((Effect.Duration - SafeAge) / 0.12f, 0.0f, 1.0f);
            }
            break;

        case EVFXPlaybackKind::Clear:
        {
            const int32 FrameCount = Effect.bObstacleCleared ? 4 : 2;
            const float FrameDuration = Effect.Duration / FrameCount;
            FrameIndex = 4 + FMath::Min(FrameCount - 1, FMath::FloorToInt(SafeAge / FrameDuration));
            DrawPosition = EndScreen;
            if (SafeAge > Effect.Duration - 0.12f)
            {
                Alpha = FMath::Clamp((Effect.Duration - SafeAge) / 0.12f, 0.0f, 1.0f);
            }
            break;
        }

        case EVFXPlaybackKind::Upgrade:
            FrameIndex = 8 + FMath::Min(3, FMath::FloorToInt(SafeAge / (Effect.Duration / 4.0f)));
            DrawPosition = EndScreen;
            if (SafeAge > Effect.Duration - 0.12f)
            {
                Alpha = FMath::Clamp((Effect.Duration - SafeAge) / 0.12f, 0.0f, 1.0f);
            }
            break;

        case EVFXPlaybackKind::Jam:
            if (SafeAge < 0.16f)
            {
                FrameIndex = 12;
            }
            else if (SafeAge < 0.42f)
            {
                FrameIndex = 13;
                const float TravelAlpha = (SafeAge - 0.16f) / 0.26f;
                DrawPosition = FMath::Lerp(StartScreen, EndScreen, TravelAlpha);
                bRotateAlongPath = true;
            }
            else
            {
                FrameIndex = 14;
                DrawPosition = EndScreen;
                Alpha = FMath::Clamp((Effect.Duration - SafeAge) / 0.18f, 0.0f, 1.0f);
            }
            break;

        case EVFXPlaybackKind::JamRecovery:
            FrameIndex = 15;
            DrawPosition = FMath::Lerp(StartScreen, EndScreen,
                FMath::Clamp(SafeAge / Effect.Duration, 0.0f, 1.0f));
            bRotateAlongPath = true;
            Alpha = FMath::Clamp((Effect.Duration - SafeAge) / 0.10f, 0.0f, 1.0f);
            break;
        }

        if (!EffectAtlas)
        {
            continue;
        }
        const int32 AtlasFrameIndex = FrameIndex + AtlasRowOffset * 4;
        const int32 Column = AtlasFrameIndex % 4;
        const int32 Row = AtlasFrameIndex / 4;
        const float AtlasWidth = static_cast<float>(EffectAtlas->GetSizeX());
        const float AtlasHeight = static_cast<float>(EffectAtlas->GetSizeY());
        const FVector2D UVPosition(
            static_cast<float>(VFXAtlasInset + Column * VFXAtlasStride) / AtlasWidth,
            static_cast<float>(VFXAtlasInset + Row * VFXAtlasStride) / AtlasHeight);
        const FVector2D UVSize(
            static_cast<float>(VFXFrameSize) / AtlasWidth,
            static_cast<float>(VFXFrameSize) / AtlasHeight);
        const float SpriteSize = CellHeight * 2.8f;
        const FVector2D SpriteOrigin(DrawPosition.X - SpriteSize * 0.5f,
            DrawPosition.Y - SpriteSize * 0.5f);
        const FVector2D Direction = EndScreen - StartScreen;
        const float Rotation = bRotateAlongPath && !Direction.IsNearlyZero()
            ? FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X))
            : 0.0f;
        Canvas->K2_DrawTexture(EffectAtlas, SpriteOrigin, FVector2D(SpriteSize, SpriteSize),
            UVPosition, UVSize, FLinearColor(1.0f, 1.0f, 1.0f, Alpha),
            BLEND_Translucent, Rotation, FVector2D(0.5f, 0.5f));
    }
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

    UFont* HudFont = GEngine ? GEngine->GetMediumFont() : nullptr;
    const float HudScale = GetHudScale(ViewportSize);
    const float Margin = 24.0f * HudScale;
    const float CellWidth = BoardSize.X / NaiwaP7::BoardColumns;
    const float CellHeight = BoardSize.Y / NaiwaP7::PresentationVisibleRows;
    const float OutlineThickness = FMath::Max(1.0f, 1.5f * HudScale);
    const float TopPanelBottom = FMath::Max(0.0f, BoardOrigin.Y - 16.0f * HudScale);
    const float BottomPanelTop = BoardOrigin.Y + BoardSize.Y + 16.0f * HudScale;

    DrawRect(HudPanelColor, 0.0f, 0.0f, ViewportSize.X, TopPanelBottom);
    DrawRect(HudPanelColor, 0.0f, BottomPanelTop, ViewportSize.X,
        FMath::Max(0.0f, ViewportSize.Y - BottomPanelTop));

    UNaiwaP7TowerDefenseSubsystem* P7Subsystem = GetWorld()->GetSubsystem<UNaiwaP7TowerDefenseSubsystem>();
    if (!P7Subsystem || !P7Subsystem->GetSimulation().HasLevel())
    {
        bHasPreviousSnapshot = false;
        ActiveVFX.Reset();
        TowersAwaitingRecoveryShot.Reset();
        DrawText(TEXT("奶罐守卫战"), TextColor, Margin, Margin, HudFont, HudScale * 1.05f);
        DrawText(TEXT("关卡数据未加载，请检查 P7 关卡和五张数据表。"), MutedTextColor,
            Margin, Margin + 34.0f * HudScale, HudFont, HudScale * 0.82f);
        return;
    }

    const NaiwaP7::FSimulationSnapshot Snapshot = P7Subsystem->GetSnapshot();
    const NaiwaP7::FLevelDefinition& Level = P7Subsystem->GetSimulation().GetLevelDefinition();
    const TArray<FNaiwaP7TowerRow>& TowerRows = P7Subsystem->GetSimulation().GetTowerRows();
    const TArray<FNaiwaP7ObstacleRow>& ObstacleRows = P7Subsystem->GetSimulation().GetObstacleRows();
    const double CurrentTime = GetWorld()->GetTimeSeconds();
    CaptureVFXEvents(Snapshot, Level, TowerRows, CurrentTime);
    const ANaiwaP7GrayboxPlayerController* P7Controller =
        Cast<ANaiwaP7GrayboxPlayerController>(PlayerOwner);
    const bool bPreparing = Snapshot.Phase == NaiwaP7::ESimulationPhase::BetweenWaves;
    const bool bWaveActive = Snapshot.Phase == NaiwaP7::ESimulationPhase::WaveActive;
    const bool bTerminal = Snapshot.Phase == NaiwaP7::ESimulationPhase::Won ||
        Snapshot.Phase == NaiwaP7::ESimulationPhase::Lost;
    const bool bPaused = P7Subsystem->IsPaused();

    int32 EnemyCount = Snapshot.Enemies.Num();
    int32 ClearedObstacleCount = 0;
    for (const NaiwaP7::FObstacleSnapshot& Obstacle : Snapshot.Obstacles)
    {
        ClearedObstacleCount += Obstacle.bCleared ? 1 : 0;
    }

    const FString WaveLabel = FString::Printf(TEXT("波次  %d / %d"), Snapshot.CurrentWave, Snapshot.TotalWaves);
    const FString CoinsLabel = FString::Printf(TEXT("金币  %d"), Snapshot.Coins);
    const FString RouteLabel = Snapshot.ActiveRoute == NaiwaP7::ERoute::A
        ? TEXT("路线  A 直行") : TEXT("路线  B 绕行");
    const FString PhaseLabel = GetPhaseLabel(Snapshot.Phase);
    const FString JarLabel = FString::Printf(TEXT("奶罐耐久  %d / %d"), Snapshot.MilkJarHealth,
        NaiwaP7::FSimulation::StartingMilkJarHealth);

    DrawText(TEXT("奶罐守卫战"), TextColor, Margin, Margin, HudFont, HudScale * 1.02f);
    if (!bTerminal)
    {
        UTexture2D* StatePortrait = bWaveActive ? CatalogLaughPortrait : CatalogIdlePortrait;
        const float PortraitSize = 42.0f * HudScale;
        DrawTowerTexture(StatePortrait,
            FVector2D(Margin + 150.0f * HudScale, Margin + 7.0f * HudScale),
            FVector2D(PortraitSize, PortraitSize));
    }
    DrawHudIcon(HUDIconCellGuardianEgg, FVector2D(Margin, Margin + 26.0f * HudScale),
        FVector2D(20.0f * HudScale, 20.0f * HudScale));
    DrawText(JarLabel, Snapshot.MilkJarHealth > 3 ? MutedTextColor : InterferenceColor,
        Margin + 25.0f * HudScale, Margin + 29.0f * HudScale, HudFont, HudScale * 0.72f);
    const float JarBarX = Margin;
    const float JarBarY = Margin + 51.0f * HudScale;
    const float JarBarWidth = 184.0f * HudScale;
    const float JarBarHeight = 8.0f * HudScale;
    DrawRect(FLinearColor(0.17f, 0.20f, 0.21f, 1.0f), JarBarX, JarBarY, JarBarWidth, JarBarHeight);
    const float JarFraction = static_cast<float>(Snapshot.MilkJarHealth) /
        NaiwaP7::FSimulation::StartingMilkJarHealth;
    DrawRect(JarFraction > 0.3f ? BuildSiteColor : InterferenceColor,
        JarBarX, JarBarY, JarBarWidth * FMath::Clamp(JarFraction, 0.0f, 1.0f), JarBarHeight);

    const FVector2D StatusIconSize(20.0f * HudScale, 20.0f * HudScale);
    DrawHudIcon(HUDIconCellWave,
        FVector2D(Margin + 205.0f * HudScale, Margin - 2.0f * HudScale), StatusIconSize);
    DrawText(WaveLabel, TextColor, Margin + 230.0f * HudScale, Margin + 5.0f * HudScale,
        HudFont, HudScale * 0.84f);
    DrawHudIcon(HUDIconCellCoin,
        FVector2D(Margin + 380.0f * HudScale, Margin - 2.0f * HudScale), StatusIconSize);
    DrawText(CoinsLabel, FLinearColor(1.0f, 0.79f, 0.30f, 1.0f),
        Margin + 405.0f * HudScale, Margin + 5.0f * HudScale, HudFont, HudScale * 0.84f);
    if (RecentCoinGain > 0 && CurrentTime < RecentCoinGainExpiry)
    {
        const float Alpha = FMath::Clamp(static_cast<float>(RecentCoinGainExpiry - CurrentTime) / 0.25f,
            0.0f, 1.0f);
        DrawText(FString::Printf(TEXT("+%d 金币"), RecentCoinGain),
            FLinearColor(1.0f, 0.87f, 0.36f, Alpha),
            Margin + 485.0f * HudScale, Margin + 5.0f * HudScale, HudFont, HudScale * 0.58f);
    }
    DrawHudIcon(Snapshot.ActiveRoute == NaiwaP7::ERoute::A
            ? HUDIconCellRouteA : HUDIconCellRouteB,
        FVector2D(Margin + 550.0f * HudScale, Margin - 2.0f * HudScale), StatusIconSize,
        Snapshot.ActiveRoute == NaiwaP7::ERoute::A ? RouteAColor : RouteBColor);
    DrawText(RouteLabel, Snapshot.ActiveRoute == NaiwaP7::ERoute::A ? RouteAColor : RouteBColor,
        Margin + 575.0f * HudScale, Margin + 5.0f * HudScale, HudFont, HudScale * 0.84f);
    DrawText(PhaseLabel, bPaused ? InterferenceColor : MutedTextColor,
        Margin + 765.0f * HudScale, Margin + 5.0f * HudScale, HudFont, HudScale * 0.84f);
    DrawText(FString::Printf(TEXT("场上敌人  %d   ·   待出场  %lld"), EnemyCount, Snapshot.PendingEnemySpawns),
        MutedTextColor, Margin + 940.0f * HudScale, Margin + 5.0f * HudScale, HudFont, HudScale * 0.70f);

    TArray<FNaiwaP7TowerRow> TowerChoices;
    for (const FNaiwaP7TowerRow& TowerRow : TowerRows)
    {
        if (TowerRow.Level == 1 && !TowerChoices.ContainsByPredicate([&TowerRow](const FNaiwaP7TowerRow& Existing)
            { return Existing.TowerId == TowerRow.TowerId; }))
        {
            TowerChoices.Add(TowerRow);
        }
    }
    TowerChoices.Sort([](const FNaiwaP7TowerRow& A, const FNaiwaP7TowerRow& B)
    {
        return A.TowerId.Compare(B.TowerId) < 0;
    });

    for (int32 Slot = 0; Slot < 3; ++Slot)
    {
        const FHudRect Card = GetTowerCardRect(ViewportSize, Slot);
        const FNaiwaP7TowerRow* TowerRow = TowerChoices.IsValidIndex(Slot) ? &TowerChoices[Slot] : nullptr;
        const bool bSelected = TowerRow && P7Controller &&
            P7Controller->GetSelectedTowerId() == TowerRow->TowerId;
        DrawRect(HudButtonColor, Card.Origin.X, Card.Origin.Y, Card.Size.X, Card.Size.Y);
        DrawOutline(this, Card.Origin, Card.Size,
            bSelected ? BuildSiteColor : FLinearColor(0.32f, 0.42f, 0.46f, 1.0f), OutlineThickness);

        if (!TowerRow)
        {
            continue;
        }
        const float IconSize = 46.0f * HudScale;
        const FVector2D IconOrigin(Card.Origin.X + 6.0f * HudScale,
            Card.Origin.Y + (Card.Size.Y - IconSize) * 0.5f);
        if (UTexture2D* ChoiceTexture = FindTowerTexture(TowerRow->TowerId, 1))
        {
            DrawTowerTexture(ChoiceTexture, IconOrigin, FVector2D(IconSize, IconSize));
        }
        else
        {
            DrawRect(TowerColor, IconOrigin.X, IconOrigin.Y, IconSize, IconSize);
        }

        const FString TowerRoleAndPrice = FString::Printf(TEXT("%s  ·  %d 金币"),
            GetTowerRoleLabel(TowerRow->TowerId), TowerRow->CostCoins);
        DrawText(GetTowerDisplayName(TowerRow->TowerId), TextColor,
            Card.Origin.X + 61.0f * HudScale, Card.Origin.Y + 9.0f * HudScale,
            HudFont, HudScale * 0.75f);
        DrawText(TowerRoleAndPrice, MutedTextColor,
            Card.Origin.X + 61.0f * HudScale, Card.Origin.Y + 31.0f * HudScale,
            HudFont, HudScale * 0.56f);
        DrawText(FString::Printf(TEXT("%d"), Slot + 1), MutedTextColor,
            Card.Origin.X + Card.Size.X - 17.0f * HudScale, Card.Origin.Y + 3.0f * HudScale,
            HudFont, HudScale * 0.46f);
    }

    const FHudRect StartButton = GetTopButtonRect(ViewportSize, ENaiwaP7HudAction::BeginWave);
    FString StartLabel;
    if (bPreparing)
    {
        StartLabel = FString::Printf(TEXT("开始第 %d 波"), Snapshot.CurrentWave + 1);
    }
    else if (bWaveActive)
    {
        StartLabel = FString::Printf(TEXT("第 %d 波进行中"), Snapshot.CurrentWave);
    }
    else
    {
        StartLabel = TEXT("开始下一波");
    }
    DrawButton(this, HudFont, StartButton, StartLabel, HudScale * 0.66f, BuildSiteColor, bPreparing);

    const FHudRect RouteAButton = GetTopButtonRect(ViewportSize, ENaiwaP7HudAction::SelectRouteA);
    const FHudRect RouteBButton = GetTopButtonRect(ViewportSize, ENaiwaP7HudAction::SelectRouteB);
    const bool bRouteASelected = Snapshot.ActiveRoute == NaiwaP7::ERoute::A;
    const bool bRouteBSelected = Snapshot.ActiveRoute == NaiwaP7::ERoute::B;
    DrawButton(this, HudFont, RouteAButton, bRouteASelected ? TEXT("A 直行 ✓") : TEXT("A 直行"),
        HudScale * 0.62f, RouteAColor, bPreparing && !bRouteASelected);
    DrawButton(this, HudFont, RouteBButton, bRouteBSelected ? TEXT("B 绕行 ✓") : TEXT("B 绕行"),
        HudScale * 0.62f, RouteBColor, bPreparing && !bRouteBSelected);
    const FHudRect PauseButton = GetTopButtonRect(ViewportSize, ENaiwaP7HudAction::TogglePause);
    const FString PauseLabel = bPaused ? TEXT("继续") : TEXT("暂停");
    DrawButton(this, HudFont, PauseButton, PauseLabel, HudScale * 0.68f,
        bPaused ? BuildSiteColor : MutedTextColor, bWaveActive);

    DrawRect(BoardBackground, BoardOrigin.X, BoardOrigin.Y, BoardSize.X, BoardSize.Y);
    DrawGroundBoardTexture(BoardOrigin, BoardSize);
    const auto DrawRoute = [this, &BoardOrigin, CellWidth, CellHeight, HudScale](
        const NaiwaP7::FRoutePath& Path,
        const FLinearColor& RouteColor,
        const bool bActive)
    {
        if (Path.Cells.Num() < 2)
        {
            return;
        }
        const float Alpha = bActive ? 1.0f : 0.34f;
        const FLinearColor LaneColor(RouteColor.R, RouteColor.G, RouteColor.B, Alpha);
        const FLinearColor LaneEdge(0.12f, 0.15f, 0.16f, bActive ? 0.90f : 0.36f);
        const float LaneWidth = CellHeight * 0.66f;
        for (int32 Index = 1; Index < Path.Cells.Num(); ++Index)
        {
            const FVector2D Start = CellCenterOnScreen(Path.Cells[Index - 1], BoardOrigin, CellWidth, CellHeight);
            const FVector2D End = CellCenterOnScreen(Path.Cells[Index], BoardOrigin, CellWidth, CellHeight);
            DrawLine(Start.X, Start.Y, End.X, End.Y, LaneEdge, LaneWidth + 4.0f * HudScale);
        }
        for (int32 Index = 1; Index < Path.Cells.Num(); ++Index)
        {
            const FVector2D Start = CellCenterOnScreen(Path.Cells[Index - 1], BoardOrigin, CellWidth, CellHeight);
            const FVector2D End = CellCenterOnScreen(Path.Cells[Index], BoardOrigin, CellWidth, CellHeight);
            DrawLine(Start.X, Start.Y, End.X, End.Y, LaneColor, LaneWidth);
        }
        for (int32 Index = 2; Index < Path.Cells.Num() - 1; ++Index)
        {
            if ((Index % 4) != 2)
            {
                continue;
            }
            const FIntPoint Previous = Path.Cells[Index - 1] - Path.Cells[Index - 2];
            const FIntPoint Next = Path.Cells[Index + 1] - Path.Cells[Index];
            if (Previous != Next)
            {
                continue;
            }
            const FVector2D Center = CellCenterOnScreen(Path.Cells[Index], BoardOrigin, CellWidth, CellHeight);
            DrawDirectionArrow(this, Center, FVector2D(static_cast<float>(Next.X), static_cast<float>(Next.Y)),
                CellHeight * 0.23f, FLinearColor(1.0f, 0.97f, 0.85f, bActive ? 1.0f : 0.40f),
                FMath::Max(1.0f, 1.8f * HudScale));
        }
    };

    if (Snapshot.ActiveRoute == NaiwaP7::ERoute::A)
    {
        DrawRoute(Level.RouteB, RouteBColor, false);
        DrawRoute(Level.RouteA, RouteAColor, true);
    }
    else
    {
        DrawRoute(Level.RouteA, RouteAColor, false);
        DrawRoute(Level.RouteB, RouteBColor, true);
    }

    const int32 SplitGateIndex = Snapshot.ActiveRoute == NaiwaP7::ERoute::A ? 0 : 1;
    const int32 MergeGateIndex = Snapshot.ActiveRoute == NaiwaP7::ERoute::A ? 0 : 2;
    for (const TPair<FIntPoint, int32>& Gate : {
        TPair<FIntPoint, int32>(FIntPoint(1, 5), SplitGateIndex),
        TPair<FIntPoint, int32>(FIntPoint(18, 5), MergeGateIndex) })
    {
        const FVector2D Center = CellCenterOnScreen(Gate.Key, BoardOrigin, CellWidth, CellHeight);
        const FVector2D GateSize(CellWidth, CellHeight);
        DrawForkGateTile(Gate.Value, Center - GateSize * 0.5f, GateSize);
    }

    const FVector2D EntranceCenter = CellCenterOnScreen(FIntPoint(0, 4), BoardOrigin, CellWidth, CellHeight);
    const FVector2D ExitCenter = CellCenterOnScreen(FIntPoint(19, 4), BoardOrigin, CellWidth, CellHeight);
    DrawMapBadge(this, HudFont, EntranceCenter, FVector2D(CellWidth * 0.92f, CellHeight * 0.52f),
        TEXT("怪物入口"), HudScale, EnemyColor);
    DrawMapBadge(this, HudFont, ExitCenter, FVector2D(CellWidth * 0.92f, CellHeight * 0.52f),
        TEXT("奶罐终点"), HudScale, BuildSiteColor);

    const FVector2D EntryLaneCenter = CellCenterOnScreen(Level.RouteA.Cells[0], BoardOrigin, CellWidth, CellHeight);
    DrawDirectionArrow(this, EntryLaneCenter, FVector2D(1.0f, 0.0f), CellHeight * 0.42f,
        FLinearColor(1.0f, 1.0f, 1.0f, 0.96f), FMath::Max(2.0f, 3.0f * HudScale));
    const FVector2D JarCenter(BoardOrigin.X + BoardSize.X + Margin,
        CellCenterOnScreen(Level.RouteA.Cells.Last(), BoardOrigin, CellWidth, CellHeight).Y);
    const FVector2D JarSize(48.0f * HudScale, 48.0f * HudScale);
    if (GuardianEggTexture)
    {
        DrawTowerTexture(GuardianEggTexture, JarCenter - JarSize * 0.5f, JarSize);
    }
    else
    {
        const FVector2D IconSize(30.0f * HudScale, 30.0f * HudScale);
        DrawHudIcon(HUDIconCellGuardianEgg, JarCenter - IconSize * 0.5f, IconSize);
    }

    const FVector2D RouteALabelCenter = CellCenterOnScreen(FIntPoint(7, 6), BoardOrigin, CellWidth, CellHeight);
    const FVector2D RouteBLabelCenter = CellCenterOnScreen(FIntPoint(8, 3), BoardOrigin, CellWidth, CellHeight);
    DrawMapBadge(this, HudFont, RouteALabelCenter, FVector2D(CellWidth * 1.34f, CellHeight * 0.54f),
        TEXT("A 直行"), HudScale, RouteAColor);
    DrawMapBadge(this, HudFont, RouteBLabelCenter, FVector2D(CellWidth * 1.34f, CellHeight * 0.54f),
        TEXT("B 绕行"), HudScale, RouteBColor);

    const float SiteWidth = CellWidth * 0.96f;
    const float SiteHeight = CellHeight * 0.96f;
    const auto DrawBuildSite = [this, HudFont, &BoardOrigin, CellWidth, CellHeight,
        SiteWidth, SiteHeight, HudScale, OutlineThickness](const FIntPoint& Cell, const bool bHiddenUnlocked)
    {
        const FVector2D Center = CellCenterOnScreen(Cell, BoardOrigin, CellWidth, CellHeight);
        const FVector2D SiteOrigin(Center.X - SiteWidth * 0.5f, Center.Y - SiteHeight * 0.5f);
        DrawRect(FLinearColor(0.10f, 0.55f, 0.30f, 0.10f),
            SiteOrigin.X, SiteOrigin.Y, SiteWidth, SiteHeight);
        DrawOutline(this, SiteOrigin, FVector2D(SiteWidth, SiteHeight), BuildSiteColor,
            bHiddenUnlocked ? OutlineThickness * 1.5f : OutlineThickness);
        DrawCenteredText(this, HudFont, bHiddenUnlocked ? TEXT("新建位") : TEXT("建造"),
            Center, FLinearColor(0.05f, 0.27f, 0.16f, 1.0f), HudScale * 0.56f);
    };
    for (const FIntPoint& Cell : Level.VisibleBuildCells)
    {
        DrawBuildSite(Cell, false);
    }
    for (const NaiwaP7::FObstacleSnapshot& Obstacle : Snapshot.Obstacles)
    {
        if (!Obstacle.bHiddenBuildSiteUnlocked)
        {
            continue;
        }
        const NaiwaP7::FHiddenBuildSite* Site = Level.HiddenBuildSites.FindByPredicate(
            [&Obstacle](const NaiwaP7::FHiddenBuildSite& Candidate)
            { return Candidate.UnlockObstacleId == Obstacle.ObstacleId; });
        if (Site)
        {
            DrawBuildSite(Site->Cell, true);
        }
    }

    const FVector2D TriggerCenter = CellCenterOnScreen(
        Level.InterferenceTriggerCell, BoardOrigin, CellWidth, CellHeight);
    const FVector2D TriggerRect(CellWidth * 0.88f, CellHeight * 0.86f);
    DrawOutline(this, TriggerCenter - TriggerRect * 0.5f, TriggerRect,
        InterferenceColor, OutlineThickness * 1.7f);
    DrawDirectionArrow(this, TriggerCenter, FVector2D(0.0f, -1.0f), CellHeight * 0.30f,
        InterferenceColor, FMath::Max(2.0f, 2.8f * HudScale));
    const FVector2D TriggerLabelCenter = CellCenterOnScreen(FIntPoint(2, 6), BoardOrigin, CellWidth, CellHeight);
    DrawMapBadge(this, HudFont, TriggerLabelCenter, FVector2D(CellWidth * 1.60f, CellHeight * 0.48f),
        TEXT("干扰触发点"), HudScale, InterferenceColor);

    for (const NaiwaP7::FTowerSnapshot& Tower : Snapshot.Towers)
    {
        if (Tower.TargetObstacleId.IsNone())
        {
            continue;
        }
        const NaiwaP7::FObstacleSnapshot* TargetObstacle = FindObstacleSnapshot(
            Snapshot.Obstacles, Tower.TargetObstacleId);
        if (!TargetObstacle || TargetObstacle->bCleared)
        {
            continue;
        }
        const FVector2D TowerCenter = CellCenterOnScreen(Tower.Cell, BoardOrigin, CellWidth, CellHeight);
        const FVector2D ObstacleCenter = CellCenterOnScreen(
            TargetObstacle->Cell, BoardOrigin, CellWidth, CellHeight);
        DrawLine(TowerCenter.X, TowerCenter.Y, ObstacleCenter.X, ObstacleCenter.Y,
            FLinearColor(1.0f, 0.69f, 0.24f, 0.72f), 2.5f * HudScale);
    }

    for (const NaiwaP7::FObstacleSnapshot& Obstacle : Snapshot.Obstacles)
    {
        if (Obstacle.bCleared)
        {
            continue;
        }
        const FVector2D Center = CellCenterOnScreen(Obstacle.Cell, BoardOrigin, CellWidth, CellHeight);
        const FVector2D Size(CellWidth * 0.82f, CellHeight * 0.74f);
        const FVector2D Origin(Center.X - Size.X * 0.5f, Center.Y - Size.Y * 0.5f);
        const bool bIsTargeted = Snapshot.Towers.ContainsByPredicate([&Obstacle](
            const NaiwaP7::FTowerSnapshot& Tower)
        {
            return Tower.TargetObstacleId == Obstacle.ObstacleId;
        });
        DrawRect(ObstacleColor, Origin.X, Origin.Y, Size.X, Size.Y);
        DrawOutline(this, Origin, Size,
            bIsTargeted ? FLinearColor(1.0f, 0.69f, 0.24f, 1.0f) : FLinearColor(0.22f, 0.13f, 0.08f, 1.0f),
            bIsTargeted ? OutlineThickness * 2.2f : OutlineThickness);
        DrawLine(Origin.X + 4.0f * HudScale, Origin.Y + 3.0f * HudScale,
            Origin.X + Size.X - 4.0f * HudScale, Origin.Y + Size.Y - 3.0f * HudScale,
            FLinearColor(0.82f, 0.63f, 0.36f, 0.70f), HudScale);
        DrawLine(Origin.X + Size.X - 4.0f * HudScale, Origin.Y + 3.0f * HudScale,
            Origin.X + 4.0f * HudScale, Origin.Y + Size.Y - 3.0f * HudScale,
            FLinearColor(0.82f, 0.63f, 0.36f, 0.70f), HudScale);
        const FString BoxShortName = Obstacle.ObstacleId.ToString().EndsWith(TEXT("B"))
            ? TEXT("箱 B") : TEXT("箱 A");
        DrawCenteredText(this, HudFont, BoxShortName, Center,
            FLinearColor(1.0f, 0.92f, 0.77f, 1.0f), HudScale * 0.58f);
        const FVector2D HealthBarSize(CellWidth * 0.90f, 6.0f * HudScale);
        const FVector2D HealthBarOrigin(Center.X - HealthBarSize.X * 0.5f,
            Origin.Y - 9.0f * HudScale);
        DrawRect(FLinearColor(0.16f, 0.14f, 0.12f, 0.95f), HealthBarOrigin.X, HealthBarOrigin.Y,
            HealthBarSize.X, HealthBarSize.Y);
        const FNaiwaP7ObstacleRow* ObstacleRow = ObstacleRows.FindByPredicate(
            [&Obstacle](const FNaiwaP7ObstacleRow& Row) { return Row.ObstacleId == Obstacle.ObstacleId; });
        const float MaxHealth = ObstacleRow ? static_cast<float>(ObstacleRow->MaxHealth) :
            static_cast<float>(FMath::Max(1, Obstacle.Health));
        DrawRect(InterferenceColor, HealthBarOrigin.X, HealthBarOrigin.Y,
            HealthBarSize.X * FMath::Clamp(Obstacle.Health / MaxHealth, 0.0f, 1.0f), HealthBarSize.Y);
        DrawCenteredText(this, HudFont, FString::Printf(TEXT("%s  %d"),
            GetObstacleDisplayName(Obstacle.ObstacleId), Obstacle.Health),
            FVector2D(Center.X, HealthBarOrigin.Y - 5.0f * HudScale),
            bIsTargeted ? FLinearColor(1.0f, 0.86f, 0.55f, 1.0f) : MutedTextColor, HudScale * 0.52f);
    }

    for (const NaiwaP7::FTowerSnapshot& Tower : Snapshot.Towers)
    {
        const FVector2D Center = CellCenterOnScreen(Tower.Cell, BoardOrigin, CellWidth, CellHeight);
        const float TowerArtSize = CellWidth * 1.18f;
        const FVector2D TowerArtOrigin(Center.X - TowerArtSize * 0.5f, Center.Y - TowerArtSize * 0.5f);
        UTexture2D* TowerTexture = FindTowerTexture(Tower.TowerId, Tower.Level);
        if (TowerTexture)
        {
            DrawTowerTexture(TowerTexture, TowerArtOrigin, FVector2D(TowerArtSize, TowerArtSize));
        }
        else
        {
            const FVector2D FallbackSize(CellWidth * 0.66f, CellHeight * 0.70f);
            DrawRect(TowerColor, Center.X - FallbackSize.X * 0.5f, Center.Y - FallbackSize.Y * 0.5f,
                FallbackSize.X, FallbackSize.Y);
        }

        const bool bTowerSelected = P7Controller && P7Controller->HasSelectedTowerCell() &&
            P7Controller->GetSelectedTowerCell() == Tower.Cell;
        const FVector2D TowerOutlineSize(CellWidth * 1.06f, CellHeight * 1.02f);
        DrawOutline(this, Center - TowerOutlineSize * 0.5f, TowerOutlineSize,
            Tower.InterferenceStepsRemaining > 0 ? InterferenceColor :
                (bTowerSelected ? BuildSiteColor : FLinearColor(0.16f, 0.20f, 0.21f, 0.92f)),
            bTowerSelected ? OutlineThickness * 1.8f : OutlineThickness);
        DrawMapBadge(this, HudFont, FVector2D(Center.X, Center.Y + CellHeight * 0.34f),
            FVector2D(CellWidth * 0.54f, CellHeight * 0.34f),
            FString::Printf(TEXT("%d级"), Tower.Level), HudScale, BuildSiteColor);
        if (!Tower.TargetObstacleId.IsNone())
        {
            DrawMapBadge(this, HudFont, FVector2D(Center.X, Center.Y + CellHeight * 0.67f),
                FVector2D(CellWidth * 1.30f, CellHeight * 0.34f),
                FString::Printf(TEXT("目标：箱%s"), Tower.TargetObstacleId.ToString().EndsWith(TEXT("B"))
                    ? TEXT("B") : TEXT("A")), HudScale, ObstacleColor);
        }
        if (Tower.InterferenceStepsRemaining > 0)
        {
            DrawMapBadge(this, HudFont, FVector2D(Center.X, Center.Y - TowerArtSize * 0.48f),
                FVector2D(CellWidth * 1.10f, CellHeight * 0.36f),
                FString::Printf(TEXT("受扰 %.1f秒"), Tower.InterferenceStepsRemaining / 60.0f),
                HudScale, InterferenceColor);
        }
    }

    for (const NaiwaP7::FEnemySnapshot& Enemy : Snapshot.Enemies)
    {
        const FVector2D Center = GridPositionOnScreen(Enemy.GridPosition, BoardOrigin, CellWidth, CellHeight);
        const float Width = CellWidth * 0.50f;
        const float Height = CellHeight * 0.60f;
        const FVector2D Origin(Center.X - Width * 0.5f, Center.Y - Height * 0.5f);
        DrawRect(EnemyColor, Origin.X, Origin.Y, Width, Height);
        DrawOutline(this, Origin, FVector2D(Width, Height),
            Enemy.EnemyId == FName(TEXT("P7_Enemy_Jammer")) ? InterferenceColor :
                FLinearColor(0.22f, 0.10f, 0.16f, 1.0f), HudScale * 1.4f);
        DrawCenteredText(this, HudFont, GetEnemyMarker(Enemy.EnemyId), Center,
            TextColor, HudScale * 0.62f);
        DrawMapBadge(this, HudFont, FVector2D(Center.X, Center.Y - Height * 0.68f),
            FVector2D(CellWidth * 0.42f, CellHeight * 0.32f), FString::Printf(TEXT("%d"), Enemy.Health),
            HudScale, InterferenceColor);
    }

    DrawVFX(BoardOrigin, CellWidth, CellHeight, CurrentTime);

    FIntPoint HoveredCell;
    const bool bHasHoveredCell = P7Controller && P7Controller->TryGetHoveredCell(HoveredCell);
    if (bHasHoveredCell)
    {
        const bool bIsVisibleBuildSite = Level.VisibleBuildCells.Contains(HoveredCell);
        const bool bIsUnlockedHiddenBuildSite = Level.HiddenBuildSites.ContainsByPredicate(
            [&HoveredCell, &Snapshot, &Level](const NaiwaP7::FHiddenBuildSite& Site)
            {
                return Site.Cell == HoveredCell && Snapshot.Obstacles.ContainsByPredicate(
                    [&Site](const NaiwaP7::FObstacleSnapshot& Obstacle)
                    {
                        return Obstacle.ObstacleId == Site.UnlockObstacleId && Obstacle.bHiddenBuildSiteUnlocked;
                    });
            });
        const bool bIsOccupied = Snapshot.Towers.ContainsByPredicate(
            [&HoveredCell](const NaiwaP7::FTowerSnapshot& Tower) { return Tower.Cell == HoveredCell; });
        if ((bIsVisibleBuildSite || bIsUnlockedHiddenBuildSite) && !bIsOccupied)
        {
            const FVector2D Center = CellCenterOnScreen(HoveredCell, BoardOrigin, CellWidth, CellHeight);
            DrawOutline(this, FVector2D(Center.X - CellWidth * 0.49f, Center.Y - CellHeight * 0.49f),
                FVector2D(CellWidth * 0.98f, CellHeight * 0.98f),
                FLinearColor(1.0f, 1.0f, 0.95f, 0.95f), FMath::Max(1.0f, 2.0f * HudScale));
        }
    }

    const float FooterY = BottomPanelTop + Margin * 0.5f;
    DrawText(TEXT("地图：青色 A 直行  ·  橙色 B 绕行  ·  绿色“建造”可放塔  ·  木箱可清除并解锁新炮位"),
        MutedTextColor, Margin, FooterY, HudFont, HudScale * 0.66f);
    DrawText(TEXT("鼠标：点卡片选塔、点“建造”放置；点塔后再点障碍指定目标；右键取消。快捷键：1-3选塔，N开波，A/B切路，空格暂停，U升级，C恢复索敌。"),
        MutedTextColor, Margin, FooterY + 20.0f * HudScale, HudFont, HudScale * 0.62f);

    FString PhaseHint;
    if (bPreparing)
    {
        PhaseHint = FString::Printf(TEXT("布防中：可以调整 A/B 路线并放塔，然后点击“开始第 %d 波”。"),
            Snapshot.CurrentWave + 1);
    }
    else if (bWaveActive)
    {
        PhaseHint = TEXT("防守中：路线已锁定；干扰敌经过红色标记时会暂时封锁附近炮塔。可点击“暂停”整理布局。");
    }
    else if (Snapshot.Phase == NaiwaP7::ESimulationPhase::Won)
    {
        PhaseHint = TEXT("全部波次已守住。清障与奶罐无伤是两项独立挑战。");
    }
    else if (Snapshot.Phase == NaiwaP7::ESimulationPhase::Lost)
    {
        PhaseHint = TEXT("奶罐耐久耗尽。本局结束，点击中央按钮重新布防。");
    }
    else
    {
        PhaseHint = TEXT("安排炮塔后，点击“开始第1波”。");
    }
    DrawText(PhaseHint, TextColor, Margin, FooterY + 41.0f * HudScale,
        HudFont, HudScale * 0.62f);

    const NaiwaP7::FTowerSnapshot* SelectedTower = nullptr;
    if (P7Controller && P7Controller->HasSelectedTowerCell())
    {
        SelectedTower = Snapshot.Towers.FindByPredicate([P7Controller](const NaiwaP7::FTowerSnapshot& Tower)
        {
            return Tower.Cell == P7Controller->GetSelectedTowerCell();
        });
    }

    if (SelectedTower)
    {
        const FNaiwaP7TowerRow* NextLevelRow = TowerRows.FindByPredicate([SelectedTower](const FNaiwaP7TowerRow& Row)
        {
            return Row.TowerId == SelectedTower->TowerId && Row.Level == SelectedTower->Level + 1;
        });
        const FString UpgradeLabel = NextLevelRow
            ? FString::Printf(TEXT("升级需要 %d 金币"), NextLevelRow->CostCoins)
            : TEXT("已达到最高等级");
        const FString TargetLabel = SelectedTower->TargetObstacleId.IsNone()
            ? TEXT("普通索敌") : FString::Printf(TEXT("目标：%s"), GetObstacleDisplayName(SelectedTower->TargetObstacleId));
        DrawText(FString::Printf(TEXT("当前选择：%s %d级  ·  %s  ·  %s"),
            GetTowerDisplayName(SelectedTower->TowerId), SelectedTower->Level, *UpgradeLabel, *TargetLabel),
            TextColor, Margin, FooterY + 63.0f * HudScale, HudFont, HudScale * 0.60f);

        DrawButton(this, HudFont, GetSelectionButtonRect(ViewportSize,
            ENaiwaP7HudAction::UpgradeSelectedTower), NextLevelRow ? TEXT("升级") : TEXT("已满级"),
            HudScale * 0.58f, BuildSiteColor, NextLevelRow != nullptr);
        DrawButton(this, HudFont, GetSelectionButtonRect(ViewportSize,
            ENaiwaP7HudAction::ClearSelectedTowerTarget), TEXT("恢复普通索敌"),
            HudScale * 0.52f, ObstacleColor, !SelectedTower->TargetObstacleId.IsNone());
    }

    if (P7Controller && !P7Controller->GetInteractionMessage().IsEmpty())
    {
        DrawText(FString::Printf(TEXT("提示：%s"), *P7Controller->GetInteractionMessage()),
            TextColor, Margin, FooterY + (SelectedTower ? 80.0f : 63.0f) * HudScale,
            HudFont, HudScale * 0.62f);
    }

    if (bHasHoveredCell)
    {
        FString CellMeaning = TEXT("普通地面：不能建造");
        if (Snapshot.Towers.ContainsByPredicate([&HoveredCell](const NaiwaP7::FTowerSnapshot& Tower)
            { return Tower.Cell == HoveredCell; }))
        {
            CellMeaning = TEXT("已建炮塔：点击可查看升级与目标操作");
        }
        else if (const NaiwaP7::FObstacleSnapshot* HoveredObstacle = Snapshot.Obstacles.FindByPredicate(
            [&HoveredCell](const NaiwaP7::FObstacleSnapshot& Obstacle)
            { return Obstacle.Cell == HoveredCell && !Obstacle.bCleared; }))
        {
            const FNaiwaP7ObstacleRow* ObstacleRow = ObstacleRows.FindByPredicate(
                [HoveredObstacle](const FNaiwaP7ObstacleRow& Row)
                { return Row.ObstacleId == HoveredObstacle->ObstacleId; });
            CellMeaning = FString::Printf(TEXT("%s：耐久 %d，清除奖励 %d 金币并解锁建造位"),
                GetObstacleDisplayName(HoveredObstacle->ObstacleId), HoveredObstacle->Health,
                ObstacleRow ? ObstacleRow->RewardCoins : 0);
        }
        else if (Level.VisibleBuildCells.Contains(HoveredCell) || Level.HiddenBuildSites.ContainsByPredicate(
            [&HoveredCell, &Snapshot](const NaiwaP7::FHiddenBuildSite& Site)
            {
                if (Site.Cell != HoveredCell)
                {
                    return false;
                }
                return Snapshot.Obstacles.ContainsByPredicate([&Site](const NaiwaP7::FObstacleSnapshot& Obstacle)
                {
                    return Obstacle.ObstacleId == Site.UnlockObstacleId && Obstacle.bHiddenBuildSiteUnlocked;
                });
            }))
        {
            CellMeaning = TEXT("建造位：可放置上方选择的炮塔");
        }
        else if (Level.RouteA.Cells.Contains(HoveredCell) || Level.RouteB.Cells.Contains(HoveredCell))
        {
            CellMeaning = TEXT("行进路线：不可建造");
        }
        DrawText(FString::Printf(TEXT("鼠标位置  %d,%d  ·  %s"), HoveredCell.X, HoveredCell.Y, *CellMeaning),
            MutedTextColor, Margin, FooterY + (SelectedTower ? 101.0f : 82.0f) * HudScale,
            HudFont, HudScale * 0.54f);
    }

    if (bPaused && bWaveActive)
    {
        const FHudRect PausePanel{ FVector2D(ViewportSize.X * 0.5f - 120.0f * HudScale,
                ViewportSize.Y * 0.5f - 38.0f * HudScale),
            FVector2D(240.0f * HudScale, 76.0f * HudScale) };
        DrawRect(FLinearColor(0.03f, 0.05f, 0.06f, 0.76f), PausePanel.Origin.X,
            PausePanel.Origin.Y, PausePanel.Size.X, PausePanel.Size.Y);
        DrawOutline(this, PausePanel.Origin, PausePanel.Size, BuildSiteColor, HudScale * 1.5f);
        DrawCenteredText(this, HudFont, TEXT("已暂停"),
            PausePanel.Origin + PausePanel.Size * 0.5f, TextColor, HudScale * 1.08f);
    }

    if (bTerminal)
    {
        DrawRect(FLinearColor(0.015f, 0.025f, 0.035f, 0.68f),
            0.0f, 0.0f, ViewportSize.X, ViewportSize.Y);
        const FHudRect ResultPanel{ FVector2D(ViewportSize.X * 0.5f - 240.0f * HudScale,
                ViewportSize.Y * 0.5f - 114.0f * HudScale),
            FVector2D(480.0f * HudScale, 228.0f * HudScale) };
        DrawRect(HudPanelColor, ResultPanel.Origin.X, ResultPanel.Origin.Y,
            ResultPanel.Size.X, ResultPanel.Size.Y);
        DrawOutline(this, ResultPanel.Origin, ResultPanel.Size,
            Snapshot.Phase == NaiwaP7::ESimulationPhase::Won ? BuildSiteColor : InterferenceColor,
            HudScale * 1.8f);
        const bool bWon = Snapshot.Phase == NaiwaP7::ESimulationPhase::Won;
        UTexture2D* ResultPortrait = bWon ? CatalogVictoryPortrait : CatalogDistressPortrait;
        const float PortraitSize = 88.0f * HudScale;
        DrawTowerTexture(ResultPortrait,
            FVector2D(ResultPanel.Origin.X + 16.0f * HudScale,
                ResultPanel.Origin.Y + (ResultPanel.Size.Y - PortraitSize) * 0.5f),
            FVector2D(PortraitSize, PortraitSize));
        const float BadgeSize = 64.0f * HudScale;
        DrawResultBadge(bWon ? 0 : 1,
            FVector2D(ResultPanel.Origin.X + 116.0f * HudScale,
                ResultPanel.Origin.Y + 15.0f * HudScale),
            FVector2D(BadgeSize, BadgeSize));
        const float ResultTextCenterX = ResultPanel.Origin.X + ResultPanel.Size.X * 0.67f;
        DrawCenteredText(this, HudFont,
            bWon ? TEXT("守住了！奶罐平安") : TEXT("奶罐被打破了"),
            FVector2D(ResultTextCenterX, ResultPanel.Origin.Y + 48.0f * HudScale),
            bWon ? BuildSiteColor : InterferenceColor, HudScale * 1.10f);
        DrawCenteredText(this, HudFont,
            bWon ? TEXT("八波敌人已全部击退。") : TEXT("重新布防后再试一次。"),
            FVector2D(ResultTextCenterX, ResultPanel.Origin.Y + 89.0f * HudScale),
            TextColor, HudScale * 0.70f);
        const FString GoalSummary = FString::Printf(TEXT("奶罐无伤：%s    清障：%s  %d / %d"),
            Snapshot.bMilkJarFull ? TEXT("达成") : TEXT("未达成"),
            ClearedObstacleCount == Snapshot.Obstacles.Num() ? TEXT("完成") : TEXT("未完成"),
            ClearedObstacleCount, Snapshot.Obstacles.Num());
        DrawCenteredText(this, HudFont, GoalSummary,
            FVector2D(ResultTextCenterX, ResultPanel.Origin.Y + 126.0f * HudScale),
            MutedTextColor, HudScale * 0.62f);
        DrawButton(this, HudFont, GetRestartButtonRect(ViewportSize), TEXT("再来一局"),
            HudScale * 0.72f, BuildSiteColor);
    }
}
