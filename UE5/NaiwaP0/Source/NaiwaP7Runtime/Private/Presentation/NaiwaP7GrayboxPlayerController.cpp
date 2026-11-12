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

namespace
{
    const TCHAR* GetTowerDisplayName(const FName TowerId)
    {
        if (TowerId == FName(TEXT("P7_Tower_MilkChicken"))) return TEXT("奶鸡炮塔");
        if (TowerId == FName(TEXT("P7_Tower_MilkDog"))) return TEXT("奶狗炮塔");
        if (TowerId == FName(TEXT("P7_Tower_MilkMouse"))) return TEXT("奶鼠炮塔");
        return TEXT("炮塔");
    }
}

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
#if WITH_DEV_AUTOMATION_TESTS
    if (bAutomationHasHoveredCell)
    {
        OutCell = AutomationHoveredCell;
        return true;
    }
#endif

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
        NaiwaP7::PresentationFirstVisibleRow,
        NaiwaP7::PresentationVisibleRows,
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
        InteractionMessage = TEXT("关卡尚未加载，暂时不能选炮塔。");
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
        InteractionMessage = TEXT("这个炮塔栏位没有可用配置。");
        return;
    }

    SelectedTowerId = TowerIds[SlotIndex];
    bHasSelectedTowerCell = false;
    InteractionMessage = FString::Printf(TEXT("已选%s：点击绿色建造位放置，右键可取消。"),
        GetTowerDisplayName(SelectedTowerId));
}

void ANaiwaP7GrayboxPlayerController::ClearSelection()
{
    SelectedTowerId = NAME_None;
    SelectedTowerCell = FIntPoint::ZeroValue;
    bHasSelectedTowerCell = false;
    InteractionMessage = TEXT("已取消当前选择。");
}

void ANaiwaP7GrayboxPlayerController::HandleHudAction(const ENaiwaP7HudAction Action)
{
    switch (Action)
    {
    case ENaiwaP7HudAction::TowerSlot1:
        SelectTowerSlot(0);
        return;
    case ENaiwaP7HudAction::TowerSlot2:
        SelectTowerSlot(1);
        return;
    case ENaiwaP7HudAction::TowerSlot3:
        SelectTowerSlot(2);
        return;
    case ENaiwaP7HudAction::BeginWave:
        HandleBeginWave(FInputActionValue());
        return;
    case ENaiwaP7HudAction::SelectRouteA:
        HandleSelectRouteA(FInputActionValue());
        return;
    case ENaiwaP7HudAction::SelectRouteB:
        HandleSelectRouteB(FInputActionValue());
        return;
    case ENaiwaP7HudAction::TogglePause:
        HandleTogglePause(FInputActionValue());
        return;
    case ENaiwaP7HudAction::UpgradeSelectedTower:
        HandleUpgrade(FInputActionValue());
        return;
    case ENaiwaP7HudAction::ClearSelectedTowerTarget:
        HandleClearTowerTarget(FInputActionValue());
        return;
    case ENaiwaP7HudAction::RestartRun:
    {
        UNaiwaP7TowerDefenseSubsystem* P7Subsystem = GetP7Subsystem();
        FString Error;
        if (P7Subsystem && P7Subsystem->TryRestart(&Error))
        {
            SelectedTowerId = NAME_None;
            SelectedTowerCell = FIntPoint::ZeroValue;
            bHasSelectedTowerCell = false;
            InteractionMessage = TEXT("新的一局开始了：先布防，再点击“开始第1波”。");
        }
        else
        {
            InteractionMessage = Error.IsEmpty() ? TEXT("本局暂时无法重开。") : Error;
        }
        return;
    }
    default:
        return;
    }
}

