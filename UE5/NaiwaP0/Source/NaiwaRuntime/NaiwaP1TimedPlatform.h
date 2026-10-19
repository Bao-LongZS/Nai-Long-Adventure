#pragma once

#include "CoreMinimal.h"
#include "NaiwaP1Mechanism.h"
#include "NaiwaP1TimedPlatform.generated.h"

class ANaiwaCharacter;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class ENaiwaP1TimedPlatformState : uint8
{
	Ready,
	Countdown,
	Disabled,
	Recovering
};

/** P1 timed floor: a stable first landing starts one countdown, then the floor vanishes temporarily. */
UCLASS(Blueprintable)
class NAIWARUNTIME_API ANaiwaP1TimedPlatform : public ANaiwaP1Mechanism
{
	GENERATED_BODY()

public:
	ANaiwaP1TimedPlatform();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void ResetForAttempt() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Timed Platform")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Timed Platform")
	TObjectPtr<UBoxComponent> PlatformCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Timed Platform")
	TObjectPtr<UStaticMeshComponent> PlatformDisplay;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Timed Platform")
	TObjectPtr<UTextRenderComponent> StatusText;

	/** Top surface height relative to this actor's origin. Keep the actor horizontal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Timed Platform", meta = (Units = "cm"))
	float PlatformTopZ = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Timed Platform", meta = (Units = "cm"))
	FVector PlatformExtent = FVector(210.0f, 160.0f, 20.0f);

	/** Continuous grounded time required before the first countdown starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Timed Platform", meta = (ClampMin = "0.05", Units = "s"))
	float StableLandingSeconds = 0.18f;

	/** Walk150 needs about 2.8 seconds to cross the default 420 cm width. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Timed Platform", meta = (ClampMin = "0.1", Units = "s"))
	float ActiveSeconds = 3.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Timed Platform", meta = (ClampMin = "0.1", Units = "s"))
	float DisabledSeconds = 1.6f;

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Timed Platform")
	ENaiwaP1TimedPlatformState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Timed Platform")
	float GetCountdownRemaining() const { return CountdownRemaining; }

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Timed Platform")
	float GetDisabledRemaining() const { return DisabledRemaining; }

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Timed Platform")
	float GetStableLandingElapsed() const { return StableLandingElapsed; }

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Timed Platform")
	bool IsCarryingCollisionEnabled() const;

	/** Conservative capsule/box intersection used to guard collision restoration. */
	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Timed Platform")
	bool IsCapsuleOccupyingPlatform() const;

	/** Lifetime counters are intentionally preserved across attempt resets for probes. */
	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Timed Platform")
	int32 GetTriggerCount() const { return TriggerCount; }

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Timed Platform")
	int32 GetDisableCount() const { return DisableCount; }

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Timed Platform")
	int32 GetRecoveryCount() const { return RecoveryCount; }

private:
	void ApplyGeometry();
	void SetCarryingEnabled(bool bEnabled);
	void UpdatePresentation();
	bool IsCharacterSupported(const ANaiwaCharacter* Character) const;

	ENaiwaP1TimedPlatformState State = ENaiwaP1TimedPlatformState::Ready;
	float StableLandingElapsed = 0.0f;
	float CountdownRemaining = 0.0f;
	float DisabledRemaining = 0.0f;
	int32 TriggerCount = 0;
	int32 DisableCount = 0;
	int32 RecoveryCount = 0;
	FString LastStatusText;
};
