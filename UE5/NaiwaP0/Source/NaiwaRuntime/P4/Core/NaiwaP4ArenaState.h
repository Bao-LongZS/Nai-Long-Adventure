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
		Entities.Reset();
		PendingDamage.Reset();
		Events.Reset();
	}
};
