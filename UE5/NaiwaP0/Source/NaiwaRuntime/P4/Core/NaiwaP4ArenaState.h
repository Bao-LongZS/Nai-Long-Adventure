#pragma once

#include "CoreMinimal.h"
#include "NaiwaP4Types.h"

/** Mutable deterministic state. Presentation actors and UObjects stay outside this struct. */
struct FNaiwaP4ArenaState
{
	static constexpr uint32 ZeroSeedFallback = 0x6D2B79F5u;

	int64 CurrentTick = 0;
	uint32 MasterSeed = 0;
	uint32 RngState = ZeroSeedFallback;
	int32 NextEntityId = 1;
	/** Current 1-based wave; stays on the final wave when the run is complete. */
	int32 CurrentWaveIndex = 0;
	int32 TotalWaveCount = 0;
	int32 WaveSpawnEntryIndex = 0;
	int32 WaveSpawnedInEntry = 0;
	int32 WaveSpawnTicksRemaining = 0;
	int32 TotalWaveEnemyCount = 0;
	int32 DefeatedWaveEnemyCount = 0;
	bool bWaveSpawningComplete = false;
	bool bWaveRunComplete = false;
	bool bBossSpawned = false;
	bool bBossDefeated = false;
	ENaiwaP4RunResult RunResult = ENaiwaP4RunResult::InProgress;
	int32 PlayerLevel = 1;
	int32 PlayerExperience = 0;
	int32 PlayerExperienceForNextLevel = 4;
	int32 PendingUpgradeChoices = 0;
	int32 UpgradeRerollsRemaining = 0;
	int32 UpgradeOfferRevision = 0;
	TArray<FName> CurrentUpgradeOfferCardIds;
	TArray<FName> AcquiredUpgradeCardIds;
	TArray<FNaiwaP4EntityState> Entities;
	TArray<FNaiwaP4DamageEvent> PendingDamage;
	TArray<FNaiwaP4Event> Events;

	/** Version-1 xorshift32 stream; zero is mapped because it is a locked state. */
	uint32 NextRandomUInt32()
	{
		// Keep default-constructed or externally restored zero states out of the
		// xorshift32 lock-up state, even if Reset() was not called first.
		uint32 Value = RngState != 0u ? RngState : ZeroSeedFallback;
		Value ^= Value << 13;
		Value ^= Value >> 17;
		Value ^= Value << 5;
		RngState = Value;
		return Value;
	}

	void Reset(const uint32 InMasterSeed)
	{
		CurrentTick = 0;
		MasterSeed = InMasterSeed;
		RngState = InMasterSeed != 0u ? InMasterSeed : ZeroSeedFallback;
		NextEntityId = 1;
		CurrentWaveIndex = 0;
		TotalWaveCount = 0;
		WaveSpawnEntryIndex = 0;
		WaveSpawnedInEntry = 0;
		WaveSpawnTicksRemaining = 0;
		TotalWaveEnemyCount = 0;
		DefeatedWaveEnemyCount = 0;
		bWaveSpawningComplete = false;
		bWaveRunComplete = false;
		bBossSpawned = false;
		bBossDefeated = false;
		RunResult = ENaiwaP4RunResult::InProgress;
		PlayerLevel = 1;
		PlayerExperience = 0;
		PlayerExperienceForNextLevel = 4;
		PendingUpgradeChoices = 0;
		UpgradeRerollsRemaining = 0;
		UpgradeOfferRevision = 0;
		CurrentUpgradeOfferCardIds.Reset();
		AcquiredUpgradeCardIds.Reset();
		Entities.Reset();
		PendingDamage.Reset();
		Events.Reset();
	}
};
