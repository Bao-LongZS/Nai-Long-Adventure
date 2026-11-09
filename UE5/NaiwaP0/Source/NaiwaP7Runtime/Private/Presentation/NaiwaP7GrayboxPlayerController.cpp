#include "Presentation/NaiwaP7GrayboxPlayerController.h"

#include "Components/InputComponent.h"
#include "Core/NaiwaP7Board.h"
#include "Core/NaiwaP7Simulation.h"
#include "Core/NaiwaP7TowerDefenseSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Presentation/NaiwaP7GrayboxHUD.h"

ANaiwaP7GrayboxPlayerController::ANaiwaP7GrayboxPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
}

void ANaiwaP7GrayboxPlayerController::BeginPlay()
{
    Super::BeginPlay();
    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);
    AddRuntimeInputContext();
}

void ANaiwaP7GrayboxPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveRuntimeInputContext();
    Super::EndPlay(EndPlayReason);
}

void ANaiwaP7GrayboxPlayerController::SetupInputComponent()
{
    // Seed the component before Super so UE binds default input delegates on the P7 Enhanced component.
    if (!Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (InputComponent)
        {
            InputComponent->DestroyComponent();
        }
        InputComponent = NewObject<UEnhancedInputComponent>(this, TEXT("P7_EnhancedInputComponent"));
        InputComponent->RegisterComponent();
    }

    Super::SetupInputComponent();
    CreateRuntimeInputContext();

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInput)
    {
        UE_LOG(LogTemp, Error, TEXT("P7 graybox input requires EnhancedInputComponent."));
        return;
    }

    EnhancedInput->BindAction(PrimaryClickAction, ETriggerEvent::Started, this, &ThisClass::HandlePrimaryClick);
    EnhancedInput->BindAction(SecondaryClickAction, ETriggerEvent::Started, this, &ThisClass::HandleSecondaryClick);
    EnhancedInput->BindAction(SelectTowerOneAction, ETriggerEvent::Started, this, &ThisClass::HandleSelectTowerOne);
    EnhancedInput->BindAction(SelectTowerTwoAction, ETriggerEvent::Started, this, &ThisClass::HandleSelectTowerTwo);
    EnhancedInput->BindAction(SelectTowerThreeAction, ETriggerEvent::Started, this, &ThisClass::HandleSelectTowerThree);
    EnhancedInput->BindAction(UpgradeAction, ETriggerEvent::Started, this, &ThisClass::HandleUpgrade);
    EnhancedInput->BindAction(ClearTowerTargetAction, ETriggerEvent::Started, this, &ThisClass::HandleClearTowerTarget);
    EnhancedInput->BindAction(BeginWaveAction, ETriggerEvent::Started, this, &ThisClass::HandleBeginWave);
    EnhancedInput->BindAction(TogglePauseAction, ETriggerEvent::Started, this, &ThisClass::HandleTogglePause);
    EnhancedInput->BindAction(SelectRouteAAction, ETriggerEvent::Started, this, &ThisClass::HandleSelectRouteA);
    EnhancedInput->BindAction(SelectRouteBAction, ETriggerEvent::Started, this, &ThisClass::HandleSelectRouteB);
}

bool ANaiwaP7GrayboxPlayerController::TryGetHoveredCell(FIntPoint& OutCell) const
{
    float MouseX = 0.0f;
    float MouseY = 0.0f;
    if (!GetMousePosition(MouseX, MouseY))
    {
        return false;
    }

    int32 ViewportWidth = 0;
    int32 ViewportHeight = 0;
    GetViewportSize(ViewportWidth, ViewportHeight);
    FVector2D BoardOrigin;
    FVector2D BoardSize;
    if (!ANaiwaP7GrayboxHUD::TryCalculateBoardRect(
        FVector2D(static_cast<float>(ViewportWidth), static_cast<float>(ViewportHeight)),
        BoardOrigin,
        BoardSize))
    {
        return false;
    }

    return NaiwaP7::TryMapBoardPointToCell(
        FVector2D(MouseX, MouseY) - BoardOrigin,
        BoardSize,
        OutCell);
}

