#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NaiwaP4PlayerController.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/** P4-only Enhanced Input adapter; it forwards commands to the deterministic arena subsystem. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP4PlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:
	void CreateRuntimeInputContext();
	void AddRuntimeInputContext();
	void RemoveRuntimeInputContext();
	UEnhancedInputLocalPlayerSubsystem* GetP4InputSubsystem() const;

	void OnMoveX(const FInputActionValue& Value);
	void OnMoveY(const FInputActionValue& Value);
	void OnMoveXEnded(const FInputActionValue& Value);
	void OnMoveYEnded(const FInputActionValue& Value);
	void SubmitMoveAxis();

	void OnAttackStarted(const FInputActionValue& Value);
	void OnAttackEnded(const FInputActionValue& Value);
	void OnDashStarted(const FInputActionValue& Value);
	void OnBlockStarted(const FInputActionValue& Value);
	void OnBlockEnded(const FInputActionValue& Value);
	void OnUltimateStarted(const FInputActionValue& Value);
	void ClearHeldP4Inputs();

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RuntimeInputContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveXAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveYAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DashAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BlockAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> UltimateAction;

	FVector2D CurrentMoveAxis = FVector2D::ZeroVector;
	bool bRuntimeInputContextAdded = false;
};
