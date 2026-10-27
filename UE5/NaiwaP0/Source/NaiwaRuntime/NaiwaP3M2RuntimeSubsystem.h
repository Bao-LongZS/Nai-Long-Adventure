#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NaiwaP3M2LevelSession.h"
#include "NaiwaP3M2RuntimeSubsystem.generated.h"

class ANaiwaP3BombActor;
class ANaiwaP3M1EnemyActor;
class ANaiwaP3M1MapActor;
class ANaiwaP3PlayerPawn;
class ANaiwaP3M2WorldPresentation;

enum class ENaiwaP3M2RunState : uint8
{
	Playing,
	Won,
	Defeated
};

struct FNaiwaP3M2SlowSplashState
{
	FIntPoint CenterCell = FIntPoint::ZeroValue;
	int32 ImpactTick = 0;
	int32 ClearTick = 0;
	int32 OwnerEnemyIndex = INDEX_NONE;
	bool bImpacted = false;
};

struct FNaiwaP3M2EnemyLootState
{
	int32 BombCapacity = 1;
	int32 BombRange = 1;
	int32 ShieldCharges = 0;
	float SpeedMultiplier = 1.0f;
};

UCLASS()
class NAIWARUNTIME_API UNaiwaP3M2RuntimeSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bSessionActive; }

	bool StartSession(int32 LevelId, int32 StartAttemptCount, bool bFogEnabled, FString& OutError);
	void StopSession();
	void RegisterPlayerPawn(ANaiwaP3PlayerPawn* Pawn);
	void RegisterPresentationActors(ANaiwaP3M1MapActor* InMapActor,
		const TArray<ANaiwaP3M1EnemyActor*>& InEnemyActors,
		ANaiwaP3M2WorldPresentation* InWorldPresentation);
	void SetPlayerDirection(const FIntPoint& Direction);
	bool PlacePlayerBomb();
	void SelectPlayerBombType(ENaiwaP3BombType Type);
	void SetActiveBombForecasts(const TArray<FNaiwaP3M2BombForecastInput>& Bombs);
	void SetRageEscape(bool bEnabled) { bRageEscape = bEnabled; }

	bool TryMovePlayer(const FIntPoint& Direction, FString& OutMessage);
	bool TryChickenJump(const FIntPoint& Direction, FString& OutMessage);
	void ReportFireAtCell(const FIntPoint& Cell);

	const FNaiwaP3M2LevelSession* GetSession() const { return bSessionActive ? &Session : nullptr; }
	FIntPoint GetPlayerCell() const { return PlayerCell; }
	const TArray<FIntPoint>& GetEnemyCells() const { return EnemyCells; }
	const TArray<FNaiwaP3M2EnemyIntent>& GetEnemyIntents() const { return EnemyIntents; }
	const TArray<int32>& GetEnemyBombRequests() const { return EnemyBombRequests; }
	const TArray<int32>& GetEnemyRangedAttackRequests() const { return EnemyRangedAttackRequests; }
	const TArray<FNaiwaP3M2BombForecastInput>& GetActiveBombForecasts() const { return ActiveBombs; }
	const TArray<FIntPoint>& GetMechanicDestroyedBombCells() const { return MechanicDestroyedBombCells; }
	void ClearPendingEvents();
	bool IsEnemyAlive(int32 EnemyIndex) const { return EnemyAlive.IsValidIndex(EnemyIndex) && EnemyAlive[EnemyIndex] != 0; }
	int32 GetPlayerHitCount() const { return PlayerHitCount; }
	int32 GetPlayerLives() const { return PlayerLives; }
	int32 GetCollectedEggCount() const { return Session.GetCollectedEggCount(); }
	int32 GetRemainingEggCount() const { return Session.GetRemainingEggCount(); }
	int32 GetAliveEnemyCount() const;
	ENaiwaP3M2RunState GetRunState() const { return RunState; }
	bool IsRageEscapeActive() const { return bRageEscape; }
	float GetRageElapsedSeconds() const
	{
		return bRageEscape ? FMath::Max(0, Session.GetCurrentTick() - RageStartTick) * FixedStepSeconds : 0.0f;
	}
	int32 GetStartAttemptCount() const { return Session.GetLevel().AttemptCount; }
	int32 GetLevelId() const { return Session.GetLevel().LevelId; }
	const FString& GetLayoutHash() const { return Session.GetLevel().LayoutHash; }
	int32 GetCurrentTick() const { return bSessionActive ? Session.GetCurrentTick() : 0; }
	float GetDamageFeedbackFraction() const
	{
		return bSessionActive ? FMath::Clamp(static_cast<float>(DamageFeedbackUntilTick - Session.GetCurrentTick()) / 15.0f, 0.0f, 1.0f) : 0.0f;
	}
	const FString& GetDamageFeedbackMessage() const { return DamageFeedbackMessage; }
	const FString& GetTransientEventMessage() const { return TransientEventMessage; }
	float GetTransientEventSeconds() const
	{
		return bSessionActive ? FMath::Max(0, TransientEventUntilTick - Session.GetCurrentTick()) * FixedStepSeconds : 0.0f;
	}
	bool WasLastDamageShielded() const { return bLastDamageShielded; }
	float GetNaiYuLockSeconds() const
	{
		if (!bSessionActive) return -1.0f;
		int32 RemainingTicks = MAX_int32;
		for (const FNaiwaP3M2SlowSplashState& Splash : SlowSplashes)
		{
			if (!Splash.bImpacted) RemainingTicks = FMath::Min(RemainingTicks, Splash.ImpactTick - Session.GetCurrentTick());
		}
		return RemainingTicks == MAX_int32 ? -1.0f : FMath::Max(0, RemainingTicks) * FixedStepSeconds;
	}
	int32 GetNearestNaiChaDistance() const
	{
		if (!bSessionActive) return INDEX_NONE;
		int32 NearestDistance = MAX_int32;
		const TArray<FNaiwaP3M2EnemyPlacement>& Placements = Session.GetLevel().Enemies;
		for (int32 Index = 0; Index < Placements.Num() && Index < EnemyCells.Num(); ++Index)
		{
			if (Placements[Index].Kind == ENaiwaP3M2EnemyKind::NaiCha && IsEnemyAlive(Index))
			{
				NearestDistance = FMath::Min(NearestDistance,
					FMath::Abs(EnemyCells[Index].X - PlayerCell.X) + FMath::Abs(EnemyCells[Index].Y - PlayerCell.Y));
			}
		}
		return NearestDistance == MAX_int32 ? INDEX_NONE : NearestDistance;
	}
	float GetExplosionFlashFraction() const
	{
		return bSessionActive ? FMath::Clamp(static_cast<float>(ExplosionFlashUntilTick - Session.GetCurrentTick()) / 5.0f, 0.0f, 1.0f) : 0.0f;
	}
	FVector2D GetExplosionShakeOffset() const;
	float GetBombPlacementCooldownSeconds() const
	{
		return bSessionActive ? FMath::Max(0, NextPlayerBombTick - Session.GetCurrentTick()) * FixedStepSeconds : 0.0f;
	}
	float GetBombPlacementIntervalSeconds() const { return BombPlacementIntervalTicks * FixedStepSeconds; }
	int32 GetActiveBombCount() const { return ActiveBombs.Num(); }
	int32 GetBombCapacity() const { return Session.GetPlayerItems().GetBombCapacity(); }
	int32 GetBombRange() const
	{
		return SelectedBombType == ENaiwaP3BombType::Standard ? 1 : Session.GetPlayerItems().GetBombRange();
	}
	bool IsPlayerSlowed() const { return bSessionActive && Session.GetCurrentTick() < PlayerSlowedUntilTick; }
	float GetPlayerSpeedMultiplier() const
	{
		return Session.GetPlayerItems().GetSpeedMultiplier() * (IsPlayerSlowed() ? 0.55f : 1.0f);
	}
	ENaiwaP3BombType GetSelectedBombType() const { return SelectedBombType; }