void ANaiwaP7GrayboxPlayerController::CreateRuntimeInputContext()
{
    if (RuntimeInputContext)
    {
        return;
    }

    RuntimeInputContext = NewObject<UInputMappingContext>(this, TEXT("IMC_P7_Graybox"));
    auto MakeBooleanAction = [this](const TCHAR* ActionName)
    {
        UInputAction* Action = NewObject<UInputAction>(this, ActionName);
        Action->ValueType = EInputActionValueType::Boolean;
        return Action;
    };

    PrimaryClickAction = MakeBooleanAction(TEXT("IA_P7_PrimaryClick"));
    SecondaryClickAction = MakeBooleanAction(TEXT("IA_P7_SecondaryClick"));
    SelectTowerOneAction = MakeBooleanAction(TEXT("IA_P7_SelectTower1"));
    SelectTowerTwoAction = MakeBooleanAction(TEXT("IA_P7_SelectTower2"));
    SelectTowerThreeAction = MakeBooleanAction(TEXT("IA_P7_SelectTower3"));
    UpgradeAction = MakeBooleanAction(TEXT("IA_P7_Upgrade"));
    ClearTowerTargetAction = MakeBooleanAction(TEXT("IA_P7_ClearTarget"));
    BeginWaveAction = MakeBooleanAction(TEXT("IA_P7_BeginWave"));
    TogglePauseAction = MakeBooleanAction(TEXT("IA_P7_TogglePause"));
    SelectRouteAAction = MakeBooleanAction(TEXT("IA_P7_RouteA"));
    SelectRouteBAction = MakeBooleanAction(TEXT("IA_P7_RouteB"));

    auto MapKey = [this](UInputAction* Action, const FKey& Key)
    {
        if (Action)
        {
            RuntimeInputContext->MapKey(Action, Key);
        }
    };

    MapKey(PrimaryClickAction, EKeys::LeftMouseButton);
    MapKey(SecondaryClickAction, EKeys::RightMouseButton);
    MapKey(SelectTowerOneAction, EKeys::One);
    MapKey(SelectTowerTwoAction, EKeys::Two);
    MapKey(SelectTowerThreeAction, EKeys::Three);
    MapKey(UpgradeAction, EKeys::U);
    MapKey(ClearTowerTargetAction, EKeys::C);
    MapKey(BeginWaveAction, EKeys::N);
    MapKey(TogglePauseAction, EKeys::SpaceBar);
    MapKey(SelectRouteAAction, EKeys::A);
    MapKey(SelectRouteBAction, EKeys::B);
}

void ANaiwaP7GrayboxPlayerController::AddRuntimeInputContext()
{
    if (bRuntimeInputContextAdded || !IsLocalController())
    {
        return;
    }

    CreateRuntimeInputContext();
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
        ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
        : nullptr;
    if (InputSubsystem && RuntimeInputContext)
    {
        InputSubsystem->AddMappingContext(RuntimeInputContext, 100);
        bRuntimeInputContextAdded = true;
    }
}

void ANaiwaP7GrayboxPlayerController::RemoveRuntimeInputContext()
{
    if (!bRuntimeInputContextAdded)
    {
        return;
    }

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
        ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
        : nullptr)
    {
        InputSubsystem->RemoveMappingContext(RuntimeInputContext);
    }
    bRuntimeInputContextAdded = false;
}

UNaiwaP7TowerDefenseSubsystem* ANaiwaP7GrayboxPlayerController::GetP7Subsystem() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UNaiwaP7TowerDefenseSubsystem>() : nullptr;
}

