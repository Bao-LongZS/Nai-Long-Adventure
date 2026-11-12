#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NaiwaP4ArenaState.h"
#include "../Input/NaiwaP4InputBuffer.h"
#include "../Data/NaiwaP4DataTypes.h"
#include "../Data/NaiwaP4UpgradeCatalog.h"
#include "../Progress/NaiwaP4Beta6ChapterFlow.h"
#include "NaiwaP4ArenaSubsystem.generated.h"

class UDataTable;
class UNaiwaP4ProgressSubsystem;

struct FNaiwaP4WaveSpawnEntry
{
	int32 WaveIndex = 0;
	int32 SpawnOrder = 0;
	FName RowId;
	FName EnemyTypeId;
	int32 Count = 0;
	int32 SpawnIntervalTicks = 0;
};

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
	virtual bool IsTickable() const override
	{
		return bSimulationActive && State.RunResult == ENaiwaP4RunResult::InProgress
			&& State.PendingUpgradeChoices <= 0;
	}

	void StartSimulation(uint32 MasterSeed);
	/** Preview-only depth override, applied before StartSimulation in the owning world. */
	void SetArenaDepthForPreview(double Depth);
	/** Starts the W2-only graybox encounter used by the art-integration preview map. */
	void StartGrayboxCombatPreview();
	/** Starts the W3 table-driven five-wave encounter in the integration preview. */
	bool StartWaveCombatPreview();
	/** Restarts the preview at its original seed and reapplies the run's upgrades. */
	bool RestartWaveCombatPreview();
	void StopSimulation();
	/** Drop held values and queued edges, e.g. when the local controller is removed. */
	void ClearPlayerInput();
	/** Game-thread input submission API for the P4 Enhanced Input adapter. */
	void SetPlayerMoveAxis(FVector2D Axis);
	void SetPlayerMeleeHeld(bool bHeld);
	void QueuePlayerDashPressed();
	void SetPlayerBlockHeld(bool bHeld);
	void QueuePlayerUltimatePressed();
	/** The command most recently latched for a fixed simulation tick. */
	const FNaiwaP4InputCommand& GetLastConsumedPlayerCommand() const { return LastConsumedPlayerCommand; }
	int64 GetCurrentTick() const { return State.CurrentTick; }
	/** Whole fixed steps still waiting after this frame's capped catch-up work. */
	int64 GetPendingCatchUpSteps() const;
	/** Presentation-only interpolation between the last two fixed states. */
	FVector2D GetSmoothedPresentationPosition(int32 EntityId, const FVector2D& CurrentPosition) const;
	uint64 GetSimulationStateHash() const;
	const FNaiwaP4ArenaState& GetState() const { return State; }
	bool LoadUpgradeDataTable(const UDataTable* DataTable, FString& OutError);
	bool LoadUpgradeCsvFile(const FString& UpgradeCsvPath, FString& OutError);
	bool LoadCombatDataTables(const UDataTable* EnemyTable, const UDataTable* WaveTable, FString& OutError);
	bool LoadCombatCsvFiles(const FString& EnemyCsvPath, const FString& WaveCsvPath, FString& OutError);
	/** Loads the frozen six-table projection without changing the legacy W3 preview tables. */
	bool LoadBeta6ProgressionTables(const UDataTable* ChapterTable, const UDataTable* ThemeTable,
		const UDataTable* WaveSpawnTable, const UDataTable* EnemyTable,
		const UDataTable* BossTable, const UDataTable* BossAttackTable, FString& OutError);
	/** Starts/resumes/retries one Beta6 chapter in the fixed-step arena. */
	bool StartBeta6Run(int64 RunSeed, FString& OutError);
	bool ResumeBeta6Run(FString& OutError);
	bool RetryBeta6Chapter(FString& OutError);
	bool BeginNextBeta6Chapter(FString& OutError);
	bool CompleteBeta6Chapter(int32& OutNextChapterIndex, FString& OutError);
	ENaiwaP4Beta6FlowPhase GetBeta6FlowPhase() const { return Beta6ChapterFlow.GetPhase(); }
	bool IsBeta6RuntimeActive() const { return bBeta6RuntimeActive; }
	const FNaiwaP4Beta6ChapterBundle* GetCurrentBeta6Chapter() const
		{ return Beta6ChapterFlow.GetCurrentChapter(); }
	TArray<FName> GetAvailableUpgradeCardIds() const;
	int32 GetUpgradeCardRank(FName CardId) const;
	bool RerollUpgradeChoice(FString& OutError);
	bool ConfirmUpgradeChoice(FName CardId, FString& OutError);
	const FNaiwaP4UpgradeRow* FindUpgradeRow(FName CardId) const { return UpgradeCatalog.Find(CardId); }
	const TArray<FName>& GetUpgradeCardIds() const { return UpgradeCatalog.GetOrderedCardIds(); }