private:
	static constexpr float FixedStepSeconds = 0.1f;

	void AdvanceFixedStep();
	void AdvancePlayerMovement(float DeltaTime);
	void ResolveDueBombs();
	void ResolveBombAtIndex(int32 BombIndex);
	void ExpireFlames();
	bool SpawnBombAt(const FIntPoint& Cell, int32 FuseTick,
		ENaiwaP3BombType Type, int32 Range, bool bIgnoreCapacity = false,
		int32 OwnerEnemyIndex = INDEX_NONE,
		const FIntPoint& VisualStartCell = FIntPoint(INDEX_NONE, INDEX_NONE));
	TArray<FIntPoint> CalculateBlastCells(const FNaiwaP3M2BombForecastInput& Bomb);
	bool IsPlayerDestinationOpen(const FIntPoint& Cell) const;
	bool IsEnemyDestinationOpen(int32 EnemyIndex, const FIntPoint& Cell) const;
	void ProcessPlayerLanding(const FIntPoint& Cell, FString& OutMessage);
	void CollectEnemyItemAt(int32 EnemyIndex, const FIntPoint& Cell);
	void DamagePlayer(bool bCanUseShield, const TCHAR* SourceLabel);
	void SetTransientEvent(const FString& Message, int32 DurationTicks = 20);
	void TriggerExplosionFeedback(const FIntPoint& BombCell);
	void SyncMapTile(const FIntPoint& Cell);
	void UpdateFogVisibility();
	void UpdatePresentation(float DeltaTime);
	FVector GridToWorld(const FIntPoint& Cell, float Z = 0.0f) const;

	FNaiwaP3M2LevelSession Session;
	TArray<FNaiwaP3M2BombForecastInput> ActiveBombs;
	TMap<uint64, TWeakObjectPtr<ANaiwaP3BombActor>> BombVisuals;
	TArray<FNaiwaP3M2HazardForecast> HazardForecast;
	TArray<FIntPoint> EnemyCells;
	TArray<FNaiwaP3M2EnemyIntent> EnemyIntents;
	TArray<int32> EnemyBombRequests;
	TArray<int32> EnemyRangedAttackRequests;
	TArray<uint8> EnemyAlive;
	TArray<FNaiwaP3M2EnemyLootState> EnemyLootStates;
	TArray<FIntPoint> MechanicDestroyedBombCells;
	TMap<uint64, int32> BombNextConveyorTick;
	TMap<FIntPoint, int32> FlameExpiryTick;
	TWeakObjectPtr<ANaiwaP3M1MapActor> MapActor;
	TWeakObjectPtr<ANaiwaP3M2WorldPresentation> WorldPresentation;
	TWeakObjectPtr<ANaiwaP3PlayerPawn> PlayerPawn;
	TArray<TWeakObjectPtr<ANaiwaP3M1EnemyActor>> EnemyActors;
	FIntPoint PlayerCell = FIntPoint::ZeroValue;
	FIntPoint PlayerDirection = FIntPoint::ZeroValue;
	float FixedAccumulator = 0.0f;
	float PlayerMoveProgress = 0.0f;
	float PlayerCellSeconds = 0.25f;
	FVector PlayerVisualFromWorld = FVector::ZeroVector;
	FVector PlayerVisualToWorld = FVector::ZeroVector;
	float PlayerVisualProgress = 1.0f;
	float PlayerVisualDuration = 0.25f;
	TArray<FIntPoint> EnemyVisualFromCells;
	TArray<FIntPoint> EnemyVisualToCells;
	TArray<float> EnemyVisualProgress;
	TArray<float> EnemyVisualDuration;
	TArray<int32> RageBombNextTicks;
	TArray<FNaiwaP3M2SlowSplashState> SlowSplashes;
	int32 PlayerHitCount = 0;
	int32 PlayerLives = 3;
	int32 PlayerInvulnerableUntilTick = 0;
	int32 PlayerSlowedUntilTick = 0;
	int32 ExplosionFlashUntilTick = 0;
	int32 DamageFeedbackUntilTick = 0;
	int32 TransientEventUntilTick = 0;
	float ExplosionShakeStartSeconds = 0.0f;
	float ExplosionShakeEndSeconds = 0.0f;
	float ExplosionShakeDurationSeconds = 0.24f;
	float ExplosionShakeAmplitude = 0.0f;
	FString DamageFeedbackMessage;
	FString TransientEventMessage;
	bool bLastDamageShielded = false;
	int32 RageStartTick = -1;
	int32 BombConveyorIntervalTicks = 2;
	int32 BombFuseTicks = 25;
	int32 BombPlacementIntervalTicks = 4;
	int32 NextPlayerBombTick = 0;
	int32 ChainDelayTicks = 2;
	int32 FlameDurationTicks = 3;
	uint64 NextBombOrder = 1;
	ENaiwaP3BombType SelectedBombType = ENaiwaP3BombType::Standard;
	ENaiwaP3M2RunState RunState = ENaiwaP3M2RunState::Playing;
	bool bRageEscape = false;
	bool bSessionActive = false;
};
