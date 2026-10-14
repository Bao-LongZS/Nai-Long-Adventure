#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NaiwaCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USkeletalMesh;
class UAnimSequence;
class UAnimInstance;
class UNaiwaAutoTestComponent;
struct FInputActionValue;

UCLASS(Blueprintable)
class NAIWARUNTIME_API ANaiwaCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ANaiwaCharacter();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void PawnClientRestart() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|Camera")
    TObjectPtr<UCameraComponent> SideCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|Tests")
    TObjectPtr<UNaiwaAutoTestComponent> AutoTest;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Assets")
    TObjectPtr<USkeletalMesh> SkeletalMeshAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Assets")
    TSubclassOf<UAnimInstance> AnimationClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Assets")
    TObjectPtr<UAnimSequence> IdleSequence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Assets")
    TObjectPtr<UAnimSequence> WalkSequence;

    /** Optional running clip; walking continues to use the original Walk asset. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Assets")
    TObjectPtr<UAnimSequence> RunSequence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Assets")
    TObjectPtr<UAnimSequence> JumpSequence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Assets")
    TObjectPtr<UAnimSequence> LaughSequence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Dimensions")
    float ReferenceHeightCm = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Dimensions")
    FVector MeshScale = FVector::OneVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Dimensions")
    FVector MeshOffset = FVector(0.0f, 0.0f, -90.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Dimensions", meta = (ClampMin = "1.0"))
    float CollisionRadiusCm = 42.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Dimensions", meta = (ClampMin = "1.0"))
    float CollisionHalfHeightCm = 90.0f;

    /** Only the mesh turns; the actor and side camera retain their world orientation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Dimensions")
    float MeshRightYaw = -90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Input")
    TObjectPtr<UInputMappingContext> InputMapping;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Input")
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Input")
    TObjectPtr<UInputAction> PauseAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Input")
    TObjectPtr<UInputAction> ResetAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|Input")
    TObjectPtr<UInputAction> GaitAction;

    UFUNCTION(BlueprintCallable, Category = "Naiwa|Control")
    void ResetToSpawn();

    UFUNCTION(BlueprintCallable, Category = "Naiwa|Control")
    void TogglePause();

    UFUNCTION(BlueprintPure, Category = "Naiwa|State")
    FString GetLocomotionState() const;

    UFUNCTION(BlueprintPure, Category = "Naiwa|State")
    bool IsRunningGait() const { return bRunningGait; }

    /** Coarse component bounds measurement; this is not a posed mesh foot contact measurement. */
    UFUNCTION(BlueprintPure, Category = "Naiwa|State")
    float GetMeshBoundsBottom() const;

    UFUNCTION(BlueprintPure, Category = "Naiwa|State")
    float GetCapsuleBottom() const;

    void SetAutomatedMoveInput(float Value);
    void DisableAutomatedInput();

    /** Opt-in real-time diagnostic samples; HUD also calls this while paused. */
    void SampleInputEvidence();

    int32 GetMoveInputEvents() const { return MoveInputEvents; }
    int32 GetJumpInputEvents() const { return JumpInputEvents; }
    int32 GetPauseInputEvents() const { return PauseInputEvents; }
    int32 GetResetInputEvents() const { return ResetInputEvents; }
    int32 GetGaitInputEvents() const { return GaitInputEvents; }
    const FVector& GetLastResetLocation() const { return LastResetLocation; }
    float GetLastResetSpeed() const { return LastResetSpeed; }

protected:
    void ApplyCharacterAssets();
    void AddInputMapping();
    void HandleMove(const FInputActionValue& Value);
    void HandleMoveCompleted(const FInputActionValue& Value);
    void HandleJumpStarted(const FInputActionValue& Value);
    void HandleJumpCompleted(const FInputActionValue& Value);
    void HandlePause(const FInputActionValue& Value);
    void HandleReset(const FInputActionValue& Value);
    void HandleGaitToggle(const FInputActionValue& Value);
    void ApplyFacing();
    void WriteInputEvidence(const FString& Event);

    FVector SpawnLocation = FVector::ZeroVector;
    bool bFacingRight = true;
    bool bRunningGait = true;
    bool bAutomatedInput = false;
    float AutomatedMoveInput = 0.0f;
    int32 MoveInputEvents = 0;
    int32 JumpInputEvents = 0;
    int32 PauseInputEvents = 0;
    int32 ResetInputEvents = 0;
    int32 GaitInputEvents = 0;
    float LastResetSpeed = 0.0f;
    FVector LastResetLocation = FVector::ZeroVector;
    float LastLoggedMoveInput = 0.0f;
    double InputEvidenceStartSeconds = 0.0;
    double NextInputEvidenceSampleSeconds = 0.0;
    FString InputEvidenceJsonlPath;
};
