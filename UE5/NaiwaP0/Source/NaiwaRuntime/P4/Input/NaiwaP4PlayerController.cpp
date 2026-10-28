#include "NaiwaP4PlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "../Core/NaiwaP4ArenaSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"

void ANaiwaP4PlayerController::BeginPlay()
{
	Super::BeginPlay();
	AddRuntimeInputContext();
}

void ANaiwaP4PlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearHeldP4Inputs();
	RemoveRuntimeInputContext();
	Super::EndPlay(EndPlayReason);
}

void ANaiwaP4PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	CreateRuntimeInputContext();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Error, TEXT("P4 input requires EnhancedInputComponent."));
		return;
	}

	EnhancedInput->BindAction(MoveXAction, ETriggerEvent::Triggered, this, &ThisClass::OnMoveX);
	EnhancedInput->BindAction(MoveXAction, ETriggerEvent::Completed, this, &ThisClass::OnMoveXEnded);
	EnhancedInput->BindAction(MoveXAction, ETriggerEvent::Canceled, this, &ThisClass::OnMoveXEnded);
	EnhancedInput->BindAction(MoveYAction, ETriggerEvent::Triggered, this, &ThisClass::OnMoveY);
	EnhancedInput->BindAction(MoveYAction, ETriggerEvent::Completed, this, &ThisClass::OnMoveYEnded);
	EnhancedInput->BindAction(MoveYAction, ETriggerEvent::Canceled, this, &ThisClass::OnMoveYEnded);
	EnhancedInput->BindAction(AttackAction, ETriggerEvent::Started, this, &ThisClass::OnAttackStarted);
	EnhancedInput->BindAction(AttackAction, ETriggerEvent::Completed, this, &ThisClass::OnAttackEnded);
	EnhancedInput->BindAction(AttackAction, ETriggerEvent::Canceled, this, &ThisClass::OnAttackEnded);
	EnhancedInput->BindAction(DashAction, ETriggerEvent::Started, this, &ThisClass::OnDashStarted);
	EnhancedInput->BindAction(BlockAction, ETriggerEvent::Started, this, &ThisClass::OnBlockStarted);
	EnhancedInput->BindAction(BlockAction, ETriggerEvent::Completed, this, &ThisClass::OnBlockEnded);
	EnhancedInput->BindAction(BlockAction, ETriggerEvent::Canceled, this, &ThisClass::OnBlockEnded);
	EnhancedInput->BindAction(UltimateAction, ETriggerEvent::Started, this, &ThisClass::OnUltimateStarted);
	AddRuntimeInputContext();
}

void ANaiwaP4PlayerController::CreateRuntimeInputContext()
{
	if (RuntimeInputContext)
	{
		return;
	}

	RuntimeInputContext = NewObject<UInputMappingContext>(this, TEXT("IMC_NaiwaP4_Runtime"));
	MoveXAction = NewObject<UInputAction>(this, TEXT("IA_P4_MoveX"));
	MoveYAction = NewObject<UInputAction>(this, TEXT("IA_P4_MoveY"));
	AttackAction = NewObject<UInputAction>(this, TEXT("IA_P4_Attack"));
	DashAction = NewObject<UInputAction>(this, TEXT("IA_P4_Dash"));
	BlockAction = NewObject<UInputAction>(this, TEXT("IA_P4_Block"));
	UltimateAction = NewObject<UInputAction>(this, TEXT("IA_P4_Ultimate"));
	check(RuntimeInputContext && MoveXAction && MoveYAction && AttackAction
		&& DashAction && BlockAction && UltimateAction);

	MoveXAction->ValueType = EInputActionValueType::Axis1D;
	MoveYAction->ValueType = EInputActionValueType::Axis1D;
	AttackAction->ValueType = EInputActionValueType::Boolean;
	DashAction->ValueType = EInputActionValueType::Boolean;
	BlockAction->ValueType = EInputActionValueType::Boolean;
	UltimateAction->ValueType = EInputActionValueType::Boolean;

	auto MapKey = [this](UInputAction* Action, const FKey& Key, const bool bNegate)
	{
		FEnhancedActionKeyMapping& Mapping = RuntimeInputContext->MapKey(Action, Key);
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(RuntimeInputContext));
		}
	};

	// P4 owns this context: both WASD and arrow keys map to the same 8-way axes.
	MapKey(MoveXAction, EKeys::D, false);
	MapKey(MoveXAction, EKeys::Right, false);
	MapKey(MoveXAction, EKeys::A, true);
	MapKey(MoveXAction, EKeys::Left, true);
	MapKey(MoveYAction, EKeys::W, false);
	MapKey(MoveYAction, EKeys::Up, false);
	MapKey(MoveYAction, EKeys::S, true);
	MapKey(MoveYAction, EKeys::Down, true);
	MapKey(AttackAction, EKeys::F, false);
	MapKey(DashAction, EKeys::LeftShift, false);
	MapKey(BlockAction, EKeys::SpaceBar, false);
	MapKey(UltimateAction, EKeys::E, false);
}