void ANaiwaP7GrayboxPlayerController::SelectTowerSlot(const int32 SlotIndex)
{
    UNaiwaP7TowerDefenseSubsystem* P7Subsystem = GetP7Subsystem();
    if (!P7Subsystem || !P7Subsystem->GetSimulation().HasLevel())
    {
        InteractionMessage = TEXT("Load a validated P7 level before selecting a tower.");
        return;
    }

    TArray<FName> TowerIds;
    for (const FNaiwaP7TowerRow& Row : P7Subsystem->GetSimulation().GetTowerRows())
    {
        if (Row.Level == 1 && !TowerIds.Contains(Row.TowerId))
        {
            TowerIds.Add(Row.TowerId);
        }
    }
    TowerIds.Sort([](const FName& A, const FName& B)
    {
        return A.Compare(B) < 0;
    });

    if (!TowerIds.IsValidIndex(SlotIndex))
    {
        InteractionMessage = TEXT("This tower slot has no loaded level-1 tower row.");
        return;
    }

    SelectedTowerId = TowerIds[SlotIndex];
    bHasSelectedTowerCell = false;
    InteractionMessage = FString::Printf(TEXT("Selected tower %s. Click an available build site."),
        *SelectedTowerId.ToString());
}

void ANaiwaP7GrayboxPlayerController::ClearSelection()
{
    SelectedTowerId = NAME_None;
    SelectedTowerCell = FIntPoint::ZeroValue;
    bHasSelectedTowerCell = false;
    InteractionMessage = TEXT("Selection cleared.");
}

void ANaiwaP7GrayboxPlayerController::HandlePrimaryClick(const FInputActionValue& Value)
{
    UNaiwaP7TowerDefenseSubsystem* P7Subsystem = GetP7Subsystem();
    if (!P7Subsystem || !P7Subsystem->GetSimulation().HasLevel())
    {
        InteractionMessage = TEXT("Load a validated P7 level before using the board.");
        return;
    }

    FIntPoint Cell;
    if (!TryGetHoveredCell(Cell))
    {
        InteractionMessage = TEXT("The pointer is outside the logical board.");
        return;
    }

    const NaiwaP7::FSimulationSnapshot Snapshot = P7Subsystem->GetSnapshot();
    const NaiwaP7::FTowerSnapshot* ClickedTower = Snapshot.Towers.FindByPredicate(
        [&Cell](const NaiwaP7::FTowerSnapshot& Tower)
        {
            return Tower.Cell == Cell;
        });
    if (ClickedTower)
    {
        SelectedTowerId = NAME_None;
        SelectedTowerCell = Cell;
        bHasSelectedTowerCell = true;
        InteractionMessage = FString::Printf(TEXT("Selected tower %s, level %d."),
            *ClickedTower->TowerId.ToString(), ClickedTower->Level);
        return;
    }

    const NaiwaP7::FObstacleSnapshot* ClickedObstacle = Snapshot.Obstacles.FindByPredicate(
        [&Cell](const NaiwaP7::FObstacleSnapshot& Obstacle)
        {
            return Obstacle.Cell == Cell && !Obstacle.bCleared;
        });
    if (ClickedObstacle)
    {
        if (!bHasSelectedTowerCell)
        {
            InteractionMessage = TEXT("Select a tower first, then click an obstacle to mark it.");
            return;
        }

        FString Error;
        if (P7Subsystem->TrySetTowerObstacleTarget(SelectedTowerCell, ClickedObstacle->ObstacleId, &Error))
        {
            InteractionMessage = FString::Printf(TEXT("Tower target set to %s."),
                *ClickedObstacle->ObstacleId.ToString());
        }
        else
        {
            InteractionMessage = Error;
        }
        return;
    }

    if (SelectedTowerId.IsNone())
    {
        InteractionMessage = bHasSelectedTowerCell
            ? TEXT("Tower selected. Choose 1-3 to build, or click an obstacle to set its target.")
            : TEXT("Choose tower 1-3, then click an available build site.");
        return;
    }

    FString Error;
    if (P7Subsystem->TryPlaceTower(Cell, SelectedTowerId, &Error))
    {
        SelectedTowerCell = Cell;
        bHasSelectedTowerCell = true;
        InteractionMessage = FString::Printf(TEXT("Placed %s. Tower selected for upgrade/target commands."),
            *SelectedTowerId.ToString());
        SelectedTowerId = NAME_None;
    }
    else
    {
        InteractionMessage = Error;
    }
}

void ANaiwaP7GrayboxPlayerController::HandleSecondaryClick(const FInputActionValue& Value)
{
    ClearSelection();
}

void ANaiwaP7GrayboxPlayerController::HandleSelectTowerOne(const FInputActionValue& Value)
{
    SelectTowerSlot(0);
}

