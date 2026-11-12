#pragma once

#include "CoreMinimal.h"
#include "Core/NaiwaP7Simulation.h"
#include "GameFramework/HUD.h"
#include "NaiwaP7GrayboxHUD.generated.h"

class UTexture2D;

enum class ENaiwaP7HudAction : uint8
{
    None,
    TowerSlot1,
    TowerSlot2,
    TowerSlot3,
    BeginWave,
    SelectRouteA,
    SelectRouteB,
    TogglePause,
    UpgradeSelectedTower,
    ClearSelectedTowerTarget,
    RestartRun
};

/** Temporary, read-only Canvas renderer for validating the P7 board and HUD layout. */
UCLASS()
class NAIWAP7RUNTIME_API ANaiwaP7GrayboxHUD final : public AHUD
{
    GENERATED_BODY()

public:
    ANaiwaP7GrayboxHUD();
    virtual void DrawHUD() override;

    /** Fits the 20x11 board within a 92% width-safe area, leaving room for entry and goal markers. */
    static bool TryCalculateBoardRect(
        const FVector2D& ViewportSize,
        FVector2D& OutOrigin,
        FVector2D& OutSize);

    static bool TryHitTestInteractiveAction(
        const FVector2D& ViewportSize,
        const FVector2D& ScreenPosition,
        ENaiwaP7HudAction& OutAction,
        bool bHasTowerSelection,
        bool bIsTerminalPhase);

private:
    enum class EVFXPlaybackKind : uint8
    {
        Shot,
        DogShot,
        MouseShot,
        Clear,
        Upgrade,
        Jam,
        JamRecovery
    };

    struct FActiveVFX
    {
        EVFXPlaybackKind Kind = EVFXPlaybackKind::Shot;
        double StartTime = 0.0;
        FVector2D StartGridPosition = FVector2D::ZeroVector;
        FVector2D EndGridPosition = FVector2D::ZeroVector;
        float Duration = 0.5f;
        bool bObstacleCleared = false;
    };

    void CaptureVFXEvents(
        const NaiwaP7::FSimulationSnapshot& Snapshot,
        const NaiwaP7::FLevelDefinition& Level,
        const TArray<FNaiwaP7TowerRow>& TowerRows,
        double CurrentTime);
    void DrawVFX(
        const FVector2D& BoardOrigin,
        float CellWidth,
        float CellHeight,
        double CurrentTime);
    UTexture2D* FindTowerTexture(FName TowerId, int32 Level) const;
    void DrawTowerTexture(UTexture2D* Texture, const FVector2D& Origin, const FVector2D& Size);
    void DrawGroundBoardTexture(const FVector2D& Origin, const FVector2D& Size);
    void DrawHudIcon(int32 AtlasCellIndex, const FVector2D& Origin, const FVector2D& Size,
        const FLinearColor& Tint = FLinearColor::White);
    void DrawResultBadge(int32 AtlasCellIndex, const FVector2D& Origin, const FVector2D& Size);
    void DrawForkGateTile(int32 AtlasCellIndex, const FVector2D& Origin, const FVector2D& Size);

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> GroundBoardTexture = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> GuardianEggTexture = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> VFXAtlas = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> TowerProjectileVFXAtlas = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> HUDIconAtlas = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> ResultBadgeAtlas = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> ForkGateAtlas = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> CatalogIdlePortrait = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> CatalogLaughPortrait = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> CatalogVictoryPortrait = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> CatalogDistressPortrait = nullptr;

    /** A1 tower textures in type-major, level-minor order: Chicken, Dog, Mouse. */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> A1TowerTextures;

    bool bHasPreviousSnapshot = false;
    NaiwaP7::FSimulationSnapshot PreviousSnapshot;
    TArray<FActiveVFX> ActiveVFX;
    TSet<FIntPoint> TowersAwaitingRecoveryShot;
    int32 RecentCoinGain = 0;
    double RecentCoinGainExpiry = 0.0;
};
