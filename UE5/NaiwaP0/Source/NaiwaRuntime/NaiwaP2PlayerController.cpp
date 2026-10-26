#include "NaiwaP2PlayerController.h"

#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/InputSettings.h"
#include "InputCoreTypes.h"
#include "NaiwaP2GameMode.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    constexpr double CentimetersPerMeter = 100.0;
    constexpr double DragDistanceForFullPowerMeters = 3.2;
    constexpr double MinimumDragDistanceMeters = 0.35;
    constexpr double LauncherXCentimeters = 0.0;
    constexpr double LauncherZCentimeters = 180.0;
    constexpr double LauncherGrabRadiusCentimeters = 240.0;
}

ANaiwaP2PlayerController::ANaiwaP2PlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ANaiwaP2PlayerController::BeginPlay()
{
    Super::BeginPlay();
    bShowMouseCursor = true;
    bEnableClickEvents = false;
    bEnableMouseOverEvents = false;
    FInputModeGameAndUI InputMode;
    // The slingshot reads the absolute cursor position each tick while the
    // button is held. Locking the cursor on capture freezes that coordinate in
    // a standalone game window, so keep it free during drag input.
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);
}

void ANaiwaP2PlayerController::PlayerTick(const float DeltaSeconds)
{
    Super::PlayerTick(DeltaSeconds);
    if (bIsAimDragging)
    {
        UpdateAimFromCursor();
    }
}

void ANaiwaP2PlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (!InputComponent)
    {
        return;
    }
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this,
        &ANaiwaP2PlayerController::StartAimDrag);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this,
        &ANaiwaP2PlayerController::FinishAimDrag);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this,
        &ANaiwaP2PlayerController::RetryLevel);
    InputComponent->BindKey(EKeys::One, IE_Pressed, this,
        &ANaiwaP2PlayerController::SelectEggSlot1);
    InputComponent->BindKey(EKeys::Two, IE_Pressed, this,
        &ANaiwaP2PlayerController::SelectEggSlot2);
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this,
        &ANaiwaP2PlayerController::SelectEggSlot3);
    InputComponent->BindKey(EKeys::Four, IE_Pressed, this,
        &ANaiwaP2PlayerController::SelectEggSlot4);
    InputComponent->BindKey(EKeys::Five, IE_Pressed, this,
        &ANaiwaP2PlayerController::SelectEggSlot5);
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this,
        &ANaiwaP2PlayerController::ReturnToLevelSelect);
}

void ANaiwaP2PlayerController::ReturnToLevelSelect()
{
    UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Naiwa/UI/L_NaiwaMainMenu")),
        true, TEXT("NaiwaMenuPage=levels"));
}

void ANaiwaP2PlayerController::StartAimDrag()
{
    ANaiwaP2GameMode* Mode = Cast<ANaiwaP2GameMode>(UGameplayStatics::GetGameMode(this));
    if (Mode && Mode->GetPlayState() == ENaiwaP2PlayState::Flying)
    {
        Mode->ActivateFlightSkill();
        return;
    }
    FVector CursorPoint;
    if (!Mode || Mode->GetPlayState() != ENaiwaP2PlayState::Aiming ||
        !CursorOnLaunchPlane(CursorPoint))
    {
        return;
    }

    const FVector Launcher(LauncherXCentimeters, 0.0, LauncherZCentimeters);
    const double DX = CursorPoint.X - Launcher.X;
    const double DZ = CursorPoint.Z - Launcher.Z;
    if (DX * DX + DZ * DZ > FMath::Square(LauncherGrabRadiusCentimeters))
    {
        return;
    }

    DragStartPoint = FVector2D(CursorPoint.X, CursorPoint.Z);
    DraggedPouchWorldLocation = Launcher;
    CurrentAngleDegrees = 45.0;
    CurrentPower = 0.0;
    bIsAimDragging = true;
    bHasValidAimDrag = false;
    Mode->BeginArcherDraw();
    Mode->SetPouchDragWorldLocation(Launcher);
    Mode->SetAimPreview(CurrentAngleDegrees, 0.0);
    UpdateAimFromCursor();
}

void ANaiwaP2PlayerController::SelectEggSlot1()
{
    if (ANaiwaP2GameMode* Mode = Cast<ANaiwaP2GameMode>(UGameplayStatics::GetGameMode(this))) Mode->SelectEggByIndex(0);
}

void ANaiwaP2PlayerController::SelectEggSlot2()
{
    if (ANaiwaP2GameMode* Mode = Cast<ANaiwaP2GameMode>(UGameplayStatics::GetGameMode(this))) Mode->SelectEggByIndex(1);
}

void ANaiwaP2PlayerController::SelectEggSlot3()
{
    if (ANaiwaP2GameMode* Mode = Cast<ANaiwaP2GameMode>(UGameplayStatics::GetGameMode(this))) Mode->SelectEggByIndex(2);
}

void ANaiwaP2PlayerController::SelectEggSlot4()
{
    if (ANaiwaP2GameMode* Mode = Cast<ANaiwaP2GameMode>(UGameplayStatics::GetGameMode(this))) Mode->SelectEggByIndex(3);
}

void ANaiwaP2PlayerController::SelectEggSlot5()
{
    if (ANaiwaP2GameMode* Mode = Cast<ANaiwaP2GameMode>(UGameplayStatics::GetGameMode(this))) Mode->SelectEggByIndex(4);
}

