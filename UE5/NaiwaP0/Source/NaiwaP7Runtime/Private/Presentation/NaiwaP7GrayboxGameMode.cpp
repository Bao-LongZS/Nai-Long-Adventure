#include "Presentation/NaiwaP7GrayboxGameMode.h"

#include "Core/NaiwaP7TowerDefenseSubsystem.h"
#include "Engine/DataTable.h"
#include "Misc/App.h"
#include "Presentation/NaiwaP7GrayboxHUD.h"
#include "Presentation/NaiwaP7GrayboxPlayerController.h"
#include "UObject/ConstructorHelpers.h"

ANaiwaP7GrayboxGameMode::ANaiwaP7GrayboxGameMode()
{
    HUDClass = ANaiwaP7GrayboxHUD::StaticClass();
    PlayerControllerClass = ANaiwaP7GrayboxPlayerController::StaticClass();
}

ANaiwaP7AlphaGrayboxGameMode::ANaiwaP7AlphaGrayboxGameMode()
{
    const TCHAR* ProjectName = FApp::GetProjectName();
    if (!ProjectName || FCString::Stricmp(ProjectName, TEXT("Nai_Long")) != 0)
    {
        return;
    }

    static ConstructorHelpers::FObjectFinder<UDataTable> LevelTableFinder(
        TEXT("/Game/Naiwa/P7/Data/DT_P7_Alpha01_Level.DT_P7_Alpha01_Level"));
    static ConstructorHelpers::FObjectFinder<UDataTable> TowerTableFinder(
        TEXT("/Game/Naiwa/P7/Data/DT_P7_Alpha01_Towers.DT_P7_Alpha01_Towers"));
    static ConstructorHelpers::FObjectFinder<UDataTable> EnemyTableFinder(
        TEXT("/Game/Naiwa/P7/Data/DT_P7_Alpha01_Enemies.DT_P7_Alpha01_Enemies"));
    static ConstructorHelpers::FObjectFinder<UDataTable> ObstacleTableFinder(
        TEXT("/Game/Naiwa/P7/Data/DT_P7_Alpha01_Obstacles.DT_P7_Alpha01_Obstacles"));
    static ConstructorHelpers::FObjectFinder<UDataTable> WaveTableFinder(
        TEXT("/Game/Naiwa/P7/Data/DT_P7_Alpha01_Waves.DT_P7_Alpha01_Waves"));

    LevelTable = LevelTableFinder.Object;
    TowerTable = TowerTableFinder.Object;
    EnemyTable = EnemyTableFinder.Object;
    ObstacleTable = ObstacleTableFinder.Object;
    WaveTable = WaveTableFinder.Object;
    LevelRowName = TEXT("P7_Alpha01");
}

void ANaiwaP7GrayboxGameMode::StartPlay()
{
    Super::StartPlay();

    if (!LevelTable && !TowerTable && !EnemyTable && !ObstacleTable && !WaveTable)
    {
        UE_LOG(LogTemp, Log, TEXT("P7 graybox has no DataTables assigned; waiting for level data."));
        return;
    }

    UNaiwaP7TowerDefenseSubsystem* P7Subsystem = GetWorld()
        ? GetWorld()->GetSubsystem<UNaiwaP7TowerDefenseSubsystem>()
        : nullptr;
    if (!P7Subsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("P7 graybox could not find its simulation subsystem."));
        return;
    }

    FString Error;
    if (!P7Subsystem->LoadFromDataTables(
        LevelTable, TowerTable, EnemyTable, ObstacleTable, WaveTable, LevelRowName, &Error))
    {
        UE_LOG(LogTemp, Error, TEXT("P7 graybox could not load its configured level: %s"), *Error);
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("P7 graybox loaded level %s from five configured DataTables."), *LevelRowName.ToString());
}