UEnhancedInputLocalPlayerSubsystem* ANaiwaP4PlayerController::GetP4InputSubsystem() const
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
}

void ANaiwaP4PlayerController::AddRuntimeInputContext()
{
	if (bRuntimeInputContextAdded || !IsLocalController())
	{
		return;
	}
	CreateRuntimeInputContext();
	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetP4InputSubsystem())
	{
		InputSubsystem->AddMappingContext(RuntimeInputContext, 1);
		bRuntimeInputContextAdded = true;
	}
}

void ANaiwaP4PlayerController::RemoveRuntimeInputContext()
{
	if (!bRuntimeInputContextAdded)
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetP4InputSubsystem())
	{
		InputSubsystem->RemoveMappingContext(RuntimeInputContext);
	}
	bRuntimeInputContextAdded = false;
}

void ANaiwaP4PlayerController::SubmitMoveAxis()
{
	if (UWorld* World = GetWorld())
	{
		if (UNaiwaP4ArenaSubsystem* ArenaSubsystem = World->GetSubsystem<UNaiwaP4ArenaSubsystem>())
		{
			ArenaSubsystem->SetPlayerMoveAxis(CurrentMoveAxis);
		}
	}
}

void ANaiwaP4PlayerController::OnMoveX(const FInputActionValue& Value)
{
	CurrentMoveAxis.X = Value.Get<float>();
	SubmitMoveAxis();
}

void ANaiwaP4PlayerController::OnMoveY(const FInputActionValue& Value)
{
	CurrentMoveAxis.Y = Value.Get<float>();
	SubmitMoveAxis();
}

void ANaiwaP4PlayerController::OnMoveXEnded(const FInputActionValue& Value)
{
	CurrentMoveAxis.X = 0.0;
	SubmitMoveAxis();
}

void ANaiwaP4PlayerController::OnMoveYEnded(const FInputActionValue& Value)
{
	CurrentMoveAxis.Y = 0.0;
	SubmitMoveAxis();
}

void ANaiwaP4PlayerController::OnAttackStarted(const FInputActionValue& Value)
{
	if (UWorld* World = GetWorld())
	{
		if (UNaiwaP4ArenaSubsystem* ArenaSubsystem = World->GetSubsystem<UNaiwaP4ArenaSubsystem>())
		{
			ArenaSubsystem->SetPlayerAttackHeld(true);
		}
	}
}

void ANaiwaP4PlayerController::OnAttackEnded(const FInputActionValue& Value)
{
	if (UWorld* World = GetWorld())
	{
		if (UNaiwaP4ArenaSubsystem* ArenaSubsystem = World->GetSubsystem<UNaiwaP4ArenaSubsystem>())
		{
			ArenaSubsystem->SetPlayerAttackHeld(false);
		}
	}
}

void ANaiwaP4PlayerController::OnDashStarted(const FInputActionValue& Value)
{
	if (UWorld* World = GetWorld())
	{
		if (UNaiwaP4ArenaSubsystem* ArenaSubsystem = World->GetSubsystem<UNaiwaP4ArenaSubsystem>())
		{
			ArenaSubsystem->QueuePlayerDashPressed();
		}
	}
}

void ANaiwaP4PlayerController::OnBlockStarted(const FInputActionValue& Value)
{
	if (UWorld* World = GetWorld())
	{
		if (UNaiwaP4ArenaSubsystem* ArenaSubsystem = World->GetSubsystem<UNaiwaP4ArenaSubsystem>())
		{
			ArenaSubsystem->SetPlayerBlockHeld(true);
		}
	}
}

void ANaiwaP4PlayerController::OnBlockEnded(const FInputActionValue& Value)
{
	if (UWorld* World = GetWorld())
	{
		if (UNaiwaP4ArenaSubsystem* ArenaSubsystem = World->GetSubsystem<UNaiwaP4ArenaSubsystem>())
		{
			ArenaSubsystem->SetPlayerBlockHeld(false);
		}
	}
}

void ANaiwaP4PlayerController::OnUltimateStarted(const FInputActionValue& Value)
{
	if (UWorld* World = GetWorld())
	{
		if (UNaiwaP4ArenaSubsystem* ArenaSubsystem = World->GetSubsystem<UNaiwaP4ArenaSubsystem>())
		{
			ArenaSubsystem->QueuePlayerUltimatePressed();
		}
	}
}

void ANaiwaP4PlayerController::ClearHeldP4Inputs()
{
	CurrentMoveAxis = FVector2D::ZeroVector;
	if (UWorld* World = GetWorld())
	{
		if (UNaiwaP4ArenaSubsystem* ArenaSubsystem = World->GetSubsystem<UNaiwaP4ArenaSubsystem>())
		{
			ArenaSubsystem->ClearPlayerInput();
		}
	}
}
