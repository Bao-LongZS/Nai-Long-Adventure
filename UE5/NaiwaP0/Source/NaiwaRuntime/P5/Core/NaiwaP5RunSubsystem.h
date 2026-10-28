#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NaiwaP5Types.h"
#include "NaiwaP5RunSubsystem.generated.h"

/** Authoritative, physics-free fixed-step simulation for P5. */
UCLASS()
class NAIWARUNTIME_API UNaiwaP5RunSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return State.Phase == ENaiwaP5RunPhase::Running; }

	void StartM1PrototypeRun(uint32 MasterSeed = 0);
	void StartM2PrototypeRun(uint32 MasterSeed = 0);
	void RestartPrototypeRun();
	void StopRun();
	void SetInputCommand(const FNaiwaP5InputCommand& Command);
	void SetPaused(bool bPaused);
	void NotifySnowContact(bool bOnSnow);

	const FNaiwaP5RunState& GetState() const { return State; }
	int32 GetDeferredCatchUpSteps() const { return State.DeferredCatchUpSteps; }
	uint64 GetSimulationStateHashV4() const;
	const FNaiwaP5RouteFixture& GetActivePrototypeFixture() const;
	FString GetLastAbilityEventText() const;

private:
	void AdvanceFixedStep();
	void ApplyInput(const FNaiwaP5InputCommand& Command);
	void StartJump(bool bChargeWhileHeld = true);
	void StartCliffLaunch();
	void ResolveLanding();
	void StartFall(double GapPenaltyMeters);
	void ProcessRouteEvents(double PreviousDistanceMeters, double NewDistanceMeters);
	void FinalizeRun(ENaiwaP5RunPhase FinalPhase);
	void StartPrototypeRun(bool bM2Route, uint32 MasterSeed);
	void UpdateAbilityAndRideTimers(double DeltaSeconds);
	void RefreshObstacleWarning();
	void TriggerLandingAbilities(bool bSuccessfulLanding);
	void HandleObstacleEvent(const FNaiwaP5RouteEvent& Event);
	void StartRide(const FNaiwaP5RouteEvent& Event);
	void EndRide(bool bNaturalEnd, FName TriggerId);
	bool TryAbsorbRideFall();
	bool IsProtectedFromImpact() const;
	void AppendAbilityEvent(FName AbilityId, FName TriggerId, int32 Value = 0);
	void ResetCombo();
	void RecordP5Settlement();
	const FNaiwaP5SkinRow* GetEquippedSkinRow() const;

	FNaiwaP5RunState State;
	FNaiwaP5InputCommand PendingInput;
	double FixedAccumulatorSeconds = 0.0;
	int32 MaxCatchUpSteps = 4;
	double PendingGapPenaltyMeters = 0.0;
	bool bJumpPressQueued = false;
	bool bJumpReleaseQueued = false;
};