void ANaiwaP7GrayboxPlayerController::HandleSelectTowerTwo(const FInputActionValue& Value)
{
    SelectTowerSlot(1);
}

void ANaiwaP7GrayboxPlayerController::HandleSelectTowerThree(const FInputActionValue& Value)
{
    SelectTowerSlot(2);
}

void ANaiwaP7GrayboxPlayerController::HandleUpgrade(const FInputActionValue& Value)
{
    UNaiwaP7TowerDefenseSubsystem* P7Subsystem = GetP7Subsystem();
    if (!P7Subsystem || !P7Subsystem->GetSimulation().HasLevel() || !bHasSelectedTowerCell)
    {
        InteractionMessage = TEXT("Select a placed tower before upgrading.");
        return;
    }

    FString Error;
    if (P7Subsystem->TryUpgradeTower(SelectedTowerCell, &Error))
    {
        InteractionMessage = TEXT("Tower upgraded.");
    }
    else
    {
        InteractionMessage = Error;
    }
}

void ANaiwaP7GrayboxPlayerController::HandleClearTowerTarget(const FInputActionValue& Value)
{
    UNaiwaP7TowerDefenseSubsystem* P7Subsystem = GetP7Subsystem();
    if (!P7Subsystem || !P7Subsystem->GetSimulation().HasLevel() || !bHasSelectedTowerCell)
    {
        InteractionMessage = TEXT("Select a placed tower before clearing its target.");
        return;
    }

    FString Error;
    if (P7Subsystem->TrySetTowerObstacleTarget(SelectedTowerCell, NAME_None, &Error))
    {
        InteractionMessage = TEXT("Tower target cleared; normal enemy targeting resumes.");
    }
    else
    {
        InteractionMessage = Error;
    }
}

void ANaiwaP7GrayboxPlayerController::HandleBeginWave(const FInputActionValue& Value)
{
    UNaiwaP7TowerDefenseSubsystem* P7Subsystem = GetP7Subsystem();
    if (!P7Subsystem || !P7Subsystem->GetSimulation().HasLevel())
    {
        InteractionMessage = TEXT("Load a validated P7 level before starting a wave.");
        return;
    }

    FString Error;
    if (P7Subsystem->TryBeginNextWave(&Error))
    {
        InteractionMessage = TEXT("Wave started.");
    }
    else
    {
        InteractionMessage = Error;
    }
}

void ANaiwaP7GrayboxPlayerController::HandleTogglePause(const FInputActionValue& Value)
{
    UNaiwaP7TowerDefenseSubsystem* P7Subsystem = GetP7Subsystem();
    if (!P7Subsystem)
    {
        InteractionMessage = TEXT("P7 simulation is unavailable.");
        return;
    }

    const bool bNewPaused = !P7Subsystem->IsPaused();
    P7Subsystem->SetPaused(bNewPaused);
    InteractionMessage = bNewPaused ? TEXT("Simulation paused.") : TEXT("Simulation resumed.");
}

void ANaiwaP7GrayboxPlayerController::HandleSelectRouteA(const FInputActionValue& Value)
{
    UNaiwaP7TowerDefenseSubsystem* P7Subsystem = GetP7Subsystem();
    FString Error;
    if (P7Subsystem && P7Subsystem->TrySelectRoute(NaiwaP7::ERoute::A, &Error))
    {
        InteractionMessage = TEXT("Route A selected.");
    }
    else
    {
        InteractionMessage = P7Subsystem ? Error : TEXT("P7 simulation is unavailable.");
    }
}

void ANaiwaP7GrayboxPlayerController::HandleSelectRouteB(const FInputActionValue& Value)
{
    UNaiwaP7TowerDefenseSubsystem* P7Subsystem = GetP7Subsystem();
    FString Error;
    if (P7Subsystem && P7Subsystem->TrySelectRoute(NaiwaP7::ERoute::B, &Error))
    {
        InteractionMessage = TEXT("Route B selected.");
    }
    else
    {
        InteractionMessage = P7Subsystem ? Error : TEXT("P7 simulation is unavailable.");
    }
}