void ANaiwaP7GrayboxPlayerController::HandlePrimaryClick(const FInputActionValue& Value)
{
    UNaiwaP7TowerDefenseSubsystem* P7Subsystem = GetP7Subsystem();
    if (!P7Subsystem || !P7Subsystem->GetSimulation().HasLevel())
    {
        InteractionMessage = TEXT("关卡尚未加载，请检查 P7 关卡配置。");
        return;
    }

    float MouseX = 0.0f;
    float MouseY = 0.0f;
    if (GetMousePosition(MouseX, MouseY))
    {
        int32 ViewportWidth = 0;
        int32 ViewportHeight = 0;
        GetViewportSize(ViewportWidth, ViewportHeight);
        const NaiwaP7::FSimulationSnapshot Snapshot = P7Subsystem->GetSnapshot();
        const bool bIsTerminal = Snapshot.Phase == NaiwaP7::ESimulationPhase::Won ||
            Snapshot.Phase == NaiwaP7::ESimulationPhase::Lost;
        ENaiwaP7HudAction HudAction = ENaiwaP7HudAction::None;
        if (ANaiwaP7GrayboxHUD::TryHitTestInteractiveAction(
            FVector2D(static_cast<float>(ViewportWidth), static_cast<float>(ViewportHeight)),
            FVector2D(MouseX, MouseY), HudAction, bHasSelectedTowerCell, bIsTerminal))
        {
            HandleHudAction(HudAction);
            return;
        }
        if (bIsTerminal)
        {
            return;
        }
    }

    FIntPoint Cell;
    if (!TryGetHoveredCell(Cell))
    {
        InteractionMessage = TEXT("鼠标不在棋盘内。");
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
        InteractionMessage = FString::Printf(TEXT("已选%s %d级：可升级，或点击障碍指定清障目标。"),
            GetTowerDisplayName(ClickedTower->TowerId), ClickedTower->Level);
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
            InteractionMessage = TEXT("先点击一座炮塔，再点击障碍，指定它优先清障。");
            return;
        }

        FString Error;
        if (P7Subsystem->TrySetTowerObstacleTarget(SelectedTowerCell, ClickedObstacle->ObstacleId, &Error))
        {
            InteractionMessage = FString::Printf(TEXT("已指定目标：%s。清除后可获得金币并解锁额外建造位。"),
                ClickedObstacle->ObstacleId.ToString().EndsWith(TEXT("B")) ? TEXT("燕麦饼叠垛 B") : TEXT("奶油饼干箱 A"));
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
            ? TEXT("已选中炮塔：可点击炮塔栏选新塔，或点击障碍指定清障目标。")
            : TEXT("先点击上方炮塔卡片，再点击绿色建造位放置。");
        return;
    }

    FString Error;
    if (P7Subsystem->TryPlaceTower(Cell, SelectedTowerId, &Error))
    {
        SelectedTowerCell = Cell;
        bHasSelectedTowerCell = true;
        InteractionMessage = FString::Printf(TEXT("已放置%s。可在下方升级，或点击障碍指定攻击目标。"),
            GetTowerDisplayName(SelectedTowerId));
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
        InteractionMessage = TEXT("请先点击棋盘上的炮塔，再执行升级。");
        return;
    }

    FString Error;
    if (P7Subsystem->TryUpgradeTower(SelectedTowerCell, &Error))
    {
        InteractionMessage = TEXT("升级完成。");
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
        InteractionMessage = TEXT("请先点击棋盘上的炮塔，再恢复普通索敌。");
        return;
    }

    FString Error;
    if (P7Subsystem->TrySetTowerObstacleTarget(SelectedTowerCell, NAME_None, &Error))
    {
        InteractionMessage = TEXT("已恢复普通索敌，炮塔会继续攻击路径上的敌人。");
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
        InteractionMessage = TEXT("关卡尚未加载，暂时不能开始波次。");
        return;
    }

    FString Error;
    if (P7Subsystem->TryBeginNextWave(&Error))
    {
        InteractionMessage = FString::Printf(TEXT("第 %d 波开始！守住奶罐。"),
            P7Subsystem->GetSnapshot().CurrentWave);
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
        InteractionMessage = TEXT("P7 模拟器暂不可用。");
        return;
    }

    const bool bNewPaused = !P7Subsystem->IsPaused();
    P7Subsystem->SetPaused(bNewPaused);
    InteractionMessage = bNewPaused ? TEXT("已暂停。") : TEXT("继续防守。");
}

void ANaiwaP7GrayboxPlayerController::HandleSelectRouteA(const FInputActionValue& Value)
{
    UNaiwaP7TowerDefenseSubsystem* P7Subsystem = GetP7Subsystem();
    FString Error;
    if (P7Subsystem && P7Subsystem->TrySelectRoute(NaiwaP7::ERoute::A, &Error))
    {
        InteractionMessage = TEXT("已选择 A 路线：直行。仅可在布防阶段切换。 ");
    }
    else
    {
        InteractionMessage = P7Subsystem ? Error : TEXT("P7 模拟器暂不可用。");
    }
}

void ANaiwaP7GrayboxPlayerController::HandleSelectRouteB(const FInputActionValue& Value)
{
    UNaiwaP7TowerDefenseSubsystem* P7Subsystem = GetP7Subsystem();
    FString Error;
    if (P7Subsystem && P7Subsystem->TrySelectRoute(NaiwaP7::ERoute::B, &Error))
    {
        InteractionMessage = TEXT("已选择 B 路线：上方绕行。仅可在布防阶段切换。");
    }
    else
    {
        InteractionMessage = P7Subsystem ? Error : TEXT("P7 模拟器暂不可用。");
    }
}
