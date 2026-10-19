#pragma once

#include "CoreMinimal.h"
#include "NaiwaP1Mechanism.h"
#include "NaiwaP1SwitchPlatform.generated.h"

class ANaiwaCharacter;
class ANaiwaP1Trigger;
class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class ENaiwaP1SwitchPlatformState : uint8
{
	DockedLeft,
	TravelingRight,
	DockedRight,
	TravelingLeft
};

/** P1 touch-switch shuttle: fixed side switches recall the platform; boarding starts a warned crossing. */
UCLASS(Blueprintable)
class NAIWARUNTIME_API ANaiwaP1SwitchPlatform : public ANaiwaP1Mechanism
{
	GENERATED_BODY()

public:
	ANaiwaP1SwitchPlatform();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void ResetForAttempt() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Switch Platform")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Switch Platform")
	TObjectPtr<UBoxComponent> PlatformCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Switch Platform")
	TObjectPtr<UStaticMeshComponent> PlatformDisplay;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Switch Platform")
	TObjectPtr<UTextRenderComponent> PlatformStatusText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Switch Platform")
	TObjectPtr<UBoxComponent> LeftSwitchTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Switch Platform")
	TObjectPtr<UStaticMeshComponent> LeftSwitchDisplay;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Switch Platform")
	TObjectPtr<UTextRenderComponent> LeftSwitchText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Switch Platform")
	TObjectPtr<UBoxComponent> RightSwitchTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Switch Platform")
	TObjectPtr<UStaticMeshComponent> RightSwitchDisplay;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Naiwa|P1 Switch Platform")
	TObjectPtr<UTextRenderComponent> RightSwitchText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Switch Platform", meta = (Units = "cm"))
	float DockLeftX = -420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Switch Platform", meta = (Units = "cm"))
	float DockRightX = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Switch Platform", meta = (Units = "cm"))
	float LeftSwitchX = -850.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Switch Platform", meta = (Units = "cm"))
	float RightSwitchX = 850.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Switch Platform", meta = (Units = "cm"))
	float PlatformTopZ = 120.0f;

	/** Optional endpoint heights relative to the actor. Negative values use PlatformTopZ. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Switch Platform", meta = (Units = "cm"))
	float DockLeftTopZ = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Switch Platform", meta = (Units = "cm"))
	float DockRightTopZ = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Switch Platform", meta = (Units = "cm"))
	FVector PlatformExtent = FVector(120.0f, 160.0f, 20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Switch Platform", meta = (ClampMin = "1.0", Units = "cm/s"))
	float TravelSpeedCmPerSecond = 300.0f;

	/** Smooth, repeatable speed swing across the route; zero preserves constant-speed behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Switch Platform", meta = (ClampMin = "0.0", Units = "cm/s"))
	float TravelSpeedVariationCmPerSecond = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Switch Platform", meta = (ClampMin = "0.05", Units = "s"))
	float StableBoardingSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Switch Platform", meta = (ClampMin = "0.05", Units = "s"))
	float DepartureCueSeconds = 0.65f;

	/** Random visible spike count per shuttle departure. Zero disables transit hazards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Switch Platform", meta = (ClampMin = "0", ClampMax = "4"))
	int32 TransitTrapMinCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Switch Platform", meta = (ClampMin = "0", ClampMax = "4"))
	int32 TransitTrapMaxCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naiwa|P1 Switch Platform")
	int32 TransitTrapSeed = 0;

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Switch Platform")
	ENaiwaP1SwitchPlatformState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Switch Platform")
	float GetCurrentX() const;

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Switch Platform")
	float GetCurrentTopZ() const;

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Switch Platform")
	float GetCurrentTravelSpeedCmPerSecond() const;

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Switch Platform")
	bool IsDepartureCueActive() const { return bDepartureCueActive; }

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Switch Platform")
	bool IsCharacterSupported(const ANaiwaCharacter* Character) const;

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Switch Platform")
	int32 GetLeftSwitchTouchCount() const { return LeftSwitchTouchCount; }

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Switch Platform")
	int32 GetRightSwitchTouchCount() const { return RightSwitchTouchCount; }

	UFUNCTION(BlueprintPure, Category = "Naiwa|P1 Switch Platform")
	int32 GetTransitTrapSpawnCount() const { return TransitTrapActors.Num(); }

	const TArray<TObjectPtr<ANaiwaP1Trigger>>& GetTransitTrapActors() const { return TransitTrapActors; }

	bool RequestDockLeft();
	bool RequestDockRight();

private:
	void ApplyGeometry();
	bool IsCharacterWithinPlatformFootprint(const ANaiwaCharacter* Character) const;
	void SetPlatformX(float NewX);
	float GetDockLeftTopZ() const;
	float GetDockRightTopZ() const;
	void ClearTransitTraps();
	void SpawnTransitTraps(bool bToRight);
	void StartTravel(bool bToRight, const TCHAR* Cause);
	UFUNCTION()
	void HandleLeftSwitchBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void HandleLeftSwitchEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);
	UFUNCTION()
	void HandleRightSwitchBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void HandleRightSwitchEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);
	void UpdateStatusText();

	ENaiwaP1SwitchPlatformState State = ENaiwaP1SwitchPlatformState::DockedLeft;
	float StableBoardingElapsed = 0.0f;
	float DepartureCueRemaining = 0.0f;
	bool bDepartureCueActive = false;
	bool bRiderMustExitBeforeNextDeparture = false;
	bool bLeftSwitchLatched = false;
	bool bRightSwitchLatched = false;
	int32 LeftSwitchTouchCount = 0;
	int32 RightSwitchTouchCount = 0;
	FString LastStatusText;
	TArray<TObjectPtr<ANaiwaP1Trigger>> TransitTrapActors;
	int32 TransitTripIndex = 0;
	uint32 TransitRunSeed = 0;
};