private:
	void AdvanceFixedStep();
	void AdvancePlayerMovement(const FNaiwaP4InputCommand& Command);
	void AdvancePlayerAutoFire();
	void AdvanceEnemyBrains();
	void AdvanceBeta6EnemyBrain(FNaiwaP4EntityState& Enemy, const FNaiwaP4EntityState& Player);
	void AdvanceBossBrain(FNaiwaP4EntityState& Boss, const FNaiwaP4EntityState& Player);
	void AdvanceBeta6BossBrain(FNaiwaP4EntityState& Boss, const FNaiwaP4EntityState& Player);
	void BeginBeta6BossAttack(FNaiwaP4EntityState& Boss, const FNaiwaP4EntityState& Player,
		const FNaiwaP4Beta6BossAttackRow& Attack);
	void AdvanceWaveDirector();
	void SpawnWaveEnemy(const FNaiwaP4WaveSpawnEntry& Entry);
	void SpawnWaveBoss();
	void QueueBeta6EnemySpawn(FName EnemyId, FVector2D Position, int32 WaveIndex);
	void QueueSummonedEnemies(const TArray<FName>& EnemyIds, int32 Count,
		FVector2D Origin, int32 WaveIndex, int32 MaxConcurrentEnemies = 4);
	void QueueEnemyProjectile(int32 OwnerEntityId, FVector2D Origin, FVector2D Target,
		float Speed, int32 Damage, float ExplosionRadius = 0.0f,
		float SlowMultiplier = 1.0f, int32 SlowDurationTicks = 0);
	void FlushPendingSimulatedEntities();
	void BeginWave(int32 WaveIndex);
	bool StartActiveBeta6Chapter(UNaiwaP4ProgressSubsystem& Progress, FString& OutError);
	bool RestoreBeta6UpgradeSequence(UNaiwaP4ProgressSubsystem& Progress, FString& OutError);
	UNaiwaP4ProgressSubsystem* GetBeta6ProgressSubsystem() const;
	void AdvanceProjectiles();
	void ResolvePendingDamage(bool bPlayerBlockHeld);
	void GrantKillExperience(int32 SourceEntityId, int32 KilledEntityId);
	void DrawUpgradeOffer(bool bTryToChangePreviousOffer);
	void TickCombatTimers();
	void QueueDamage(int32 SourceEntityId, int32 TargetEntityId, int32 Damage, FVector2D ImpactPosition);
	void EmitCombatEvent(FName EventId, int32 SourceEntityId, int32 TargetEntityId, FVector2D Position);
	void FirePlayerProjectile(int32 PlayerEntityId, int32 TargetEntityId);
	void ResolvePlayerMeleeHit(FNaiwaP4EntityState& Player);
	void StartPlayerDash(FNaiwaP4EntityState& Player, FVector2D MoveAxis);
	void ApplyDashDamage(FNaiwaP4EntityState& Player, FVector2D StartPosition, FVector2D EndPosition);
	void UsePlayerUltimate(FNaiwaP4EntityState& Player);
	FNaiwaP4EntityState* FindPlayerEntity();
	FNaiwaP4EntityState* FindNearestEnemyTarget(const FVector2D& Origin, double MaxRange);
	const TMap<FName, FNaiwaP4EnemyRow>& GetActiveEnemyCatalog() const;
	const TArray<FNaiwaP4WaveSpawnEntry>& GetActiveWaveSpawnPlan() const;
	int32 SecondsToTicks(double Seconds) const;

	FNaiwaP4ArenaState State;
	FNaiwaP4InputBuffer InputBuffer;
	FNaiwaP4InputCommand LastConsumedPlayerCommand;
	FNaiwaP4UpgradeCatalog UpgradeCatalog;
	FNaiwaP4Beta6ChapterFlow Beta6ChapterFlow;
	TMap<FName, FNaiwaP4EnemyRow> EnemyCatalog;
	TArray<FNaiwaP4WaveSpawnEntry> WaveSpawnPlan;
	TMap<FName, FNaiwaP4EnemyRow> Beta6EnemyCatalog;
	TArray<FNaiwaP4WaveSpawnEntry> Beta6WaveSpawnPlan;
	TArray<FNaiwaP4EntityState> PendingSimulatedEntities;
	TMap<int32, FVector2D> PreviousPresentationPositions;
	double FixedAccumulatorSeconds = 0.0;
	double FixedStepSeconds = 1.0 / 60.0;
	int32 MaxCatchUpSteps = 8;
	double LogicalArenaWidth = 1100.0;
	double LogicalArenaDepth = 185.0;
	double PlayerWalkSpeed = 270.0;
	double PlayerDepthSpeedScale = 0.611;
	double PlayerBlockMoveSpeedScale = 0.45;
	int32 PlayerMaxHP = 120;
	bool bSimulationActive = false;
	bool bBeta6CatalogLoaded = false;
	bool bBeta6RuntimeActive = false;
	bool bRestoringBeta6Upgrades = false;
};
