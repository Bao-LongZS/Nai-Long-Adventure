#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NaiwaP4ArenaState.h"
#include "../Input/NaiwaP4InputBuffer.h"
#include "../Data/NaiwaP4UpgradeCatalog.h"
#include "NaiwaP4ArenaSubsystem.generated.h"

class UDataTable;

/** Owns the fixed-step clock and authoritative P4 state for one arena world. */
UCLASS()
class NAIWARUNTIME_API UNaiwaP4ArenaSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bSimulationActive; }

	void StartSimulation(uint32 MasterSeed);
	void StopSimulation();
	/** Drop held values and queued edges, e.g. when the local controller is removed. */
	void ClearPlayerInput();
	/** Game-thread input submission API for the P4 Enhanced Input adapter. */
	void SetPlayerMoveAxis(FVector2D Axis);
	void SetPlayerAttackHeld(bool bHeld);
	void QueuePlayerDashPressed();
	void SetPlayerBlockHeld(bool bHeld);
	void QueuePlayerUltimatePressed();
	/** The command most recently latched for a fixed simulation tick. */
	const FNaiwaP4InputCommand& GetLastConsumedPlayerCommand() const { return LastConsumedPlayerCommand; }
	int64 GetCurrentTick() const { return State.CurrentTick; }
	/** Whole fixed steps still waiting after this frame's capped catch-up work. */
	int64 GetPendingCatchUpSteps() const;
	uint64 GetSimulationStateHash() const;
	const FNaiwaP4ArenaState& GetState() const { return State; }
	bool LoadUpgradeDataTable(const UDataTable* DataTable, FString& OutError);
	const FNaiwaP4UpgradeRow* FindUpgradeRow(FName CardId) const { return UpgradeCatalog.Find(CardId); }
	const TArray<FName>& GetUpgradeCardIds() const { return UpgradeCatalog.GetOrderedCardIds(); }

private:
	void AdvanceFixedStep();
	void AdvancePlayerMovement(const FNaiwaP4InputCommand& Command);

	FNaiwaP4ArenaState State;
	FNaiwaP4InputBuffer InputBuffer;
	FNaiwaP4InputCommand LastConsumedPlayerCommand;
	FNaiwaP4UpgradeCatalog UpgradeCatalog;
	double FixedAccumulatorSeconds = 0.0;
	double FixedStepSeconds = 1.0 / 60.0;
	int32 MaxCatchUpSteps = 4;
	double LogicalArenaWidth = 1100.0;
	double LogicalArenaDepth = 185.0;
	double PlayerWalkSpeed = 270.0;
	double PlayerDepthSpeedScale = 0.611;
	double PlayerBlockMoveSpeedScale = 0.45;
	int32 PlayerMaxHP = 120;
	bool bSimulationActive = false;
};