void ANaiwaP2PlayerController::FinishAimDrag()
{
    if (!bIsAimDragging)
    {
        return;
    }
    UpdateAimFromCursor();
    FVector FinalCursorPoint = FVector::ZeroVector;
    const bool bFinalCursorOnLaunchPlane = CursorOnLaunchPlane(FinalCursorPoint);
    UE_LOG(LogTemp, Display,
        TEXT("NAIWA_P2_AIM_DRAG_RELEASE valid=%d power=%.3f angle=%.2f plane=%d start_cm=(%.1f,%.1f) cursor_cm=(%.1f,%.1f) pouch_cm=(%.1f,%.1f)"),
        bHasValidAimDrag ? 1 : 0, CurrentPower, CurrentAngleDegrees, bFinalCursorOnLaunchPlane ? 1 : 0,
        DragStartPoint.X, DragStartPoint.Y, FinalCursorPoint.X, FinalCursorPoint.Z,
        DraggedPouchWorldLocation.X, DraggedPouchWorldLocation.Z);
    bIsAimDragging = false;

    if (ANaiwaP2GameMode* Mode = Cast<ANaiwaP2GameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (bHasValidAimDrag && CurrentPower >= 0.05)
        {
            if (Mode->TryFire(CurrentAngleDegrees, CurrentPower))
            {
                Mode->ReleaseArcherDraw();
            }
            else
            {
                Mode->CancelArcherDraw();
            }
        }
        else
        {
            Mode->CancelArcherDraw();
            Mode->ShowActionNotice(TEXT("NO SHOT - PULL LEFT AND DOWN TO BUILD POWER"), 1.4);
            Mode->SetAimPreview(CurrentAngleDegrees, 0.0);
        }
        Mode->ReleasePouchToLauncher();
    }
    bHasValidAimDrag = false;
}

void ANaiwaP2PlayerController::UpdateAimFromCursor()
{
    FVector CursorPoint;
    if (!CursorOnLaunchPlane(CursorPoint))
    {
        return;
    }

    double LaunchDX = (DragStartPoint.X - CursorPoint.X) / CentimetersPerMeter;
    double LaunchDZ = (DragStartPoint.Y - CursorPoint.Z) / CentimetersPerMeter;
    double PullDistance = FMath::Sqrt(LaunchDX * LaunchDX + LaunchDZ * LaunchDZ);
    if (PullDistance > DragDistanceForFullPowerMeters)
    {
        const double Scale = DragDistanceForFullPowerMeters / PullDistance;
        LaunchDX *= Scale;
        LaunchDZ *= Scale;
        PullDistance = DragDistanceForFullPowerMeters;
    }

    DraggedPouchWorldLocation = FVector(
        LauncherXCentimeters + LaunchDX * CentimetersPerMeter,
        0.0,
        LauncherZCentimeters + LaunchDZ * CentimetersPerMeter);
    if (ANaiwaP2GameMode* Mode = Cast<ANaiwaP2GameMode>(UGameplayStatics::GetGameMode(this)))
    {
        Mode->SetPouchDragWorldLocation(DraggedPouchWorldLocation);
    }

    if (PullDistance < MinimumDragDistanceMeters || LaunchDX <= 0.0)
    {
        CurrentPower = 0.0;
        bHasValidAimDrag = false;
        if (ANaiwaP2GameMode* Mode = Cast<ANaiwaP2GameMode>(UGameplayStatics::GetGameMode(this)))
        {
            Mode->SetArcherDrawProgress(0.0f);
        }
        if (ANaiwaP2GameMode* Mode = Cast<ANaiwaP2GameMode>(UGameplayStatics::GetGameMode(this)))
        {
            Mode->SetAimPreview(CurrentAngleDegrees, 0.0);
        }
        return;
    }

    constexpr double RadiansToDegrees = 57.2957795130823208768;
    CurrentAngleDegrees = FMath::Clamp(FMath::Atan2(LaunchDZ, LaunchDX) * RadiansToDegrees, 1.0, 89.0);
    CurrentPower = FMath::Clamp(PullDistance / DragDistanceForFullPowerMeters, 0.05, 1.0);
    bHasValidAimDrag = true;
    if (ANaiwaP2GameMode* Mode = Cast<ANaiwaP2GameMode>(UGameplayStatics::GetGameMode(this)))
    {
        Mode->SetArcherDrawProgress(static_cast<float>(CurrentPower));
        Mode->SetAimPreview(CurrentAngleDegrees, CurrentPower);
    }
}

void ANaiwaP2PlayerController::RetryLevel()
{
    if (ANaiwaP2GameMode* Mode = Cast<ANaiwaP2GameMode>(UGameplayStatics::GetGameMode(this)))
    {
        bIsAimDragging = false;
        bHasValidAimDrag = false;
        CurrentPower = 0.0;
        Mode->Retry();
    }
}

bool ANaiwaP2PlayerController::CursorOnLaunchPlane(FVector& OutPoint) const
{
    FVector RayOrigin;
    FVector RayDirection;
    if (!DeprojectMousePositionToWorld(RayOrigin, RayDirection) || FMath::IsNearlyZero(RayDirection.Y))
    {
        return false;
    }
    const double Distance = -RayOrigin.Y / RayDirection.Y;
    if (Distance < 0.0)
    {
        return false;
    }
    OutPoint = RayOrigin + RayDirection * Distance;
    return FMath::IsFinite(OutPoint.X) && FMath::IsFinite(OutPoint.Z);
}
