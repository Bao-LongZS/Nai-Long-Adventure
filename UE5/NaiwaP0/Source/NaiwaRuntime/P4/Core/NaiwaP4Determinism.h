#pragma once

#include "CoreMinimal.h"

struct FNaiwaP4ArenaState;

/** Versioned deterministic helpers for replay checkpoints. */
struct NAIWARUNTIME_API FNaiwaP4Determinism
{
	/**
	 * Hashes the authoritative state fields currently present in FNaiwaP4ArenaState.
	 * Entity rows are sorted by EntityId before hashing; IDs must be unique.
	 * Presentation-only ActionCue is excluded; queued simulation Events are included.
	 */
	static uint64 ComputeSimulationStateHashV1(const FNaiwaP4ArenaState& State);
	/** Hash version 2 includes deterministic combat, projectile, and training-enemy state. */
	static uint64 ComputeSimulationStateHashV2(const FNaiwaP4ArenaState& State);
	/** Hash version 3 adds enemy archetype and wave director state. */
	static uint64 ComputeSimulationStateHashV3(const FNaiwaP4ArenaState& State);
	/** Hash version 4 adds XP progression, acquired upgrades, and upgrade-driven combat state. */
	static uint64 ComputeSimulationStateHashV4(const FNaiwaP4ArenaState& State);
	/** Hash version 5 adds random offers, reroll state, and auto-fire/melee combat state. */
	static uint64 ComputeSimulationStateHashV5(const FNaiwaP4ArenaState& State);
	/** Hash version 6 adds boss phase/attack state and terminal run outcome. */
	static uint64 ComputeSimulationStateHashV6(const FNaiwaP4ArenaState& State);
	/** Hash version 7 adds buffered actions and in-progress boss charge state. */
	static uint64 ComputeSimulationStateHashV7(const FNaiwaP4ArenaState& State);

	static FString FormatHash(const uint64 Hash);
	static FString FormatHashV2(const uint64 Hash);
	static FString FormatHashV3(const uint64 Hash);
	static FString FormatHashV4(const uint64 Hash);
	static FString FormatHashV5(const uint64 Hash);
	static FString FormatHashV6(const uint64 Hash);
	static FString FormatHashV7(const uint64 Hash);
};
