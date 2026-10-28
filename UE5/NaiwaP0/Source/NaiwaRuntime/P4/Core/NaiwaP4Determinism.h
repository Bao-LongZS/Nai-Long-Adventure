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

	static FString FormatHash(const uint64 Hash);
};
