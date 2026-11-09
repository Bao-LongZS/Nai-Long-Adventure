#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NaiwaP7GrayboxPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UNaiwaP7TowerDefenseSubsystem;
struct FInputActionValue;

/** Mouse-cursor owner for the temporary P7 graybox interaction surface. */
UCLASS()
class NAIWAP7RUNTIME_API ANaiwaP7GrayboxPlayerController final : public APlayerController
{
    GENERATED_BODY()

public:
    ANaiwaP7GrayboxPlayerController();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupInputComponent() override;

    bool TryGetHoveredCell(FIntPoint& OutCell) const;
    FName GetSelectedTowerId() const { return SelectedTowerId; }
    bool HasSelectedTowerCell() const { return bHasSelectedTowerCell; }
    FIntPoint GetSelectedTowerCell() const { return SelectedTowerCell; }
    const FString& GetInteractionMessage() const { return InteractionMessage; }

private:
    void CreateRuntimeInputContext();
    void AddRuntimeInputContext();
    void RemoveRuntimeInputContext();
    UNaiwaP7TowerDefenseSubsystem* GetP7Subsystem() const;
    void SelectTowerSlot(int32 SlotIndex);
    void ClearSelection();
    void HandlePrimaryClick(const FInputActionValue& Value);
    void HandleSecondaryClick(const FInputActionValue& Value);
    void HandleSelectTowerOne(const FInputActionValue& Value);
    void HandleSelectTowerTwo(const FInputActionValue& Value);
    void HandleSelectTowerThree(const FInputActionValue& Value);
    void HandleUpgrade(const FInputActionValue& Value);
    void HandleClearTowerTarget(const FInputActionValue& Value);
    void HandleBeginWave(const FInputActionValue& Value);
    void HandleTogglePause(const FInputActionValue& Value);
    void HandleSelectRouteA(const FInputActionValue& Value);
    void HandleSelectRouteB(const FInputActionValue& Value);

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> RuntimeInputContext;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> PrimaryClickAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> SecondaryClickAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> SelectTowerOneAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> SelectTowerTwoAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> SelectTowerThreeAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> UpgradeAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> ClearTowerTargetAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> BeginWaveAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> TogglePauseAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> SelectRouteAAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> SelectRouteBAction;

    FName SelectedTowerId = NAME_None;
    FIntPoint SelectedTowerCell = FIntPoint::ZeroValue;
    bool bHasSelectedTowerCell = false;
    bool bRuntimeInputContextAdded = false;
    FString InteractionMessage = TEXT("Choose tower 1-3, then click a build site.");
};
