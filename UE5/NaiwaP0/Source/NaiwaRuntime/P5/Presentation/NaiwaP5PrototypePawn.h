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
	bool GetA0RideAnchorProbeWorldPoints(FVector& OutRiderSeatWorld,
		FVector& OutMountSaddleWorld) const;

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
	void UpdateA0ArtPresentation(const struct FNaiwaP5RunState& State, double DeltaSeconds,
		double RouteSlopeDegrees);

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
	TObjectPtr<UStaticMeshComponent> A0MountArt;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> A0SnowboardArt;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> A0LandingPuffArt;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> A0SnowTrackArt;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> A0SnowflakeArts;

	UPROPERTY()
	TObjectPtr<UCameraComponent> SideCamera;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> A0HeroArtMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> A0MountArtMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> A0SnowboardArtMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> A0LandingPuffArtMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> A0SnowTrackArtMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> A0SnowflakeArtMaterials;

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
	bool bA0ContinuousAtlasActive = false;
	bool bA0ContinuousMountAtlasActive = false;
	bool bA0WasAirborne = false;
	double A0TakeoffPoseSecondsRemaining = 0.0;
	double A0LandingPoseSecondsRemaining = 0.0;
	double A0RideSeatMeshLocalY = 0.0;
	double A0MountSaddleMeshLocalY = 0.0;
	bool bA0RideAnchorProbeValid = false;
};
