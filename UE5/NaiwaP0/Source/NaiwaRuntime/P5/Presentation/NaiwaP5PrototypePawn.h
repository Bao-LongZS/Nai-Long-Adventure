#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "NaiwaP5PrototypePawn.generated.h"

class UCameraComponent;
class UCapsuleComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;
class UNaiwaP5RunSubsystem;

/** Graybox pawn for the M1 movement slice; production art remains a 2D atlas. */
UCLASS()
class NAIWARUNTIME_API ANaiwaP5PrototypePawn final : public APawn
{
	GENERATED_BODY()

public:
	ANaiwaP5PrototypePawn();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void JumpPressed();
	void JumpReleased();
	void RotateLeftPressed();
	void RotateLeftReleased();
	void RotateRightPressed();
	void RotateRightReleased();
	void RestartPressed();
	void RestartReleased();
	void TogglePause();
	void SubmitInput();
	void UpdateA0ArtPresentation(const struct FNaiwaP5RunState& State, double DeltaSeconds);

	UPROPERTY()
	TObjectPtr<UCapsuleComponent> CollisionCapsule;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> GrayboxBody;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> GrayboxSnowboard;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> GrayboxMount;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> A0HeroArt;

	UPROPERTY()
	TObjectPtr<UCameraComponent> SideCamera;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> A0HeroArtMaterial;

	TWeakObjectPtr<UNaiwaP5RunSubsystem> RunSubsystem;
	FVector RunStartLocation = FVector::ZeroVector;
	double RestartHeldSeconds = 0.0;
	uint32 RunSeed = 0;
	bool bJumpHeld = false;
	bool bJumpPressedPending = false;
	bool bJumpReleasedPending = false;
	bool bRotateLeft = false;
	bool bRotateRight = false;
	bool bRestartHeld = false;
	bool bRestartTriggered = false;
	bool bPaused = false;
	bool bA0ArtPresentationActive = false;
	bool bA0AtlasMaterialActive = false;
	bool bA0WasAirborne = false;
	double A0LandingPoseSecondsRemaining = 0.0;
};
